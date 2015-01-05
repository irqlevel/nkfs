#pragma once

struct ds_image {
	struct 			amap map;
	struct ds_dev 		*dev;
	u32			magic;
	u32			version;
	u64			size;
	struct ds_obj_id	id;
	u32			bsize;
	u32			bm_block;
	u64			bm_blocks;
};

void ds_image_delete(struct ds_image *image);
void ds_image_stop(struct ds_image *image);

int ds_image_format(struct ds_dev *dev, struct ds_image **pimage);
int ds_image_load(struct ds_dev *dev, struct ds_image **pimage);
