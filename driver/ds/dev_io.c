#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/major.h>
#include <linux/blkdev.h>
#include <linux/bio.h>
#include <linux/highmem.h>
#include <linux/mutex.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/cdrom.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/cdev.h>
#include <linux/kthread.h>
#include <linux/time.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>

#include <ds.h>
#include <ds_cmd.h>
#include <klog.h>
#include <ksocket.h>
#include <ds_priv.h>

#define __SUBCOMPONENT__ "ds-devio"
#define __LOGNAME__ "ds.log"

static void ds_dev_io_free(struct ds_dev_io *io)
{
	kfree(io->bio->bi_io_vec);
	kfree(io->bio);
	if (io->complete)
		kfree(io->complete);
}

static void ds_dev_io_bio_end(struct bio *bio, int err)
{
	struct ds_dev_io *io = (struct ds_dev_io *)bio->bi_private;
	
	BUG_ON(bio != io->bio);
	io->err = err;

	klog(KL_DBG, "bio %p io %p dev %p err %d", bio, io, io->dev, err);

	if (io->clb)
		io->clb(io);

	spin_lock_irq(&io->dev->io_lock);
	list_del(&io->io_list);
	spin_unlock_irq(&io->dev->io_lock);


	if (io->complete)
		complete(io->complete);
	else
		ds_dev_io_free(io);
}

int ds_dev_io_page(struct ds_dev *dev, struct page *page, __u64 off,
	int bi_flags, int rw_flags, void (*clb)(struct ds_dev_io *io), int wait)
{
	struct bio *bio;
	struct bio_vec *bio_vec;
	struct ds_dev_io *io;
	int err;

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
	if (wait) {
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
	io->clb = clb;

	bio_init(bio);

	bio->bi_io_vec = bio_vec;
	bio->bi_io_vec->bv_page = page;
	bio->bi_io_vec->bv_len = PAGE_SIZE;
	bio->bi_io_vec->bv_offset = 0;

	bio->bi_vcnt = 1;
	bio->bi_iter.bi_size = PAGE_SIZE;
	bio->bi_iter.bi_sector = off >> SECTOR_SHIFT;
	bio->bi_bdev = dev->bdev;
	bio->bi_flags |= bi_flags;
	bio->bi_rw |= rw_flags;
	bio->bi_private = io;
	bio->bi_end_io = ds_dev_io_bio_end;
	
	spin_lock_irq(&dev->io_lock);
	list_add_tail(&io->io_list, &dev->io_list);
	spin_unlock_irq(&dev->io_lock);

	klog(KL_DBG, "bio %p io %p queued dev %p", bio, io, dev);
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
		klog(KL_ERR, "cant alloc page");
		return -ENOMEM;
	}

	err = ds_dev_io_page(dev, page, 0, 0, 0, NULL, 1);
	if (err) {
		klog(KL_ERR, "ds_dev_io_page err %d", err);
		goto out;
	}

	err = ds_dev_io_page(dev, page, 0, 0, REQ_WRITE, NULL, 1);
	if (err) {
		klog(KL_ERR, "ds_dev_io_page err %d", err);	
		goto out;
	}

	err = 0;
out:
	__free_page(page);
	return err;
}


