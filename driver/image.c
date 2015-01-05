#include <inc/ds_priv.h>

#define __SUBCOMPONENT__ "image"

static int ds_image_gen_header(struct ds_image *image,
	u64 size,
	u32 bsize)
{
	int err;
	u64 bm_blocks;	

	if (ds_mod(size, bsize) || (size <= bsize)) {
		KLOG(KL_ERR, "size %lld bsize %u", size, bsize);
		return -EINVAL;
	}

	bm_blocks = ds_div_round_up(ds_div_round_up(ds_div(size, bsize), 8), bsize);
	if (size <= (DS_IMAGE_BM_BLOCK + bm_blocks)*bsize) {
		KLOG(KL_ERR, "size %lld to small bsize %u bm_blocks %u",
			size, bsize, bm_blocks);
		return -EINVAL;
	}

	err = ds_obj_id_gen(&image->id);
	if (err) {
		KLOG(KL_ERR, "cant gen obj id err %d", err);
		return err;
	}


	image->magic = DS_IMAGE_MAGIC;
	image->version = DS_IMAGE_VER_1;
	image->size = size;
	image->bsize = bsize;

	image->bm_block = DS_IMAGE_BM_BLOCK;
	image->bm_blocks = bm_blocks;

	return 0;
}

static void ds_image_parse_header(struct ds_image *image,
	struct ds_image_header *header)
{
	image->magic = be32_to_cpu(header->magic);
	image->version = be32_to_cpu(header->version);
	image->size = be64_to_cpu(header->size);
	image->bsize = be32_to_cpu(header->bsize);
	image->bm_block = be32_to_cpu(header->bm_block);
	image->bm_blocks = be64_to_cpu(header->bm_blocks);
	memcpy(&image->id, &header->id, sizeof(header->id));	
}

static void ds_image_fill_header(struct ds_image *image,
	struct ds_image_header *header)
{
	memset(header, 0, sizeof(*header));

	header->magic = cpu_to_be32(image->magic);
	header->version = cpu_to_be32(image->version);
	header->size = cpu_to_be64(image->size);
	header->bsize = cpu_to_be32(image->bsize);
	header->bm_block = cpu_to_be32(image->bm_block);
	header->bm_blocks = cpu_to_be64(image->bm_blocks);
	memcpy(&header->id, &image->id, sizeof(image->id));
}

static int ds_image_check(struct ds_image *image)
{
	int err;

	if (image->magic != DS_IMAGE_MAGIC) {
		KLOG(KL_ERR, "img %p invalid magic %x", image,
			image->magic);
		err = -EINVAL;
		goto out;
	}

	if (image->version != DS_IMAGE_VER_1) {
		KLOG(KL_ERR, "img %p invalid version %x", image,
				image->version);
		err = -EINVAL;
		goto out;
	}

	if ((image->size == 0) || (image->bsize == 0) ||
		(image->size <= image->bsize) ||
		(ds_mod(image->size, image->bsize))) {
		KLOG(KL_ERR, "img %p invalid size %llu bsize %u",
			image, image->size, image->bsize);
		err = -EINVAL;
		goto out;
	}

	if (image->bm_block != DS_IMAGE_BM_BLOCK) {
		KLOG(KL_ERR, "img %p invalid bm_block %u",
			image, image->bm_block);
		err = -EINVAL;
		goto out;
	}

	if (image->size <= (image->bm_blocks + DS_IMAGE_BM_BLOCK)) {
		KLOG(KL_ERR, "img %p invalid size %llu bm_blocks %llu",
			image, image->size, image->bm_blocks);
		err = -EINVAL;
		goto out;
	}

	err = 0;
out:
	return err;
}


static int ds_image_create(struct ds_dev *dev,
		struct ds_image_header *header,
		struct ds_image **pimage)
{
	int err;
	struct ds_image *image;

	image = kmalloc(GFP_KERNEL, sizeof(struct ds_image));
	if (!image) {
		KLOG(KL_ERR, "cant alloc image");
		return -ENOMEM;
	}
	memset(image, 0, sizeof(*image));

	if (!header) {
		err = ds_image_gen_header(image, i_size_read(dev->bdev->bd_inode),
			dev->bsize);
		if (err) {
			KLOG(KL_ERR, "can gen header");
			goto free_image;
		}
	} else {
		ds_image_parse_header(image, header);
	}

	err = ds_image_check(image);
	if (err) {
		KLOG(KL_ERR, "invalid image");
		goto free_image;
	}

	image->dev = dev;
	*pimage = image;
	return 0;

free_image:
	kfree(image);
	return err;
}

static void ds_image_release(struct ds_image *image)
{
	KLOG(KL_DBG, "img %p", image);
}

void ds_image_delete(struct ds_image *image)
{
	ds_image_release(image);
	kfree(image);
}

void ds_image_stop(struct ds_image *image)
{
	KLOG(KL_ERR, "image %p dev %s stopping",
			image, image->dev->dev_name);
}

int ds_image_format(struct ds_dev *dev, struct ds_image **pimage)
{
	struct buffer_head *bh;
	struct ds_image_header *header;
	int err;
	struct ds_image *image = NULL;

	bh = __bread(dev->bdev, 0, dev->bsize);
	if (!bh) {
		KLOG(KL_ERR, "cant read 0block");
		err = -EIO;
		goto out;	
	}

	header = kmalloc(sizeof(*header), GFP_NOFS);
	if (!header) {
		err = -ENOMEM;
		goto free_bh;
	}

	err = ds_image_create(dev, NULL, &image);
	if (err) {
		KLOG(KL_ERR, "cant create image");
		goto free_header;
	}

	memset(bh->b_data, 0, dev->bsize);
	ds_image_fill_header(image, header);
	memcpy(bh->b_data, header, sizeof(*header));

	mark_buffer_dirty(bh);
	err = sync_dirty_buffer(bh);
	if (err) {
		KLOG(KL_ERR, "sync 0block err %d", err);
		goto del_image;
	}

	err = 0;
	*pimage = image;
	goto free_header;

del_image:
	ds_image_delete(image);
free_header:
	kfree(header);
free_bh:
	brelse(bh);
out:
	return err;
}

int ds_image_load(struct ds_dev *dev, struct ds_image **pimage)
{
	struct buffer_head *bh;
	struct ds_image *image = NULL;
	int err;
	
	bh = __bread(dev->bdev, 0, dev->bsize);
	if (!bh) {
		KLOG(KL_ERR, "cant read 0block");
		err = -EIO;
		goto out;	
	}

	err = ds_image_create(dev, (struct ds_image_header *)bh->b_data,
		&image);
	if (err) {
		KLOG(KL_ERR, "cant init image");
		goto free_bh;
	}

	if (image->size > i_size_read(dev->bdev->bd_inode)) {
		KLOG(KL_ERR, "dev %p invalid size %llu img %p", dev,
				image->size, image);
		err = -EINVAL;
		goto free_image;
	}
	*pimage = image;
	err = 0;
	goto free_bh;

free_image:
	ds_image_delete(image);
free_bh:
	brelse(bh);
out:
	return err;
}
