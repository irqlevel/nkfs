#pragma once
#include <include/nkfs_types.h>
#include <include/nkfs_const.h>
#pragma pack(push, 1)

struct nkfs_obj_info {
	struct nkfs_obj_id	obj_id;
	struct nkfs_obj_id	sb_id;
	char			dev_name[NKFS_NAME_MAX_SZ];
	u64			size;
	u64			block;
	u32			bsize;
};

#pragma pack(pop)
