#include <inc/ds_priv.h>

#define __SUBCOMPONENT__ "dev"

static DEFINE_MUTEX(dev_list_lock);
static LIST_HEAD(dev_list);

static void ds_dev_free(struct ds_dev *dev)
{
	if (dev->image)
		ds_image_delete(dev->image);
	if (dev->dev_name)
		kfree(dev->dev_name);
	kfree(dev);
}

static int ds_dev_insert(struct ds_dev *cand)
{
	struct ds_dev *dev;
	int err;

	mutex_lock(&dev_list_lock);
	list_for_each_entry(dev, &dev_list, dev_list) {
		if (0 == strncmp(dev->dev_name, cand->dev_name,
			strlen(cand->dev_name)+1)) {
			err = -EEXIST;
			break;
		}
	}
	list_add_tail(&cand->dev_list, &dev_list);
	err = 0;
	mutex_unlock(&dev_list_lock);
	return err;
}

static void ds_dev_release(struct ds_dev *dev)
{
	KLOG(KL_DBG, "releasing dev=%p bdev=%p", dev, dev->bdev);

	if (dev->bdev)
		blkdev_put(dev->bdev, dev->fmode);
}

static void ds_dev_unlink(struct ds_dev *dev)
{
	mutex_lock(&dev_list_lock);
	list_del(&dev->dev_list);
	mutex_unlock(&dev_list_lock);
}

static struct ds_dev *ds_dev_lookup_unlink(char *dev_name)
{
	struct ds_dev *dev;

	mutex_lock(&dev_list_lock);
	list_for_each_entry(dev, &dev_list, dev_list) {
		if (0 == strncmp(dev->dev_name, dev_name,
			strlen(dev_name)+1)) {
			list_del(&dev->dev_list);
			mutex_unlock(&dev_list_lock);
			return dev;
		}
	}
	mutex_unlock(&dev_list_lock);
	return NULL;
}

struct ds_dev *ds_dev_create(char *dev_name, int fmode)
{
	struct ds_dev *dev;
	int len;
	int err;

	len = strlen(dev_name);
	if (len == 0) {
		KLOG(KL_ERR, "len=%d", len);
		return NULL;
	}

	dev = kmalloc(sizeof(struct ds_dev), GFP_KERNEL);
	if (!dev) {
		KLOG(KL_ERR, "dev alloc failed");
		return NULL;
	}

	memset(dev, 0, sizeof(*dev));
	dev->dev_name = kmalloc(len + 1, GFP_KERNEL);
	if (!dev->dev_name) {
		KLOG(KL_ERR, "dev_name alloc failed");
		ds_dev_free(dev);
		return NULL;
	}
	spin_lock_init(&dev->io_lock);
	INIT_LIST_HEAD(&dev->io_list);

	memcpy(dev->dev_name, dev_name, len + 1);

	dev->bdev = blkdev_get_by_path(dev->dev_name,
		fmode, dev);
	if ((err = IS_ERR(dev->bdev))) {
		dev->bdev = NULL;
		KLOG(KL_ERR, "bkdev_get_by_path failed err %d", err);
		ds_dev_free(dev);
		
		return NULL;
	}
	dev->fmode = fmode;
	dev->bsize = PAGE_SIZE;

	return dev;
}

static int ds_dev_thread_routine(void *data)
{
	struct ds_dev *dev = (struct ds_dev *)data;
	int err = 0;

	KLOG(KL_DBG, "dev %p thread starting", dev);

	if (dev->thread != current)
		BUG_ON(1);

	KLOG(KL_DBG, "going to run main loop dev=%p", dev);
	while (!kthread_should_stop()) {
		msleep_interruptible(100);
		if (dev->stopping)
			break;
	}

	if (dev->image)
		ds_image_stop(dev->image);
	KLOG(KL_DBG, "dev %p exiting", dev);
	return err;
}

static int ds_dev_start(struct ds_dev *dev, int format)
{
	int err;

	BUG_ON(dev->image);
	if (!format)
		err = ds_image_load(dev, &dev->image);
	else
		err = ds_image_format(dev, &dev->image);

	if (err) {
		KLOG(KL_ERR, "check or format err %d", err);
		return err;
	}

	dev->thread = kthread_create(ds_dev_thread_routine, dev, "ds_dev_th");
	if (IS_ERR(dev->thread)) {
		err = PTR_ERR(dev->thread);
		dev->thread = NULL;
		KLOG(KL_ERR, "kthread_create err=%d", err);
		return err;
	}
	get_task_struct(dev->thread);
	wake_up_process(dev->thread);
	err = 0;
	return err;
}

static void ds_dev_stop(struct ds_dev *dev)
{
	dev->stopping = 1;
	if (dev->thread) {
		kthread_stop(dev->thread);
		put_task_struct(dev->thread);
	}
	while (!list_empty(&dev->io_list)) {
		msleep_interruptible(50);	
	}
}

int ds_dev_add(char *dev_name, int format)
{
	int err;
	struct ds_dev *dev;

	KLOG(KL_DBG, "inserting dev %s", dev_name);
	dev = ds_dev_create(dev_name, FMODE_READ|FMODE_WRITE|FMODE_EXCL);
	if (!dev) {
		return -ENOMEM;
	}

	err = ds_dev_insert(dev);
	if (err) {
		KLOG(KL_ERR, "ds_dev_insert err %d", err);
		ds_dev_release(dev);
		ds_dev_free(dev);
		return err;
	}

	err = ds_dev_start(dev, format);
	if (err) {
		KLOG(KL_ERR, "ds_dev_insert err %d", err);
		ds_dev_unlink(dev);		
		ds_dev_release(dev);
		ds_dev_free(dev);
		return err;
	}

	return err;
}

int ds_dev_remove(char *dev_name)
{
	int err;
	struct ds_dev *dev;

	KLOG(KL_DBG, "removing dev %s", dev_name);
	dev = ds_dev_lookup_unlink(dev_name);
	if (dev) {
		ds_dev_stop(dev);
		ds_dev_release(dev);
		ds_dev_free(dev);
		err = 0;
	} else {
		KLOG(KL_ERR, "dev with name %s not found", dev_name);
		err = -ENOENT;
	}

	return err;
}

void ds_dev_release_all(void)
{
	struct ds_dev *dev;
	struct ds_dev *tmp;
	mutex_lock(&dev_list_lock);
	list_for_each_entry_safe(dev, tmp, &dev_list, dev_list) {
		ds_dev_stop(dev);
		ds_dev_release(dev);
		list_del(&dev->dev_list);
		ds_dev_free(dev);
	}
	mutex_unlock(&dev_list_lock);
}

