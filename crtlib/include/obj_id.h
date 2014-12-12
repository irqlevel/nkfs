#pragma once

#include <include/ds_obj_id.h>

extern asmlinkage char *ds_obj_id_to_str(struct ds_obj_id *id);
extern asmlinkage int ds_obj_id_gen(struct ds_obj_id *id);
extern asmlinkage struct ds_obj_id *ds_obj_id_create(void);

