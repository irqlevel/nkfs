#pragma once

struct ds_sb {
	struct list_head	list;
	struct ds_dev 		*dev;
	struct block_device 	*bdev;	
	atomic_t		refs;
	struct ds_obj_id	id;	
	struct rw_semaphore	rw_lock;
	struct btree		*obj_tree;
	u64			nr_blocks;
	u32			magic;
	u32			version;
	u64			size;
	u64			bm_block;
	u64			bm_blocks;
	u64			obj_tree_block;
	u32			bsize;
};

void ds_sb_stop(struct ds_sb *sb);

void ds_sb_ref(struct ds_sb *sb);
void ds_sb_deref(struct ds_sb *sb);
struct ds_sb *ds_sb_lookup(struct ds_obj_id *id);

int ds_sb_insert(struct ds_sb *sb);

int ds_sb_format(struct ds_dev *dev, struct ds_sb **psb);
int ds_sb_load(struct ds_dev *dev, struct ds_sb **psb);


int ds_sb_insert_obj(struct ds_sb *sb, struct ds_obj_id *obj_id,
	u64 value, int replace);

int ds_sb_find_obj(struct ds_sb *sb, struct ds_obj_id *obj_id,
	u64 *pvalue);

int ds_sb_delete_obj(struct ds_sb *sb, struct ds_obj_id *obj_id);

int ds_sb_init(void);
void ds_sb_finit(void);
