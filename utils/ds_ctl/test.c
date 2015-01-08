#include "ctl.h"
#include "test.h"

void __test_free_kvs(struct ds_obj_id **keys,
	u64 *values, int num_keys)
{
	int i;

	if (values) {
		crt_free(values);
	}

	if (keys) {
		for (i = 0; i < num_keys; i++)
			if (keys[i])
				crt_free(keys[i]);
		crt_free(keys);
	}
}

int __test_gen_kvs(struct ds_obj_id ***pkeys,
	u64 **pvalues, int num_keys)
{
	struct ds_obj_id **keys = NULL;
	u64 *values = NULL;
	int i, err;

	*pkeys = NULL;
	*pvalues = NULL;

	keys = crt_malloc(num_keys*sizeof(struct ds_obj_id *));
	if (!keys) {
		CLOG(CL_ERR, "cant malloc keys");
		err = -ENOMEM;
		goto cleanup;
	}
	memset(keys, 0, num_keys*sizeof(struct ds_obj_id *));

	values = crt_malloc(num_keys*sizeof(u64));
	if (!values) {
		CLOG(CL_ERR,"cant malloc values");
		err = -ENOMEM;
		goto cleanup;
	}
	memset(values, 0, num_keys*sizeof(u64));

	for (i = 0; i < num_keys; i++) {
		keys[i] = ds_obj_id_create();
		if (!keys[i]) {
			CLOG(CL_ERR, "cant gen key[%d]", i);
			err = -ENOMEM;
			goto cleanup;
		}
		values[i] = get_random_u64();
	}

	*pkeys = keys;
	*pvalues = values;
	return 0;
cleanup:
	__test_free_kvs(keys, values, num_keys);
	return err;
}

static int ds_sb_obj_test(struct ds_obj_id *sb_id, int num_keys)
{
	int err;
	struct ds_obj_id **keys = NULL;
	u64 *values = NULL;
	int i;

	err = __test_gen_kvs(&keys, &values, num_keys);
	if (err) {
		CLOG(CL_ERR, "cant gen keys/values");
		goto out;
	}

	for (i = 0; i < num_keys; i++) {
		err = ds_obj_insert(sb_id, keys[i], values[i], 0);
		if (err) {
			CLOG(CL_ERR, "cant insert key %d err %d", i, err);
			goto cleanup;
		}
	}
	
	for (i = 0; i < num_keys; i++) {
		u64 val;
		err = ds_obj_find(sb_id, keys[i], &val);
		if (err) {
			CLOG(CL_ERR, "cant find key %d err %d", i, err);
			goto cleanup;
		}

		if (val != values[i]) {
			CLOG(CL_ERR, "key %d found invalid val %llu vs %llu",
				i, val, values[i]);
			err = -EINVAL;
			goto cleanup;
		}
	}

	for (i = 0; i < num_keys; i++) {
		err = ds_obj_delete(sb_id, keys[i]);
		if (err) {
			CLOG(CL_ERR, "cant delete key %d err %d", i, err);
			goto cleanup;
		}
	}

	err = 0;
cleanup:
	__test_free_kvs(keys, values, num_keys);
out:
	return err;
}

int ds_dev_obj_test(const char *dev_name)
{
	struct ds_obj_id sb_id;
	int err;
	
	err = ds_dev_query(dev_name, &sb_id);
	if (err)
		goto out;
	
	err = ds_sb_obj_test(&sb_id, 1000);
	if (err) {
		printf("obj_test ERR %d\n", err);
	}
out:
	return err;
}
