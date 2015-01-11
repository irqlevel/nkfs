#include "ctl.h"
#include "test.h"
#include <stdlib.h>

#define PAGE_SIZE 4096

int ds_sb_obj_test(const char *dev_name, int num_objs)
{
	int err;
	struct ds_obj_id sb_id;
	void *buf = NULL, *result = NULL;
	struct ds_obj_id *obj_id = NULL;
	u32 osize = 10*PAGE_SIZE;

	CLOG(CL_INF, "dev_name %s num_objs %d",
		dev_name, num_objs);

	printf("dev %s num objs %d\n", dev_name, num_objs);

	obj_id = ds_obj_id_create();
	if (!obj_id) {
		err = -ENOMEM;
		goto cleanup;
	}

	err = posix_memalign(&buf, PAGE_SIZE, osize);
	if (err)
		goto cleanup;

	err = crt_random_buf(buf, osize);
	if (err)
		goto cleanup;
	
	err = posix_memalign(&result, PAGE_SIZE, osize);
	if (err)
		goto cleanup;

	memset(result, 0, osize);
	if (0 == memcmp(result, buf, osize)) {
		err = -EINVAL;
		goto cleanup;
	}

	err = ds_dev_query(dev_name, &sb_id);
	if (err) {
		CLOG(CL_ERR, "cant query sb_id for dev %d",
			dev_name);
		goto out;
	}

	err = ds_obj_write(&sb_id, obj_id, 0, buf, osize);
	if (err) {
		printf("cant write obj err %d\n", err);
		goto cleanup;
	}

	err = ds_dev_rem(dev_name);
	if (err) {
		CLOG(CL_ERR, "cant rem dev %s err %d", dev_name, err);
		goto cleanup;
	}
	printf("dev %s removed\n", dev_name);

	err = ds_dev_add(dev_name, 0);
	if (err) {
		CLOG(CL_ERR, "cant add dev %s err %d", dev_name, err);
		goto cleanup;
	}
	printf("dev %s added\n", dev_name);

	err = ds_obj_read(&sb_id, obj_id, 0, result, osize);
	if (err) {
		printf("cant read obj err %d\n", err);
		goto cleanup;
	}

	if (0 != memcmp(buf, result, osize)) {
		printf("read invalid buf\n");
		err = -EINVAL;
		goto cleanup;
	}

	err = ds_obj_tree_check(&sb_id);
	if (err) {
		CLOG(CL_ERR, "obj tree check dev %s err %d", dev_name, err);
		goto cleanup;
	}
	printf("dev %s obj tree correct\n", dev_name);

	err = 0;

cleanup:
	if (result)
		free(result);
	if (buf)
		free(buf);
	if (obj_id)
		crt_free(obj_id);
out:
	printf("completed err %d\n", err);
	CLOG(CL_INF, "err %d", err);
	return err;
}
