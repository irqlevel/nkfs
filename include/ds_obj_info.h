#pragma once
#include <include/ds_types.h>
#include <include/ds_const.h>
#pragma pack(push, 1)

struct ds_obj_info {
	struct ds_obj_id	obj_id;
	struct ds_obj_id	sb_id;
	char			dev_name[DS_NAME_MAX_SZ];
	u64			size;
	u64			block;
	u32			bsize;
};

#pragma pack(pop)
