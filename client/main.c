#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <memory.h>
#include <errno.h>
#include <malloc.h>
#include <crt/include/crt.h>
#include <include/ds_client.h> /* client lib */

static void prepare_logging()
{
	/* 
	 * set CLOG log path : NULL and log name ds_client.log
	 * NULL means use current working dir as path
	*/
	crt_log_set_path(NULL, "ds_client.log");
	/* set CLOG log level */
	crt_log_set_level(CL_DBG);
	crt_log_enable_printf(1);
}

int obj_test()
{
#define OBJ_BODY "This is object body bytes"
	struct ds_obj_id obj_id;
	int err;
	struct ds_con con;
	char obj_body[] = OBJ_BODY;
	char obj_body_ret[sizeof(OBJ_BODY)];

	err = ds_obj_id_gen(&obj_id);
	if (err) {
		CLOG(CL_ERR, "cant gen obj id err %d", err);
		goto out;
	}

	err = ds_connect(&con, "127.0.0.1", 8000);
	if (err) {
		CLOG(CL_ERR, "connect err %d", err);
		goto out;
	}

	err = ds_create_object(&con, &obj_id);
	if (err) {
		CLOG(CL_ERR, "create obj err %d", err);
		goto out;
	}

	err = ds_put_object(&con, &obj_id, 0, obj_body, sizeof(obj_body));
	if (err) {
		CLOG(CL_ERR, "ds_put_object err %d", err);
		goto discon;
	}

	err = ds_get_object(&con, &obj_id, 0, obj_body_ret, sizeof(obj_body_ret));
	if (err) {
		CLOG(CL_ERR, "ds_get_object err %d", err);
		goto discon;
	}

	if (0 != memcmp(obj_body_ret, obj_body, sizeof(obj_body))) {
		CLOG(CL_ERR, "memcmp objects err");
		err = -EINVAL;
		goto discon;
	}

	err = ds_delete_object(&con, &obj_id);
	if (err) {
		CLOG(CL_ERR, "delete obj err %d", err);
		goto out;
	}

discon:
	ds_close(&con);
out:
	return err;
}

int echo_test()
{
	int err;
	struct ds_con con;

	err = ds_connect(&con, "127.0.0.1", 8000);
	if (err) {
		CLOG(CL_ERR, "ds_connect failed err %d", err);
		goto out;
	}

	err = ds_echo(&con);
	if (err) {
		CLOG(CL_ERR, "echo failed err %d", err);
	}

	ds_close(&con);
out:
	return err;
}

int main(int argc, const char *argv[])
{
	prepare_logging();
	return obj_test();
}
