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

#define NKFS_BLOCK_SIZE (64*1024)

#define NKFS_IMAGE_MAGIC	0x3EFFBDAE
#define NKFS_IMAGE_SIG		0xBEDABEDA
#define NKFS_IMAGE_VER_1	1

#define NKFS_IMAGE_BM_BLOCK	1

/* Do not change if you do not know about B-tree */
#define NKFS_BTREE_T		896
#define NKFS_BTREE_KEY_PAGES	7
#define NKFS_BTREE_CHILD_PAGES	4
#define NKFS_BTREE_VALUE_PAGES	4

#define NKFS_BTREE_SIG1 ((u32)0xCBACBADA)
#define NKFS_BTREE_SIG2 ((u32)0x3EFFEEFE)

#define NKFS_INODE_SIG1 ((u32)0xCBDACBDA)
#define NKFS_INODE_SIG2 ((u32)0xBEDABEDA)

#pragma pack(push, 1)

struct nkfs_btree_key {
	u8	val[16];
};

struct nkfs_btree_child {
	union {
		__be64	val_be;
		u64	val;
	};
};

struct nkfs_btree_value {
	union {
		__be64	val_be;
		u64	val;
	};
};

struct nkfs_btree_key_page {
        struct nkfs_btree_key keys[256];
};

struct nkfs_btree_child_page {
	struct nkfs_btree_child children[512];
};

struct nkfs_btree_value_page {
	struct nkfs_btree_value values[512];
};

struct nkfs_btree_header_page {
	__be32			sig1;
	__be32			leaf;
	__be32			nr_keys;
	char			pad[4072];
	struct csum		sum;
	__be32			sig2;
};

_Static_assert(sizeof(struct nkfs_btree_key_page) == PAGE_SIZE,
	"size is not correct");

_Static_assert(sizeof(struct nkfs_btree_child_page) == PAGE_SIZE,
	"size is not correct");

_Static_assert(sizeof(struct nkfs_btree_value_page) == PAGE_SIZE,
	"size is not correct");

_Static_assert(sizeof(struct nkfs_btree_header_page) == PAGE_SIZE,
	"size is not correct");

struct nkfs_btree_node_disk {
	struct nkfs_btree_header_page	header;
	struct nkfs_btree_key_page	keys[NKFS_BTREE_VALUE_PAGES];
	struct nkfs_btree_child_page	children[NKFS_BTREE_CHILD_PAGES];
	struct nkfs_btree_value_page	values[NKFS_BTREE_VALUE_PAGES];
};

struct nkfs_inode_disk {
	__be32			sig1; /* = NKFS_INODE_SIG1 */
	struct nkfs_obj_id	ino; /* unique id */
	__be64			size; /* size of data in bytes */
	__be64			blocks_tree_block; /* data blocks tree */
	__be64			blocks_sum_tree_block; /* sha256 data sums */
	struct csum		sum; /* sha256 sum of [sig1 ...pad] */
	__be32			sig2; /* = NKFS_INODE_SIG2 */
};

struct nkfs_image_header {
	__be32			magic; /* = NKFS_IMAGE_MAGIC */
	__be32			version; /* = NKFS_IMAGE_VER1 */
	struct nkfs_obj_id	id;	/* image unique id */
	__be64			size; /*size of image in bytes includes header*/
	__be64			bm_block; /*first blocks bitmap's block */
	__be64			bm_blocks; /* number of bitmap blocks */
	__be64			inodes_tree_block; /* inodes tree */
	__be64			used_blocks; /*number of allocated blocks */
	__be32			bsize; /* block size in bytes=NKFS_BLOCK_SIZE */
	struct csum		sum; /* sum of [sig1 ... bsize ] */
	__be32			sig; /* = NKFS_IMAGE_SIG */
};

#pragma pack(pop)

_Static_assert(sizeof(struct nkfs_btree_node_disk) <= NKFS_BLOCK_SIZE,
	"size is not correct");
_Static_assert(sizeof(struct nkfs_inode_disk) <= NKFS_BLOCK_SIZE,
	"incorrect sizes");
_Static_assert(sizeof(struct nkfs_image_header) <= NKFS_BLOCK_SIZE,
	"incorrect sizes");
