#include <ds_priv.h>
#define __SUBCOMPONENT__ "ds-objid"

char *ds_obj_id_to_str(struct ds_obj_id *id)
{
	int result_len = 2*sizeof(id->bytes) + 1;
	char *result = kmalloc(result_len, GFP_KERNEL);
	if (!result)
		return NULL;

	if (char_to_hex_buf(id->bytes, sizeof(id->bytes), result, result_len)) {
		kfree(result);
		return NULL;
	}

	result[2*DS_OBJ_ID_BYTES] = '\0';
	return result;
}

struct ds_obj_id *ds_obj_id_gen(void)
{
	struct ds_obj_id *id;
	int err;

	id = kmalloc(sizeof(struct ds_obj_id), GFP_KERNEL);
	if (!id) {
		klog(KL_ERR, "no memory");
		return NULL;
	}

	err = ds_random_buf_read(id, sizeof(*id), 0);
	if (err) {
		klog(KL_ERR, "ds_random_buf_read err %d", err);
		kfree(id);
		return NULL;
	}

	return id;
}
