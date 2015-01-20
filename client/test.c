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
#include <unistd.h>
#include <getopt.h>
#include <crt/include/crt.h>
#include <include/ds_client.h> /* client lib */

#include "test.h"

struct ds_obj {
	struct ds_obj_id 	id;
	u32			len;
	void			*body;
};

static struct ds_obj *__obj_gen(u32 body_bytes)
{
	struct ds_obj *obj;
	int err;

	if (body_bytes == 0)
		return NULL;

	obj = malloc(sizeof(struct ds_obj));
	if (!obj)
		return NULL;

	memset(obj, 0, sizeof(*obj));
	obj->len = body_bytes;
	obj->body = malloc(obj->len);
	if (!obj->body) {
		free(obj);
		return NULL;
	}

	err = crt_random_buf(obj->body, obj->len);
	if (err) {
		free(obj->body);
		free(obj);
		return NULL;
	}

	return obj;
}

static void __obj_free(struct ds_obj *obj)
{
	free(obj->body);
	free(obj);
}

static int __obj_arr_gen(struct ds_obj ***pobjs, u32 num_objs, u32 min_bytes,
	u32 max_bytes)
{
	struct ds_obj **objs;
	u32 i, j;
	int err;

	objs = malloc(num_objs*sizeof(struct ds_obj *));
	if (!objs)
		return -ENOMEM;
	for (i = 0; i < num_objs; i++) {
		objs[i] = __obj_gen(rand_u32_min_max(min_bytes, max_bytes));
		CLOG(CL_INF, "gen obj %u %p", i, objs[i]);
		if (!objs[i]) {
			err = -ENOMEM;
			goto fail;		
		}
	}

	*pobjs = objs;
	return 0;

fail:
	for (j = 0; j < i; j++)
		__obj_free(objs[i]);
	free(objs);
	return err;		
}

static void __obj_arr_free(struct ds_obj **objs, u32 num_objs)
{
	u32 i;
	for (i = 0; i < num_objs; i++)
		__obj_free(objs[i]);
	free(objs);
}

int obj_test(char *server, int port, u32 num_objs, u32 min_bytes, u32 max_bytes)
{
	int err;
	struct ds_obj **objs = NULL;
	struct ds_con con;
	u32 i;

	CLOG(CL_INF, "obj_test num objs %d", num_objs);

	err = ds_connect(&con, server, port);
	if (err) {
		CLOG(CL_ERR, "connect %s:%d err %d", server, port, err);
		goto out;
	}
	CLOG(CL_INF, "connected");
	err = ds_echo(&con);
	if (err) {
		CLOG(CL_ERR, "echo failed err %d", err);
		goto discon;
	}

	err = __obj_arr_gen(&objs, num_objs, min_bytes, max_bytes);
	if (err) {
		CLOG(CL_ERR, "cant alloc objs");
		goto discon;
	}

	for (i = 0; i < num_objs; i++) {
		err = ds_create_object(&con, &objs[i]->id);
		CLOG(CL_INF, "create obj %u, err %d", i, err);
		if (err) {
			goto cleanup;
		}
	}

	for (i = 0; i < num_objs; i++) {
		err = ds_put_object(&con, &objs[i]->id, 0,
			objs[i]->body, objs[i]->len);
		CLOG(CL_INF, "put obj %u, err %d", i, err);
		if (err) {
			goto cleanup;
		}
	}

	for (i = 0; i < num_objs; i++) {
		void *result;
		u32 read;

		result = malloc(objs[i]->len);
		if (!result) {
			CLOG(CL_ERR, "cant alloc result buf");
			err = -ENOMEM;
			goto cleanup;
		}

		err = ds_get_object(&con, &objs[i]->id, 0, result,
			objs[i]->len, &read);
		CLOG(CL_INF, "get obj %u, err %d", i, err);
		if (err) {
			free(result);
			goto cleanup;
		}
		if (read != objs[i]->len) {
			CLOG(CL_ERR, "obj %u read %d vs olen %d",
				i, read, objs[i]->len);
			err = -EIO;
			free(result);
			goto cleanup;
		}
		if (0 != memcmp(objs[i]->body, result, objs[i]->len)) {
			char *rhex, *bhex;
			bhex = bytes_hex(objs[i]->body, objs[i]->len);
			rhex = bytes_hex(result, objs[i]->len);
			CLOG(CL_ERR, "read invalid buf of obj %u", i);
			CLOG(CL_ERR, "b %s", bhex);
			CLOG(CL_ERR, "r %s", rhex);
			if (bhex)
				crt_free(bhex);
			if (rhex)
				crt_free(rhex);
			err = -EINVAL;
			free(result);
			goto cleanup;
		}
		free(result);
	}

	for (i = 0; i < num_objs; i++) {
		err = ds_delete_object(&con, &objs[i]->id);
		if (err) {
			CLOG(CL_ERR, "del obj %u err %d", i, err);
			goto cleanup;
		}
	}

	err = 0;

cleanup:
	__obj_arr_free(objs, num_objs);
discon:
	ds_close(&con);
out:
	CLOG(CL_INF, "completed err %d", err);
	return err;
}

int echo_test(void)
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


