#pragma once

#include <include/nkfs_obj_id.h>

char *nkfs_obj_id_str(struct nkfs_obj_id *id);
int nkfs_obj_id_gen(struct nkfs_obj_id *id);
struct nkfs_obj_id *nkfs_obj_id_create(void);
int nkfs_obj_id_cmp(struct nkfs_obj_id *id1,
			struct nkfs_obj_id *id2);

void nkfs_obj_id_dist(struct nkfs_obj_id *id1, struct nkfs_obj_id *id2,
	struct nkfs_obj_id *result);

void nkfs_obj_id_copy(struct nkfs_obj_id *dst,
			struct nkfs_obj_id *src);

struct nkfs_obj_id *nkfs_obj_id_by_str(char *s);
