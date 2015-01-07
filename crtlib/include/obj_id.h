#pragma once

#include <include/ds_obj_id.h>

extern asmlinkage char *ds_obj_id_to_str(struct ds_obj_id *id);
extern asmlinkage int ds_obj_id_gen(struct ds_obj_id *id);
extern asmlinkage struct ds_obj_id *ds_obj_id_create(void);
extern asmlinkage int ds_obj_id_cmp(struct ds_obj_id *id1,
			struct ds_obj_id *id2);
extern asmlinkage void ds_obj_id_copy(struct ds_obj_id *dst,
			struct ds_obj_id *src);
