#pragma once

struct ds_image {
	__u32	magic;
	__u32	version;
	__u64	size;
	struct ds_obj_id id;
};


int ds_image_check(struct ds_dev *dev);
int ds_image_format(struct ds_dev *dev);
void ds_image_dev_free(struct ds_dev *dev);
void ds_image_dev_stop(struct ds_dev *dev);

