#pragma once

#define DS_IMAGE_MAGIC 0x3EFFBDAE

#define DS_IMAGE_VER_1	1

#define DS_IMAGE_BM_BLOCK 1

#pragma pack(push, 1)
struct ds_image_header {
	__be32			magic;
	__be32			version;
	__be64			size;
	struct ds_obj_id	id;
	__be32			bsize;
	__be64			bm_block;
	__be64			bm_blocks;
	__be64			obj_tree_block;
};
#pragma pack(pop)
