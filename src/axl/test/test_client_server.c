#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "axl.h"
#include "kvtree.h"
#include "kvtree_util.h"

#define AXLCS_SUCCESS					0
#define AXLCS_CLIENT_INVALID			1
#define AXLCS_SERVICE_CREATION_FAILURE	1000
#define AXLCS_SERVICE_KILLED			2000
#define AXLCS_SERVICE_FAIL 				3000

extern int axl_socket_server_run(int port);

int run_service(int port)
{
	fprintf(stderr, "%s, %i...\n", __FILE__, __LINE__);
	fprintf(stdout, "Service Starting!\n");
	int rval = axl_socket_server_run(port);
	fprintf(stdout, "Service Ending!\n");
	return rval;
}

int set_global_options (void) {
	int rc = 0;
	kvtree* axl_config_values = NULL;
	if (!(axl_config_values = kvtree_new())) {
		return -1;
	}

	/* check AXL configuration settings */
	rc = kvtree_util_set_bytecount(axl_config_values,
								   AXL_KEY_CONFIG_FILE_BUF_SIZE,
								   4200000);
	if (rc != KVTREE_SUCCESS) {
		printf("kvtree_util_set_bytecount failed (error %d)\n", rc);
		goto done;
	}

	rc = kvtree_util_set_int(axl_config_values, AXL_KEY_CONFIG_DEBUG,
							 10);
	if (rc != KVTREE_SUCCESS) {
		printf("kvtree_util_set_int failed (error %d)\n", rc);
		goto done;
	}

	printf("Configuring AXL (first set of options)...\n");
	if (AXL_Config(axl_config_values) == NULL) {
		printf("AXL_Config() failed\n");
		rc = -1;
		goto done;
	}

done:
	kvtree_delete(&axl_config_values);
	return rc;
}

int transfer_file (void) {
	int axl_id;
	const char *source_path;
	char dest_path[4096];
	if ((source_path = getenv("AXL_SOCKET_TRANSFER")) == NULL
		|| snprintf(dest_path, sizeof(dest_path), "%s_dest", source_path) >= sizeof(dest_path) - 1) {
		printf("ERROR: fetching source and dest file paths");
		return -1;
	}

	if ((axl_id = AXL_Create (AXL_XFER_DEFAULT, "socket transfer demo", NULL)) < 0) {
		printf("AXL_Create returned %d", axl_id);
		return -1;
	}
	if (AXL_Add(axl_id, source_path, dest_path) < 0) {
		printf ("ERROR: AXL_Add\n");
		goto error;
	}
	if (AXL_Dispatch(axl_id) != AXL_SUCCESS){
		printf ("ERROR: AXL_Dispatch\n");
		goto error;
	}
	AXL_Free(axl_id);
	return 0;

error:
	AXL_Stop();
	return -1;
}

int run_client()
{
	int rval;

	if ((rval = AXL_Init()) != AXL_SUCCESS) {
		fprintf(stderr, "Call to AXL_Init failed with code: %d\n", rval);
		return rval;
	}
	if ((rval = set_global_options()) < 0
		|| (rval = transfer_file()) < 0) {
		return rval;
	}
	if ((rval = AXL_Finalize()) != AXL_SUCCESS) {
		fprintf(stderr, "Call to AXL_Init failed with code: %d\n", rval);
	}

	fprintf(stderr, "Done with rval %i...\n", rval);
	return rval;
}

int main(int argc, char **argv)
{
	fprintf(stderr, "Just testing stderr...\n");
	int port = 0;
	if (argc != 3) {
		fprintf(stderr, "Command count (%d) incorrect:\nUsage: test_client_server --<client|server> port\n", argc);
		return AXLCS_CLIENT_INVALID;
	}

	if (strcmp("--server", argv[1]) == 0) {
		if ((port = atoi(argv[2])) <= 0){
			fprintf(stderr, "Port number (%s) incorrect:\nUsage: test_client_server --<client|server> port\n", argv[2]);
			return AXLCS_CLIENT_INVALID;
		}
		fprintf(stderr, "%s, %i...\n", __FILE__, __LINE__);
		return run_service(port);
	}
	else if (strcmp("--client", argv[1]) == 0) {
		return run_client();
	}

	fprintf(stderr, "Unknown Argument (%s) incorrect:\nUsage: test_client_server --<client|server> port\n", argv[1]);
	return AXLCS_CLIENT_INVALID;
}
