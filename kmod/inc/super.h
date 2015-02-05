#pragma once

struct ds_sb {
	struct list_head	list;
	struct ds_dev 		*dev;
	struct dio_dev		*ddev;
	struct block_device 	*bdev;	
	atomic_t		refs;
	struct ds_obj_id	id;	
	struct rw_semaphore	rw_lock;
	struct btree		*inodes_tree;
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

struct ds_sb_link {
	struct list_head 	list;
	struct ds_sb		*sb;
};

void ds_sb_stop(struct ds_sb *sb);

void ds_sb_ref(struct ds_sb *sb);
void ds_sb_deref(struct ds_sb *sb);
struct ds_sb *ds_sb_lookup(struct ds_obj_id *id);

int ds_sb_insert(struct ds_sb *sb);

int ds_sb_format(struct ds_dev *dev, struct ds_sb **psb);
int ds_sb_load(struct ds_dev *dev, struct ds_sb **psb);

int ds_sb_list_create_obj(struct ds_obj_id *pobj_id);

int ds_sb_list_get_obj(struct ds_obj_id *obj_id, u64 off,
	u32 pg_off, u32 len, struct page **pages, int nr_pages, u32 *pread);

int ds_sb_list_put_obj(struct ds_obj_id *obj_id, u64 off,
	u32 pg_off, u32 len, struct page **pages, int nr_pages);

int ds_sb_list_delete_obj(struct ds_obj_id *obj_id);

int ds_sb_list_query_obj(struct ds_obj_id *obj_id, struct ds_obj_info *info);

int ds_sb_init(void);
void ds_sb_finit(void);
