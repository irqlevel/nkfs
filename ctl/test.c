#include "ctl.h"
#include "test.h"
#include <stdlib.h>

#define PAGE_SIZE 4096

struct ds_obj {
	struct ds_obj_id 	id;
	u32			len;
	void			*body;
};

/*
static void *pmalloc(u32 size)
{
	void *buf = NULL;
	int err;
	err = posix_memalign(&buf, PAGE_SIZE, size);
	if (err)
		return NULL;

	return buf;
}
*/

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
		printf("gen obj %u %p\n", i, objs[i]);
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

int ds_obj_test(u32 num_objs, u32 min_bytes, u32 max_bytes)
{
	int err;
	struct ds_obj **objs = NULL;
	u32 i;

	printf("obj_test num objs %d\n", num_objs);

	err = __obj_arr_gen(&objs, num_objs, min_bytes, max_bytes);
	if (err) {
		printf("cant alloc objs\n");
		goto out;
	}

	for (i = 0; i < num_objs; i++) {
		err = ds_create_obj(&objs[i]->id);
		printf("create obj %u, err %d\n", i, err);
		if (err) {
			goto cleanup;
		}
	}

	for (i = 0; i < num_objs; i++) {
		err = ds_put_obj(&objs[i]->id, 0,
			objs[i]->body, objs[i]->len);
		printf("put obj %u, err %d\n", i, err);
		if (err) {
			goto cleanup;
		}
	}

	for (i = 0; i < num_objs; i++) {
		void *result;
		result = malloc(objs[i]->len);
		if (!result) {
			printf("cant alloc result buf\n");
			err = -ENOMEM;
			goto cleanup;
		}

		err = ds_get_obj(&objs[i]->id, 0, result, objs[i]->len);
		printf("get obj %u, err %d\n", i, err);
		if (err) {
			free(result);
			goto cleanup;
		}

		if (0 != memcmp(objs[i]->body, result, objs[i]->len)) {
			char *rhex, *bhex;
			bhex = bytes_hex(objs[i]->body, objs[i]->len);
			rhex = bytes_hex(result, objs[i]->len);
			printf("read invalid buf of obj %u\n", i);
			printf("b %s\n", bhex);
			printf("r %s\n", rhex);
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

	err = 0;

cleanup:
	__obj_arr_free(objs, num_objs);
out:
	printf("completed err %d\n", err);
	return err;
}
