#include <crtlib/include/crtlib.h>

asmlinkage char *ds_obj_id_to_str(struct ds_obj_id *id)
{
	return char_buf_to_hex_str((char *)id, sizeof(*id));
}

asmlinkage struct ds_obj_id *ds_obj_id_gen(void)
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
