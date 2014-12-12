#include <inc/ds_priv.h>

#define __SUBCOMPONENT__ "ds-image"
	
#define IMAGE_AMAP_PAGES 128

static void ds_image_io(void *io, int rw, struct page *page, u64 off,
		void *context, io_complete_t complete)
{
	struct ds_image *image = io;
	klog(KL_INF, "image %p devname %s io off %llu rw %d",
			image, image->dev->dev_name, off, rw);
	return;
}

static int ds_image_create(struct ds_dev *dev, struct ds_image_header *header,
		struct ds_image **pimage)
{
	int err;
	struct ds_image *image;

	image = kmalloc(GFP_KERNEL, sizeof(struct ds_image));
	if (dev->image) {
		klog(KL_ERR, "cant alloc image");
		return -ENOMEM;
	}

	memset(image, 0, sizeof(*image));
	memcpy(&image->header, header, sizeof(*header));
	image->dev = dev;

	err = amap_init(&image->map, image, IMAGE_AMAP_PAGES, ds_image_io);
	if (err) {
		klog(KL_ERR, "amap_init err %d", err);
		goto free_image;
	}

	*pimage = image;
	return 0;

free_image:
	kfree(image);
	return err;
}

static void ds_image_release(struct ds_image *image)
{
	amap_release(&image->map);
}

void ds_image_delete(struct ds_image *image)
{
	ds_image_release(image);
	kfree(image);
}

void ds_image_stop(struct ds_image *image)
{
	klog(KL_ERR, "image %p dev %s stopping",
			image, image->dev->dev_name);
}

static int ds_image_gen_header(struct ds_image_header *header, u64 size)
{
	int err;
	memset(header, 0, sizeof(*header));
	err = ds_obj_id_gen(&header->id);
	if (err) {
		klog(KL_ERR, "cant gen obj id err %d", err);
		return err;
	}
	ds_image_header_set_magic(header, DS_IMAGE_MAGIC);
	ds_image_header_set_version(header, DS_IMAGE_VER_1);
	ds_image_header_set_size(header, size);
	return 0;
}

int ds_image_format(struct ds_dev *dev, struct ds_image **pimage)
{
	struct page *page;
	struct ds_image_header *header;
	int err;
	struct ds_image *image = NULL;

	page = alloc_page(GFP_NOIO);
	if (!page) {
		klog(KL_ERR, "no page");
		return -ENOMEM;
	}
	header = kmap(page);
	memset(header, 0, PAGE_SIZE);
	err = ds_image_gen_header(header, i_size_read(dev->bdev->bd_inode));
	if (err) {
		klog(KL_ERR, "cant gen image header");
		goto free_page;
	}

	err = ds_image_create(dev, header, &image);
	if (err) {
		klog(KL_ERR, "cant create image");
		goto free_page;
	}

	err = ds_dev_io_page(dev, DS_IO_CTX_NULL, page, 0,
			REQ_WRITE|REQ_FUA, DS_IO_SYNC, DS_IO_COMP_NULL);
	if (err) {
		klog(KL_ERR, "ds_dev_io_page err %d", err);
		goto free_image;
	}
	err = 0;
	*pimage = image;
	goto free_page;
free_image:
	ds_image_delete(image);
free_page:
	kunmap(page);
	put_page(page);
	return err;
}

int ds_image_load(struct ds_dev *dev, struct ds_image **pimage)
{
	struct page *page;
	struct ds_image_header *header;
	struct ds_image *image = NULL;
	int err;

	page = alloc_page(GFP_NOIO);
	if (!page) {
		klog(KL_ERR, "no page");
		return -ENOMEM;	
	}

	header = kmap(page);
	memset(header, 0, PAGE_SIZE);

	err = ds_dev_io_page(dev, DS_IO_CTX_NULL, page, 0, DS_IO_READ,
			DS_IO_SYNC, DS_IO_COMP_NULL);
	if (err) {
		klog(KL_ERR, "ds_dev_io_page err %d", err);
		goto free_page;
	}

	err = ds_image_create(dev, header, &image);
	if (err) {
		klog(KL_ERR, "cant init image");
		goto free_page;
	}

	if (ds_image_header_magic(&image->header) != DS_IMAGE_MAGIC) {
		klog(KL_ERR, "dev %p invalid magic %x", dev,
				ds_image_header_magic(&image->header));
		err = -EINVAL;
		goto free_image;
	}

	if (ds_image_header_version(&image->header) != DS_IMAGE_VER_1) {
		klog(KL_ERR, "dev %p invalid version %x", dev,
				ds_image_header_version(&image->header));
		err = -EINVAL;
		goto free_image;
	}

	if (ds_image_header_size(&image->header) !=
			i_size_read(dev->bdev->bd_inode)) {
		klog(KL_ERR, "dev %p invalid size %llu", dev,
				ds_image_header_size(&image->header));
		err = -EINVAL;
		goto free_image;
	}
	*pimage = image;
	err = 0;
	goto free_page;

free_image:
	ds_image_delete(image);
free_page:
	kunmap(page);
	put_page(page);
	return err;
}
