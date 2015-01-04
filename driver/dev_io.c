#include <inc/ds_priv.h>

#define __SUBCOMPONENT__ "ds-devio"

static void ds_dev_io_free(struct ds_dev_io *io)
{
	kfree(io->bio->bi_io_vec);
	kfree(io->bio);
	if (io->complete)
		kfree(io->complete);
}

static void ds_dev_io_bio_complete(struct bio *bio, int err)
{
	struct ds_dev_io *io = (struct ds_dev_io *)bio->bi_private;
	
	BUG_ON(bio != io->bio);
	io->err = err;

	KLOG(KL_DBG, "bio %p io %p dev %p err %d", bio, io, io->dev, err);

	if (io->complete_clb)
		io->complete_clb(io->err, io->dev, io->context, io->page, io->off, io->rw_flags);

	spin_lock_irq(&io->dev->io_lock);
	list_del(&io->io_list);
	spin_unlock_irq(&io->dev->io_lock);

	if (io->complete)
		complete(io->complete);
	else
		ds_dev_io_free(io);
}

int ds_dev_io_page(struct ds_dev *dev, void *context, struct page *page, u64 off,
		int rw_flags, int sync, ds_dev_io_complete_t complete_clb)
{
	struct bio *bio;
	struct bio_vec *bio_vec;
	struct ds_dev_io *io;
	int err;

	if (off & (PAGE_SIZE - 1))
		return -EINVAL;

	if (dev->stopping)
		return -EINVAL;

	bio = kmalloc(sizeof(struct bio), GFP_NOIO);
	if (!bio)
		return -ENOMEM;
	memset(bio, 0, sizeof(*bio));
	bio_vec = kmalloc(sizeof(struct bio), GFP_NOIO);
	if (!bio_vec) {
		kfree(bio);
		return -ENOMEM;
	}
	memset(bio_vec, 0, sizeof(*bio_vec));
	io = kmalloc(sizeof(struct ds_dev_io), GFP_NOIO);
	if (!io) {
		kfree(bio_vec);
		kfree(bio);
		return -ENOMEM;
	}
	memset(io, 0, sizeof(*io));
	if (sync) {
		io->complete = kmalloc(sizeof(struct completion), GFP_NOIO);
		if (!io->complete) {
			kfree(bio_vec);
			kfree(bio);
			kfree(io);			
		}
		memset(io->complete, 0, sizeof(*io->complete));
		init_completion(io->complete);
	}

	io->dev = dev;
	io->bio = bio;
	io->complete_clb = complete_clb;
	io->rw_flags = rw_flags;
	io->context = context;

	bio_init(bio);

	bio->bi_io_vec = bio_vec;
	bio->bi_io_vec->bv_page = page;
	bio->bi_io_vec->bv_len = PAGE_SIZE;
	bio->bi_io_vec->bv_offset = 0;

	bio->bi_vcnt = 1;
	bio->bi_iter.bi_size = PAGE_SIZE;
	bio->bi_iter.bi_sector = off >> SECTOR_SHIFT;
	bio->bi_bdev = dev->bdev;
	bio->bi_rw |= rw_flags;
	bio->bi_private = io;
	bio->bi_end_io = ds_dev_io_bio_complete;
	
	spin_lock_irq(&dev->io_lock);
	list_add_tail(&io->io_list, &dev->io_list);
	spin_unlock_irq(&dev->io_lock);

	KLOG(KL_DBG, "bio %p io %p queued dev %p", bio, io, dev);
	generic_make_request(bio);

	if (io->complete) {
		wait_for_completion(io->complete);
		err = io->err;
		ds_dev_io_free(io);
	} else
		err = 0;

	return err;
}

int ds_dev_io_touch0_page(struct ds_dev *dev)
{
	struct page *page;
	int err;

	page = alloc_page(GFP_NOIO);
	if (!page) {
		KLOG(KL_ERR, "cant alloc page");
		return -ENOMEM;
	}

	err = ds_dev_io_page(dev, DS_IO_CTX_NULL, page, 0,
			DS_IO_READ, DS_IO_SYNC, DS_IO_COMP_NULL);
	if (err) {
		KLOG(KL_ERR, "ds_dev_io_page err %d", err);
		goto out;
	}

	err = ds_dev_io_page(dev, DS_IO_CTX_NULL, page, 0,
			REQ_WRITE, DS_IO_SYNC, DS_IO_COMP_NULL);
	if (err) {
		KLOG(KL_ERR, "ds_dev_io_page err %d", err);
		goto out;
	}

	err = 0;
out:
	put_page(page);
	return err;
}


