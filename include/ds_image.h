#pragma once

#include <crt/include/sha256.h>
#include <crt/include/obj_id.h>

/*
 * Ds image on disk structure:
 *  header-> allocated blocks bitmap
 *  	\
 *  	 -> inodes tree
 *  	           \
 *  	            ->inode0 -> data blocks tree
 *  	            \	\
 *  	            |	  -> data blocks sha256 sums tree
 *  	            ...
 *  	            \
 *  	             ->inode2
 *  	             \
 *  	              ....
 */

#define DS_BLOCK_SIZE 4096

#define DS_IMAGE_MAGIC	0x3EFFBDAE
#define DS_IMAGE_SIG	0xBEDABEDA
#define DS_IMAGE_VER_1	1

#define DS_IMAGE_BM_BLOCK 1

#define BTREE_T 63
#define BTREE_SIG1 ((u32)0xCBACBADA)
#define BTREE_SIG2 ((u32)0x3EFFEEFE)
#define BTREE_NODE_PAD 40

#define DS_INODE_SIG1 ((u32)0xCBDACBDA)
#define DS_INODE_SIG2 ((u32)0xBEDABEDA)

#pragma pack(push, 1)

struct btree_node_disk {
	__be32			sig1; /* = BTREE_SIG1 */
	struct ds_obj_id	keys[2*BTREE_T-1];
	__be64			values[2*BTREE_T-1];
	__be64			childs[2*BTREE_T]; /* offsets in block units */
	__be32			leaf; /* leaf or internal node */
	__be32			nr_keys; /* number of valid keys */
	u8			pad[BTREE_NODE_PAD];
	struct sha256_sum	sum; /* sha256 check sum of [sig1 ... pad] */
	__be32			sig2; /* = BTREE_SIG2 */
};

struct ds_inode_disk {
	__be32			sig1; /* = DS_INODE_SIG1 */
	struct ds_obj_id 	ino; /* unique id */
	__be64			size; /* size of data in bytes */
	__be64			blocks_tree_block; /* data blocks tree */
	__be64			blocks_sum_tree_block; /* sha256 data sums */
	u8			pad[4016];
	struct sha256_sum	sum; /* sha256 sum of [sig1 ...pad] */
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
	struct sha256_sum	sum; /* sha256 sum of [sig1 ... bsize ] */
	__be32			sig; /* = DS_IMAGE_SIG */ 
};

#pragma pack(pop)

_Static_assert(sizeof(struct btree_node_disk) == DS_BLOCK_SIZE,
	"size is not correct");
_Static_assert(sizeof(struct ds_inode_disk) == DS_BLOCK_SIZE,
	"incorrect sizes");

