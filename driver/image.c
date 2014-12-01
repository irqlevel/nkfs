#include <inc/ds_priv.h>

#define __SUBCOMPONENT__ "ds-image"
	
#define DS_IMAGE_INFO_DUMP(DEV)						\
{									\
	struct ds_obj_id *img_id = &((DEV)->image)->id;			\
	char *img_id_s = ds_obj_id_to_str(img_id);			\
	klog(KL_INF, "d=%p m=%x v=%d size=%llu id=%s",			\
		(DEV), (DEV)->image->magic, (DEV)->image->version,	\
		(DEV)->image->size, img_id_s);				\
	if (img_id_s)							\
		kfree(img_id_s);					\
}

void ds_image_dev_free(struct ds_dev *dev)
{
	if (dev->image)
		kfree(dev->image);
}

void ds_image_dev_stop(struct ds_dev *dev)
{
	if (dev->image)
		DS_IMAGE_INFO_DUMP(dev);
}

int ds_image_format(struct ds_dev *dev)
{
	struct page *page;
	struct ds_image_header *header;
	int err;
	struct ds_obj_id *id;

	BUG_ON(dev->image);
	id = ds_obj_id_gen();
	if (!id) {
		klog(KL_ERR, "obj id gen failed");
		return -ENOMEM;
	}

	klog(KL_INF, "generated id=%p", id);

	page = alloc_page(GFP_NOIO);
	if (!page) {
		klog(KL_ERR, "no page");
		err = -ENOMEM;
		goto free_id;	
	}
	header = kmap(page);
	memset(header, 0, PAGE_SIZE);

	dev->image = kmalloc(GFP_KERNEL, sizeof(struct ds_image));
	if (!dev->image) {
		klog(KL_ERR, "cant alloc ds_image struct");
		err = -ENOMEM;
		goto free_page;
	}
	memset(dev->image, 0, sizeof(struct ds_image));
	dev->image->magic = DS_IMAGE_MAGIC;
	dev->image->version = DS_IMAGE_VER_1;
	dev->image->size = i_size_read(dev->bdev->bd_inode);
	memcpy(&dev->image->id, id, sizeof(*id));

	DS_IMAGE_INFO_DUMP(dev);

	ds_image_header_set_magic(header, dev->image->magic);
	ds_image_header_set_version(header, dev->image->version);
	ds_image_header_set_id(header, &dev->image->id);
	ds_image_header_set_size(header,  dev->image->size);

	err = ds_dev_io_page(dev, DIO_CONTEXT_NULL, page, 0, REQ_WRITE|REQ_FUA,
			DIO_SYNC, DIO_COMPCLB_NULL);
	if (err) {
		klog(KL_ERR, "ds_dev_io_page err %d", err);
		kfree(dev->image);
		dev->image = NULL;
		goto free_page;
	}
	err = 0;
free_page:
	kunmap(page);
	__free_page(page);
free_id:
	kfree(id);
	return err;
}

int ds_image_check(struct ds_dev *dev)
{
	struct page *page;
	struct ds_image_header *header;
	int err;

	BUG_ON(dev->image);

	page = alloc_page(GFP_NOIO);
	if (!page) {
		klog(KL_ERR, "no page");
		return -ENOMEM;	
	}
	header = kmap(page);
	memset(header, 0, PAGE_SIZE);

	err = ds_dev_io_page(dev, DIO_CONTEXT_NULL, page, 0, DIO_RW_FLAGS_READ,
			DIO_SYNC, DIO_COMPCLB_NULL);
	if (err) {
		klog(KL_ERR, "ds_dev_io_page err %d", err);
		goto free_page;
	}

	dev->image = kmalloc(GFP_KERNEL, sizeof(struct ds_image));
	if (!dev->image) {
		klog(KL_ERR, "cant alloc ds_image struct");
		err = -ENOMEM;
		goto free_page;
	}
	memset(dev->image, 0, sizeof(struct ds_image));
	dev->image->magic = ds_image_header_magic(header);
	dev->image->version = ds_image_header_version(header);
	dev->image->size = ds_image_header_size(header);
	ds_image_header_id(header, &dev->image->id);
		
	DS_IMAGE_INFO_DUMP(dev);

	if (dev->image->magic != DS_IMAGE_MAGIC) {
		klog(KL_ERR, "dev %p invalid magic %x", dev, ds_image_header_magic(header));
		err = -EINVAL;
		goto free_page;
	}

	if (dev->image->version != DS_IMAGE_VER_1) {
		klog(KL_ERR, "dev %p invalid version %x", dev, ds_image_header_version(header));
		err = -EINVAL;
		goto free_page;
	}

	if (dev->image->size != i_size_read(dev->bdev->bd_inode)) {
		klog(KL_ERR, "dev %p invalid size %llu", dev, ds_image_header_size(header));
		err = -EINVAL;
		goto free_page;
	}

	err = 0;
free_page:
	kunmap(page);
	__free_page(page);
	return err;
}
