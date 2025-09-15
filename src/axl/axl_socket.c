#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include "axl_internal.h"
#include "axl_socket.h"
#include "kvtree.h"

/*
 * Flag to state whether the AXL client/server mode of operation is enabled,
 * and if so, whether the code is running as the client or the server.
 */
axl_socket_RunMode axl_service_mode = AXL_SOCKET_DISABLED;

static int axl_socket_socket = -1;

#define LOG() fprintf(stderr, "%s: in %s, line %i...\n", __func__, __FILE__, __LINE__)


/*
 * Client implementation
 */
int axl_socket_client_init(char* host, unsigned short port)
{
  struct sockaddr_in server;
  struct hostent *hostnm = gethostbyname(host);

  if (hostnm == (struct hostent *) 0) {
    AXL_ERR("Gethostbyname failed: (%s)", strerror(errno));
    return 0;
  }

  if ( (axl_socket_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    AXL_ERR("socket() failed: (%s)", strerror(errno));
    return 0;
  }

  memset(&server, 0, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_port = htons(port);
  server.sin_addr.s_addr = *((unsigned long *)hostnm->h_addr);

  if ( connect(axl_socket_socket, (struct sockaddr *)&server, sizeof(server) ) < 0) {
    AXL_ERR("connect() failed: (%s)", strerror(errno));
    close(axl_socket_socket);
    return 0;
  }

  const char *message;
  if ((message = getenv("AXL_SOCKET_MESSAGE")) == NULL) {
    return 1;
  }
  axl_socket_Request req;
  req.request = AXL_SOCKET_INFO;
  req.payload_length = strlen(message);
  if (send(axl_socket_socket, &req, sizeof(axl_socket_Request), 0) < sizeof(axl_socket_Request)
    || send(axl_socket_socket, message, req.payload_length, 0) < req.payload_length) {
    return 0;
  }

  return 1;   // success
}

/*
 * function to perform client-side request to server for AXL_Finalize()
 */
void axl_socket_client_AXL_Finalize()
{
  if (axl_socket_socket >= 0)
    close(axl_socket_socket);
}

/*
 * Send a kvtree through `axl_socket_socket`.
 */
static int axl_socket_send_kvtree(axl_socket_Request* request, const kvtree* msg)
{
  ssize_t bytecount;
  char *buf;

  request->payload_length = (ssize_t)kvtree_pack_size(msg);
  bytecount = axl_write_attempt("AXLSVC send_kvtree header",
                                  axl_socket_socket, request, sizeof(*request));

  if (bytecount != sizeof(*request)) {
    AXL_ERR("Unexpected write response to server: Expected %zu, Got %zd",
                  sizeof(*request), bytecount);
    return -1;
  }
  buf = malloc(request->payload_length);
  if ((bytecount = kvtree_pack(buf, msg)) != request->payload_length) {
    free(buf);
    AXL_ERR("Unexpected kvtree_pack: Expected %zu, Got %zd",
                  request->payload_length, bytecount);
    return -1;
  }
  bytecount = axl_write_attempt("AXLSVC send_kvtree payload",
                                  axl_socket_socket, buf, request->payload_length);

  free(buf);
  if (bytecount != request->payload_length) {
    AXL_ERR("Unexpected write response to server: Expected %zd, Got %zd",
                  request->payload_length, bytecount);
    return -1;
  }
  return 0;
}

/*
 *
 */
int axl_socket_client_send_and_receive(const kvtree* to_send, int request_type)
{
  ssize_t bytecount;
  axl_socket_Request request;
  axl_socket_Response response;

  request.request = request_type;
  if (axl_socket_send_kvtree(&request, to_send) != AXL_SUCCESS){
    AXL_ERR("axl_socket_send_kvtree");
    return -1;
  }

  bytecount = axl_read("AXLSVC Client <-- Response",
                                  axl_socket_socket, &response, sizeof(response));

  if (bytecount != sizeof(response)
      || response.response != AXL_SOCKET_SUCCESS
    ) {
    AXL_ERR("Unexpected response from server: Expected %zu, Got %d",
                  sizeof(response), bytecount);
    return -1;
  }

  return AXL_SUCCESS;
}

/*
 * function to perform client-side request to server for AXL_Config_Set
 */
int axl_socket_client_AXL_Config_Set(const kvtree* config)
{
  return axl_socket_client_send_and_receive(config, AXL_SOCKET_AXL_CONFIG_SET);
}

/*
 * function to perform client-side request to server for file transfer
 */
int axl_socket_client_AXL_Dispatch(const kvtree* file_list)
{
  return axl_socket_client_send_and_receive(file_list, AXL_SOCKET_FILE_TRANSFER);
}

/* 
 * Server Implementation
 */

static int time_to_leave = 0;

#define AXL_SOCKET_MAX_CLIENTS 16
struct axl_socket_conn_ctx {
  int sd;                         /* Connection to our socket */
  struct axl_transfer_array xfr;  /* Pointer to client-specific xfer array */
} axl_socket_conn_ctx_array[AXL_SOCKET_MAX_CLIENTS];


static kvtree *axl_socket_recv_kvtree (const char *buf, size_t bytecount) {
  kvtree* tree = kvtree_new();
  size_t unpacked = kvtree_unpack(buf, tree);
  if (unpacked <= 0 || unpacked != bytecount) {
    kvtree_delete (&tree);
    AXL_ERR("Failed to unpack kvtree, got %zu bytes, expected %zu", unpacked, bytecount);
    return NULL;
  }
  fprintf(stderr, "printing kvtree to stdout\n");
  kvtree_print (tree, 2);
  return tree;
}


static ssize_t axl_socket_request_from_client(struct axl_socket_conn_ctx *conn_ctx)
{
  ssize_t bytecount;
  axl_socket_Request req;
  axl_socket_Response response;
  char* buffer;
  kvtree *kvtree_msg;

  LOG();
  if (!conn_ctx){
    AXL_ERR("axl_socket_request_from_client: NULL conn_ctx");
    return -1;
  }
  bytecount = axl_read("AXLSVC Client Request", conn_ctx->sd, &req, sizeof(req));

  if (bytecount == 0) {
    AXL_DBG(2, "Client for socket %d closed", conn_ctx->sd);
    return bytecount;
  }

  buffer = malloc(req.payload_length);

  bytecount = axl_read("AXLSVC Request Payload", conn_ctx->sd, buffer, req.payload_length);

  if (bytecount != req.payload_length) {
    AXL_ABORT(-1, "Unexpected Payload Length: Expected %zd, Got %zd", req.payload_length, bytecount);
  }
  fprintf(stderr, "received a payload of %zd bytes\n", bytecount);
  fprintf(stderr, "Request type is %d\n", req.request);

  switch (req.request) {
    case AXL_SOCKET_AXL_CONFIG_SET:

      LOG();
      if (!(kvtree_msg = axl_socket_recv_kvtree (buffer, bytecount))) {
        AXL_ABORT(-1, "axl_socket_recv_kvtree");
      }
      kvtree_delete (&kvtree_msg);
      response.response = AXL_SOCKET_SUCCESS;
      response.payload_length = 0;
      bytecount = axl_write_attempt("AXLSVC Response to Client", conn_ctx->sd, &response, sizeof(response));
      if (bytecount != sizeof(response)) {
        AXL_ABORT(-1, "Unexpected write response to client: Expected %zu, Got %zd",
                      sizeof(response), bytecount);
      }
      break;
    case AXL_SOCKET_INFO:
      LOG();
      buffer[req.payload_length - 1] = '\0';
      fprintf(stderr, "Received %s", buffer);
      break;
    case AXL_SOCKET_FILE_TRANSFER:

      // int axl_id;
      LOG();
      fprintf(stderr, "received file transfer message\n");
      if (!(kvtree_msg = axl_socket_recv_kvtree (buffer, bytecount))) {
        AXL_ABORT(-1, "axl_socket_recv_kvtree");
      }

      int id = conn_ctx->xfr.axl_kvtrees_count;
      conn_ctx->xfr.axl_kvtrees_count++;

      conn_ctx->xfr.axl_kvtrees = realloc(conn_ctx->xfr.axl_kvtrees, sizeof(struct kvtree*) * conn_ctx->xfr.axl_kvtrees_count);
      conn_ctx->xfr.axl_kvtrees[id] = kvtree_msg;

      if (AXL_Dispatch (id) != AXL_SUCCESS
        || AXL_Wait (id) != AXL_SUCCESS) {
        AXL_ERR ("AXL_Dispatch || AXL_WAIT");
      }
      // if ((axl_id = AXL_Create (AXL_XFER_DEFAULT, "socket transfer demo", NULL)) < 0) {
      //   printf("AXL_Create returned %d", axl_id);
      //   return -1;
      // }
      // if (AXL_Dispatch(axl_id) != AXL_SUCCESS
      //   || AXL_Wait (axl_id) != AXL_SUCCESS){
      //   printf ("AXL_Dispatch || AXL_Wait");
      //   goto error;
      // }
      // AXL_Free(axl_id);
      // kvtree_delete (&kvtree_msg);
      response.response = AXL_SOCKET_SUCCESS;
      response.payload_length = 0;
      bytecount = axl_write_attempt("AXLSVC Response to Client", conn_ctx->sd, &response, sizeof(response));
      if (bytecount != sizeof(response)) {
        AXL_ABORT(-1, "Unexpected write response to client: Expected %zu, Got %zd",
                      sizeof(response), bytecount);
      }
      break;
    default:
      AXL_ABORT(-1, "AXLSVC Unknown Request Type %d", req.request);
      break;
  }

  free(buffer);
  return bytecount;
}

static void sigterm_handler(int sig, siginfo_t* info, void* ucontext)
{
  AXL_DBG(2, "SIGTERM Received");
  time_to_leave++;
}

static int use_sigterm_to_exit()
{
  struct sigaction act = {0};

  act.sa_flags = 0;
  sigemptyset(&act.sa_mask);
  act.sa_sigaction = sigterm_handler;
  if (sigaction(SIGTERM, &act, NULL) == -1) {
    perror("sigaction");
    return AXL_FAILURE;
  }

  return AXL_SUCCESS;
}

int axl_socket_server_run(int port)
{
  int server_socket;
  int opt = 1;
  struct sockaddr_in address;
  int addrlen;
  int new_socket;
  fd_set readfds;
  int activity;
  int max_sd;
  int rval = AXL_FAILURE;


  LOG();

  axl_service_mode = AXL_SOCKET_SERVER;
  memset(axl_socket_conn_ctx_array, 0, sizeof(axl_socket_conn_ctx_array));

  /*
   * Need to check whether calling AXL_Init() at this point is really appropriate
   */
  if ( (rval = AXL_Init()) != AXL_SUCCESS)
    return rval;

  LOG();
  if ((rval = use_sigterm_to_exit()) != AXL_SUCCESS)
    return rval;

  LOG();
  if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    AXL_ABORT(-1, "socket() failed: (%s)", strerror(errno));
  }

  LOG();
  if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 ) {
    AXL_ABORT(-1, "setsockopt() failed: (%s)", strerror(errno));
  }

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port);

  LOG();
  if (bind(server_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
    AXL_ABORT(-1, "bind() failed: (%s)", strerror(errno));
  }
  fprintf(stderr, "Service bound to socket!\n");

  if (listen(server_socket, AXL_SOCKET_MAX_CLIENTS) < 0) {
    AXL_ABORT(-1, "listen() failed: (%s)", strerror(errno));
  }

  addrlen = sizeof(address);

  LOG();
  while (!time_to_leave) {
    FD_ZERO(&readfds);
    FD_SET(server_socket, &readfds);
    max_sd = server_socket;

    for (int i = 0 ; i < AXL_SOCKET_MAX_CLIENTS ; i++) {
      if (axl_socket_conn_ctx_array[i].sd > 0)
        FD_SET(axl_socket_conn_ctx_array[i].sd, &readfds);

      if (axl_socket_conn_ctx_array[i].sd > max_sd)
        max_sd = axl_socket_conn_ctx_array[i].sd;
    }

    LOG();
    activity = select(max_sd + 1 , &readfds , NULL , NULL , NULL);

    if (time_to_leave)
      break;

    if (activity < 0 && errno != EINTR) {
      AXL_ABORT(-1, "select() error: (%s)", strerror(errno));
    }

    if (FD_ISSET(server_socket, &readfds)) {
      AXL_DBG(1, "Accepting new incoming connection");
      LOG();
      if ((new_socket = accept(server_socket, (struct sockaddr *)&address,
                                                (socklen_t*)&addrlen)) < 0) {
        AXL_ABORT(-1, "accept() error: (%s)", strerror(errno));
      }

      for (int i = 0; i < AXL_SOCKET_MAX_CLIENTS; i++) {
        if(axl_socket_conn_ctx_array[i].sd == 0 ){
          axl_socket_conn_ctx_array[i].sd = new_socket;
          break;
        }
      }
      LOG();
      AXL_DBG(1, "Connection established");
    }

    LOG();
    for ( int i = 0; i < AXL_SOCKET_MAX_CLIENTS; i++) {
      if (FD_ISSET(axl_socket_conn_ctx_array[i].sd , &readfds)) {
        axl_xfer_list = &axl_socket_conn_ctx_array[i].xfr;

        int socket_request_rc = -1;
        if ((socket_request_rc = axl_socket_request_from_client(&axl_socket_conn_ctx_array[i])) == 0) {
          AXL_DBG(1, "Closing server side socket(%d) to client", axl_socket_conn_ctx_array[i].sd);
          close(axl_socket_conn_ctx_array[i].sd);
          axl_socket_conn_ctx_array[i].sd = 0;
          axl_free(&axl_xfer_list->axl_kvtrees);
          axl_xfer_list->axl_kvtrees_count = 0;
          LOG();
        }
        else if (socket_request_rc < 0){
          AXL_ERR ("axl_socket_request_from_client");
        }
      }
    }
  }
  return 0;
}

