#include <crtlib.h>

char *ds_obj_id_to_str(struct ds_obj_id *id)
{
	int result_len = 2*sizeof(id->bytes) + 1;
	char *result = crt_malloc(result_len);
	if (!result)
		return NULL;

	if (char_to_hex_buf(id->bytes, sizeof(id->bytes), result, result_len)) {
		crt_free(result);
		return NULL;
	}

	result[2*DS_OBJ_ID_BYTES] = '\0';
	return result;
}

struct ds_obj_id *ds_obj_id_gen(void)
{
	struct ds_obj_id *id;
	int err;

	id = crt_malloc(sizeof(struct ds_obj_id));
	if (!id) {
		return NULL;
	}

	err = crt_random_buf(id, sizeof(*id));
	if (err) {
		crt_free(id);
		return NULL;
	}

	return id;
}
