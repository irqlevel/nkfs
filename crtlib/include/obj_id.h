#pragma once

#include <include/ds_obj_id.h>

char *ds_obj_id_str(struct ds_obj_id *id);
int ds_obj_id_gen(struct ds_obj_id *id);
struct ds_obj_id *ds_obj_id_create(void);
int ds_obj_id_cmp(struct ds_obj_id *id1,
			struct ds_obj_id *id2);
void ds_obj_id_copy(struct ds_obj_id *dst,
			struct ds_obj_id *src);
