#include <crt/include/crt.h>

char *ds_obj_id_str(struct ds_obj_id *id)
{
	return bytes_hex((char *)id, sizeof(*id));
}
EXPORT_SYMBOL(ds_obj_id_str);

int ds_obj_id_gen(struct ds_obj_id *id)
{
	return crt_random_buf(id, sizeof(*id));
}
EXPORT_SYMBOL(ds_obj_id_gen);

int ds_obj_id_cmp(struct ds_obj_id *id1, struct ds_obj_id *id2)
{
	if (id1->high > id2->high)
		return 1;
	else if (id1->high < id2->high)
		return -1;
	else {
		if (id1->low > id2->low)
			return 1;
		else if (id1->low < id2->low)
			return -1;
		else
			return 0;	
	}
}

EXPORT_SYMBOL(ds_obj_id_cmp);

void ds_obj_id_copy(struct ds_obj_id *dst, struct ds_obj_id *src)
{
	crt_memcpy(dst, src, sizeof(*src));
}
EXPORT_SYMBOL(ds_obj_id_copy);

struct ds_obj_id *ds_obj_id_create(void)
{
	struct ds_obj_id *id;
	int err;

	id = crt_malloc(sizeof(struct ds_obj_id));
	if (!id) {
		return NULL;
	}

	err = ds_obj_id_gen(id);
	if (err) {
		crt_free(id);
		return NULL;
	}

	return id;
}
EXPORT_SYMBOL(ds_obj_id_create);


struct ds_obj_id *ds_obj_id_by_str(char *s)
{
	struct ds_obj_id *id;
	
	id = crt_malloc(sizeof(struct ds_obj_id));
	if (!id) {
		CLOG(CL_ERR, "no mem");
		return NULL;
	}

	crt_memset(id, 0, sizeof(*id));
	if (hex_bytes(s, crt_strlen(s), (char *)id, sizeof(*id))) {
		crt_free(id);
		CLOG(CL_ERR, "conv failed");
		return NULL;
	}

	return id;
}
EXPORT_SYMBOL(ds_obj_id_by_str);
