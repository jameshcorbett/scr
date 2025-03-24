#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "axl.h"

#define AXLCS_SUCCESS					0
#define AXLCS_CLIENT_INVALID			1
#define AXLCS_SERVICE_CREATION_FAILURE	1000
#define AXLCS_SERVICE_KILLED			2000
#define AXLCS_SERVICE_FAIL 				3000

extern int axl_socket_server_run(int port);

int run_service(int port)
{
	fprintf(stdout, "Service Started!\n");
	int rval = axl_socket_server_run(port);
	fprintf(stdout, "Service Ending!\n");
	return rval;
}

int run_client()
{
	int rval;

	if ((rval = AXL_Init()) != AXL_SUCCESS) {
		fprintf(stderr, "Call to AXL_Init failed with code: %d\n", rval);
	} else {
		if ((rval = AXL_Finalize()) != AXL_SUCCESS) {
			fprintf(stderr, "Call to AXL_Init failed with code: %d\n", rval);
		}
	}

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
		return run_service(port);
	}
	else if (strcmp("--client", argv[1]) == 0) {
		return run_client();
	}

	fprintf(stderr, "Unknown Argument (%s) incorrect:\nUsage: test_client_server --<client|server> port\n", argv[1]);
	return AXLCS_CLIENT_INVALID;
}
