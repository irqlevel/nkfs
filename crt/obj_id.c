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

static void ds_obj_id_minus(struct ds_obj_id *id1, struct ds_obj_id *id2,
	struct ds_obj_id *result)
{
	if (id1->high > id2->high) {
		if (id1->low >= id2->low) {
			result->high = id1->high - id2->high;
			result->low = id1->low - id2->low;
			return;
		} else {
			result->high = id1->high - id2->high - 1;
			result->low = (U64_MAX - id2->low) + id1->low + 1;
			return;
		}
	} else if (id1->high == id2->high) {
		CRT_BUG_ON(id1->low < id2->low);
		result->high = 0;
		result->low = id1->low - id2->low;
		return;
	} else
		CRT_BUG();
	return;
}

void ds_obj_id_dist(struct ds_obj_id *id1, struct ds_obj_id *id2,
	struct ds_obj_id *result)
{
	return (ds_obj_id_cmp(id1, id2) >= 0) ? 
		ds_obj_id_minus(id1, id2, result) :
		ds_obj_id_minus(id2, id1, result);
}
EXPORT_SYMBOL(ds_obj_id_dist);

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
