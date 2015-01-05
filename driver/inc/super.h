#pragma once

struct ds_sb {
	struct list_head	list;
	struct ds_dev 		*dev;
	atomic_t		refs;
	struct ds_obj_id	id;	
	u32			magic;
	u32			version;
	u64			size;
	u32			bsize;
	u32			bm_block;
	u64			bm_blocks;
};

void ds_sb_delete(struct ds_sb *image);
void ds_sb_stop(struct ds_sb *image);

int ds_sb_format(struct ds_dev *dev, struct ds_sb **psb);
int ds_sb_load(struct ds_dev *dev, struct ds_sb **psb);
