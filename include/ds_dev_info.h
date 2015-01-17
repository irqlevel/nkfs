#pragma once
#include <include/ds_types.h>
#include <include/ds_const.h>

#pragma pack(push, 1)

struct ds_dev_info {
	struct ds_obj_id	sb_id;
	char			dev_name[DS_NAME_MAX_SZ];
	u64			size;
	u64			used_size;
	u64			free_size;
	u64			blocks;
	u64			used_blocks;
	u64			obj_tree_block;
	u64			bm_block;
	u64			bm_blocks;
	u32			bsize;
	unsigned int		major;
	unsigned int		minor;
};

#pragma pack(pop)
