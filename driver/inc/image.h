#pragma once

struct ds_image {
	struct ds_image_header header;
	struct amap map;
	struct ds_dev *dev;
};

void ds_image_delete(struct ds_image *image);
void ds_image_stop(struct ds_image *image);

int ds_image_format(struct ds_dev *dev, struct ds_image **pimage);
int ds_image_load(struct ds_dev *dev, struct ds_image **pimage);
