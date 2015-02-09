#pragma once

#include <crt/include/obj_id.h>
#include <crt/include/csum.h>

/*
 * Ds image on disk structure:
 *  header-> allocated blocks bitmap
 *  	\
 *  	 -> inodes b-tree {obj_id -> block} 
 *  	           \
 *  	            ->inode0 -> data blocks b-tree {vblock -> block}
 *  	            \	\
 *  	            |	  -> data blocks sha256 sums b-tree {vblock -> block}
 *  	            ...
 *  	            \
 *  	             ->inode2
 *  	             \
 *  	              ....
 */

#define DS_BLOCK_SIZE (64*1024)

#define DS_IMAGE_MAGIC	0x3EFFBDAE
#define DS_IMAGE_SIG	0xBEDABEDA
#define DS_IMAGE_VER_1	1

#define DS_IMAGE_BM_BLOCK 1

/* Do not change if you do not know about B-tree */
#define BTREE_T			896
#define BTREE_KEY_PAGES		7
#define BTREE_CHILD_PAGES	4
#define BTREE_VALUE_PAGES	4

#define BTREE_SIG1 ((u32)0xCBACBADA)
#define BTREE_SIG2 ((u32)0x3EFFEEFE)

#define DS_INODE_SIG1 ((u32)0xCBDACBDA)
#define DS_INODE_SIG2 ((u32)0xBEDABEDA)

#pragma pack(push, 1)

struct btree_key {
	u8	val[16];
};

struct btree_child {
	union {
		__be64 	val_be;
		u64	val;
	};
};

struct btree_value {
	union {
		__be64 	val_be;
		u64	val;
	};
};

struct btree_key_page {
        struct btree_key keys[256];
};

struct btree_child_page {
	struct btree_child childs[512];
};

struct btree_value_page {
	struct btree_value values[512];
};

struct btree_header_page {
	__be32			sig1;
	__be32			leaf;
	__be32			nr_keys;
	char			pad[4072];
	struct csum		sum;
	__be32			sig2;
};

_Static_assert(sizeof(struct btree_key_page) == PAGE_SIZE,
	"size is not correct");

_Static_assert(sizeof(struct btree_child_page) == PAGE_SIZE,
	"size is not correct");

_Static_assert(sizeof(struct btree_value_page) == PAGE_SIZE,
	"size is not correct");

_Static_assert(sizeof(struct btree_header_page) == PAGE_SIZE,
	"size is not correct");

struct btree_node_disk {
	struct btree_header_page	header;
	struct btree_key_page		keys[BTREE_VALUE_PAGES];
	struct btree_child_page		childs[BTREE_CHILD_PAGES];
	struct btree_value_page		values[BTREE_VALUE_PAGES];
};

struct ds_inode_disk {
	__be32			sig1; /* = DS_INODE_SIG1 */
	struct ds_obj_id 	ino; /* unique id */
	__be64			size; /* size of data in bytes */
	__be64			blocks_tree_block; /* data blocks tree */
	__be64			blocks_sum_tree_block; /* sha256 data sums */
	struct csum		sum; /* sha256 sum of [sig1 ...pad] */
	__be32			sig2; /* = DS_INODE_SIG2 */
};

struct ds_image_header {
	__be32			magic; /* = DS_IMAGE_MAGIC */
	__be32			version; /* = DS_IMAGE_VER1 */
	struct ds_obj_id	id;	/* image unique id */
	__be64			size; /*size of image in bytes includes header*/
	__be64			bm_block; /*first blocks bitmap's block */
	__be64			bm_blocks; /* number of bitmap blocks */
	__be64			inodes_tree_block; /* inodes tree */
	__be64			used_blocks; /*number of allocated blocks */
	__be32			bsize; /* block size in bytes=DS_BLOCK_SIZE */
	struct csum		sum; /* sum of [sig1 ... bsize ] */
	__be32			sig; /* = DS_IMAGE_SIG */ 
};

#pragma pack(pop)

_Static_assert(sizeof(struct btree_node_disk) <= DS_BLOCK_SIZE,
	"size is not correct");
_Static_assert(sizeof(struct ds_inode_disk) <= DS_BLOCK_SIZE,
	"incorrect sizes");
_Static_assert(sizeof(struct ds_image_header) <= DS_BLOCK_SIZE,
	"incorrect sizes");
