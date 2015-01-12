#include "ctl.h"
#include "test.h"
#include <stdlib.h>

#define PAGE_SIZE 4096

struct ds_obj {
	struct ds_obj_id 	id;
	u32			len;
	void			*body;
};

void *pmalloc(u32 size)
{
	void *buf = NULL;
	int err;
	err = posix_memalign(&buf, PAGE_SIZE, size);
	if (err)
		return NULL;

	return buf;
}

struct ds_obj *__obj_gen(u32 nr_pages)
{
	struct ds_obj *obj;
	int err;

	obj = malloc(sizeof(struct ds_obj));
	if (!obj)
		return NULL;

	err = ds_obj_id_gen(&obj->id);
	if (err) {
		free(obj);
		return NULL;
	}

	obj->len = nr_pages*PAGE_SIZE;
	obj->body = pmalloc(obj->len);
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

void __obj_free(struct ds_obj *obj)
{
	free(obj->body);
	free(obj);
}

int __obj_arr_gen(struct ds_obj ***pobjs, u32 num_objs, u32 min_pages,
	u32 max_pages)
{
	struct ds_obj **objs;
	u32 i, j;
	int err;

	objs = malloc(num_objs*sizeof(struct ds_obj *));
	if (!objs)
		return -ENOMEM;
	for (i = 0; i < num_objs; i++) {
		objs[i] = __obj_gen(rand_u32_min_max(min_pages, max_pages));
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

void __obj_arr_free(struct ds_obj **objs, u32 num_objs)
{
	u32 i;
	for (i = 0; i < num_objs; i++)
		__obj_free(objs[i]);
	free(objs);
}

int ds_sb_obj_test(const char *dev_name, u32 num_objs, u32 min_pages,
		u32 max_pages)
{
	int err;
	struct ds_obj_id sb_id;
	struct ds_obj **objs = NULL;
	u32 i;

	printf("dev %s num objs %d\n", dev_name, num_objs);

	err = __obj_arr_gen(&objs, num_objs, min_pages, max_pages);
	if (err) {
		printf("cant alloc objs\n");
		goto out;
	}

	err = ds_dev_query(dev_name, &sb_id);
	if (err) {
		printf("cant query sb_id for dev %s\n",
			dev_name);
		goto cleanup;
	}

	for (i = 0; i < num_objs; i++) {
		err = ds_obj_write(&sb_id, &objs[i]->id, 0,
			objs[i]->body, objs[i]->len);
		printf("wrote obj %u, err %d\n", i, err);
		if (err) {
			printf("cant write obj %u err %d\n", i, err);
			goto cleanup;
		}
	}

	err = ds_dev_rem(dev_name);
	if (err) {
		printf("cant rem dev %s err %d\n", dev_name, err);
		goto cleanup;
	}
	printf("dev %s removed\n", dev_name);

	err = ds_dev_add(dev_name, 0);
	if (err) {
		printf("cant add dev %s err %d\n", dev_name, err);
		goto cleanup;
	}

	printf("dev %s added\n", dev_name);
	for (i = 0; i < num_objs; i++) {
		void *result;
		result = pmalloc(objs[i]->len);
		if (!result) {
			printf("cant alloc result buf\n");
			err = -ENOMEM;
			goto cleanup;
		}

		err = ds_obj_read(&sb_id, &objs[i]->id, 0, result, objs[i]->len);
		if (err) {
			printf("cant read obj %u err %d\n", i, err);
			free(result);
			goto cleanup;
		}

		printf("read obj %u, err %d\n", i, err);
		if (0 != memcmp(objs[i]->body, result, objs[i]->len)) {
			printf("read invalid buf of obj %u\n", i);
			err = -EINVAL;
			free(result);
			goto cleanup;
		}
		free(result);
	}

	err = ds_obj_tree_check(&sb_id);
	if (err) {
		printf("obj tree check dev %s err %d\n", dev_name, err);
		goto cleanup;
	}
	printf("dev %s obj tree correct\n", dev_name);

	err = 0;

cleanup:
	__obj_arr_free(objs, num_objs);
out:
	printf("completed err %d\n", err);
	return err;
}
