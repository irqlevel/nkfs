#pragma once

#pragma pack(push, 1)

struct ds_obj_info {
	struct ds_obj_id	obj_id;
	struct ds_obj_id	sb_id;
	char			dev_name[256];
	u64			size;
	u64			block;
	u32			bsize;
};

#pragma pack(pop)
