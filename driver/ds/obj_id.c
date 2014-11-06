#include <ds_priv.h>
#define __SUBCOMPONENT__ "ds-objid"

static char char_to_hex(char c)
{
	if (c < 10)
		return '0' + c;
	else
		return 'a' + c - 10;
}

static int char_to_hex_buf(char *src, int src_count, char *hex, int hex_count)
{
	int i;

	if (hex_count < 2*src_count)
		return -EINVAL;

	for (i = 0; i < src_count; i++) {
		*hex++ = char_to_hex((*src >> 4) & 0xF);
		*hex++ = char_to_hex(*src++ & 0xF);
	}
	return 0;
}

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
