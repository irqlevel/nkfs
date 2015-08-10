#pragma once

#pragma pack(push, 1)

struct nkfs_inode {
	u32			sig1;
	u32			pad;
	atomic_t		ref;
	struct nkfs_obj_id	ino;
	struct rb_node		inodes_link;
	struct rw_semaphore	rw_sem;
	u64			block;
	u64			size;
	u64			blocks_tree_block;
	u64			blocks_sum_tree_block;
	struct nkfs_btree	*blocks_tree;
	struct nkfs_btree	*blocks_sum_tree;
	struct nkfs_sb		*sb;
	u32			dirty;
	u32			sig2;
};

struct inode_block {
	u64			vblock;
	u64			block;
	u64			vsum_block;
	u64			sum_block;
	struct dio_cluster	*clu;
	struct dio_cluster	*sum_clu;
	u32			sum_off;
};

#pragma pack(pop)

void nkfs_inode_ref(struct nkfs_inode *inode);
void nkfs_inode_deref(struct nkfs_inode *inode);

#define INODE_REF(n)	\
	nkfs_inode_ref(n);

#define INODE_DEREF(n)	\
	nkfs_inode_deref(n);

int nkfs_inode_io_pages(struct nkfs_inode *inode, u64 off,
		u32 pg_off, u32 len, struct page **pages,
		int nr_pages, int write, u32 *pio_count);

struct nkfs_inode *nkfs_inode_create(struct nkfs_sb *sb, struct nkfs_obj_id *ino);
struct nkfs_inode *nkfs_inode_read(struct nkfs_sb *sb, u64 block);
void nkfs_inode_delete(struct nkfs_inode *inode);

int nkfs_inode_init(void);
void nkfs_inode_finit(void);

