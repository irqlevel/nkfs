#pragma once

#define DS_INODE_SIG1 ((u32)0xCBDACBDA)
#define DS_INODE_SIG2 ((u32)0xBEDABEDA)

#pragma pack(push, 1)

struct ds_inode_disk {
	__be32			sig1;
	struct ds_obj_id 	ino;
	__be64			size;
	__be64			blocks_tree_block;
	__be64			blocks_sum_tree_block;
	u8			pad[4016];			
	struct sha256_sum	sum;
	__be32			sig2;
};

struct ds_inode {
	atomic_t		ref;
	struct ds_obj_id	ino;
	struct rb_node		inodes_link;
	u64			block;
	u64			size;
	u64			blocks_tree_block;
	u64			blocks_sum_tree_block;
	struct btree		*blocks_tree;
	struct btree		*blocks_sum_tree;
	struct ds_sb		*sb;
};

#pragma pack(pop)

_Static_assert(sizeof(struct ds_inode_disk) == DS_BLOCK_SIZE,
	"incorrect sizes");


void ds_inode_ref(struct ds_inode *inode);
void ds_inode_deref(struct ds_inode *inode);

#define INODE_REF(n)	\
	ds_inode_ref(n);

#define INODE_DEREF(n)	\
	ds_inode_deref(n);

int ds_inode_read_buf(struct ds_inode *inode, u64 off, void *buf, u32 len);
int ds_inode_write_buf(struct ds_inode *inode, u64 off, void *buf, u32 len);

struct ds_inode *ds_inode_create(struct ds_sb *sb, struct ds_obj_id *ino);
struct ds_inode *ds_inode_read(struct ds_sb *sb, u64 block);
void ds_inode_delete(struct ds_inode *inode);

int ds_inode_init(void);
void ds_inode_finit(void);

