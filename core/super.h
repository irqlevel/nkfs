#ifndef __NKFS_CORE_SUPER_H__
#define __NKFS_CORE_SUPER_H__

#include "btree.h"

#include <include/nkfs_obj_id.h>
#include <include/nkfs_obj_info.h>
#include <include/nkfs_image.h>

#include <linux/rwsem.h>

struct nkfs_sb {
	struct list_head	list;
	struct nkfs_dev		*dev;
	struct dio_dev		*ddev;
	struct block_device	*bdev;
	atomic_t		refs;
	struct nkfs_obj_id	id;
	struct rw_semaphore	rw_lock;
	struct nkfs_btree	*inodes_tree;
	struct rb_root		inodes;
	rwlock_t		inodes_lock;
	int			inodes_active;
	u64			nr_blocks;
	u32			magic;
	u32			version;
	u64			size;
	u64			bm_block;
	u64			bm_blocks;
	u64			inodes_tree_block;
	atomic64_t		used_blocks;
	u32			bsize;
	int			stopping;
};

_Static_assert(sizeof(struct nkfs_btree_key) >= sizeof(struct nkfs_obj_id),
	"sizes error");

_Static_assert(sizeof(struct nkfs_btree_value) >= sizeof(u64),
	"sizes error");

struct nkfs_sb_link {
	struct list_head	list;
	struct nkfs_sb		*sb;
};

void nkfs_sb_stop(struct nkfs_sb *sb);

void nkfs_sb_ref(struct nkfs_sb *sb);
void nkfs_sb_deref(struct nkfs_sb *sb);
struct nkfs_sb *nkfs_sb_lookup(struct nkfs_obj_id *id);

int nkfs_sb_insert(struct nkfs_sb *sb);

int nkfs_sb_format(struct nkfs_dev *dev, struct nkfs_sb **psb);
int nkfs_sb_load(struct nkfs_dev *dev, struct nkfs_sb **psb);

int nkfs_sb_list_create_obj(struct nkfs_obj_id *pobj_id);

int nkfs_sb_list_get_obj(struct nkfs_obj_id *obj_id, u64 off,
	u32 pg_off, u32 len, struct page **pages, int nr_pages, u32 *pread);

int nkfs_sb_list_put_obj(struct nkfs_obj_id *obj_id, u64 off,
	u32 pg_off, u32 len, struct page **pages, int nr_pages);

int nkfs_sb_list_delete_obj(struct nkfs_obj_id *obj_id);

int nkfs_sb_list_query_obj(struct nkfs_obj_id *obj_id,
			   struct nkfs_obj_info *info);

int nkfs_sb_init(void);
void nkfs_sb_finit(void);

#endif
