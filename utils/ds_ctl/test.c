#include "ctl.h"
#include "test.h"

static int ds_sb_obj_test(struct ds_obj_id *sb_id)
{
	int err;
	struct ds_obj_id *obj_id;
	u64 value, val_find;

	obj_id = ds_obj_id_create();
	if (!obj_id) {
		err = -ENOMEM;
		goto out;
	}
	value = get_random_u64();

	err = ds_obj_insert(sb_id, obj_id, value, 0);
	if (err) {
		goto cleanup;
	} 

	err = ds_obj_find(sb_id, obj_id, &val_find);
	if (err) {
		goto cleanup;
	} 

	if (value != val_find) {
		goto cleanup;	
	}

	err = ds_obj_delete(sb_id, obj_id);
	if (err) {
		goto cleanup;
	}

cleanup:
	crt_free(obj_id);
out:
	return err;
}

int ds_dev_obj_test(const char *dev_name)
{
	struct ds_obj_id sb_id;
	int err;
	int i;
	
	err = ds_dev_query(dev_name, &sb_id);
	if (err)
		goto out;
	
	for (i = 0; i < 100000; i++) {
		err = ds_sb_obj_test(&sb_id);
		if (err) {
			printf("obj_test[%d] err %d\n", i, err);
		}
		if (i % 500 == 0)
			printf("obj_test[%d]\n", i);
	}
out:
	return err;
}
