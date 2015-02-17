#include <crt/include/crt.h>

char *nkfs_obj_id_str(struct nkfs_obj_id *id)
{
	return bytes_hex((char *)id, sizeof(*id));
}
EXPORT_SYMBOL(nkfs_obj_id_str);

int nkfs_obj_id_gen(struct nkfs_obj_id *id)
{
	return crt_random_buf(id, sizeof(*id));
}
EXPORT_SYMBOL(nkfs_obj_id_gen);

int nkfs_obj_id_cmp(struct nkfs_obj_id *id1, struct nkfs_obj_id *id2)
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
EXPORT_SYMBOL(nkfs_obj_id_cmp);

static void nkfs_obj_id_minus(struct nkfs_obj_id *id1, struct nkfs_obj_id *id2,
	struct nkfs_obj_id *result)
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

void nkfs_obj_id_dist(struct nkfs_obj_id *id1, struct nkfs_obj_id *id2,
	struct nkfs_obj_id *result)
{
	return (nkfs_obj_id_cmp(id1, id2) >= 0) ? 
		nkfs_obj_id_minus(id1, id2, result) :
		nkfs_obj_id_minus(id2, id1, result);
}
EXPORT_SYMBOL(nkfs_obj_id_dist);

void nkfs_obj_id_copy(struct nkfs_obj_id *dst, struct nkfs_obj_id *src)
{
	crt_memcpy(dst, src, sizeof(*src));
}
EXPORT_SYMBOL(nkfs_obj_id_copy);

struct nkfs_obj_id *nkfs_obj_id_create(void)
{
	struct nkfs_obj_id *id;
	int err;

	id = crt_malloc(sizeof(struct nkfs_obj_id));
	if (!id) {
		return NULL;
	}

	err = nkfs_obj_id_gen(id);
	if (err) {
		crt_free(id);
		return NULL;
	}

	return id;
}
EXPORT_SYMBOL(nkfs_obj_id_create);


struct nkfs_obj_id *nkfs_obj_id_by_str(char *s)
{
	struct nkfs_obj_id *id;
	
	id = crt_malloc(sizeof(struct nkfs_obj_id));
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
EXPORT_SYMBOL(nkfs_obj_id_by_str);
