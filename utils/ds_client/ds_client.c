#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <memory.h>
#include <errno.h>
#include <malloc.h>

#include <crtlib/include/crtlib.h>
#include <utils/ucrt/include/ucrt.h>

int main(int argc, const char *argv[])
{
	int err = DS_E_BUF_SMALL;

	printf("This is ds client\n");

	/* 
	 * set CLOG log path : NULL and log name ds_client.log
	 * NULL means use current working dir as path
	 */
	ucrt_log_set_path(NULL, "ds_client.log");
	/* set CLOG log level */
	ucrt_log_set_level(CL_DBG);

	CLOG(CL_INF, "Hello from ds client");
	/* translate error code to string description */
	CLOG(CL_INF, "err %x - %s", err, ds_error(err));
	/* run sha256 test from crtlib */
	__sha256_test();

	return 0;
}
