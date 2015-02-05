#include <inc/ds_priv.h>

#define __SUBCOMPONENT__ "dev"

static DEFINE_MUTEX(dev_list_lock);
static LIST_HEAD(dev_list);

static struct kmem_cache *ds_dev_cachep;

static void ds_dev_free(struct ds_dev *dev)
{
	if (dev->sb)
		ds_sb_deref(dev->sb);

	KLOG(KL_DBG, "dev %s %p sb %p",
		dev->dev_name, dev, dev->sb);

	kmem_cache_free(ds_dev_cachep, dev);
}

void ds_dev_ref(struct ds_dev *dev)
{
	atomic_inc(&dev->ref);
}

void ds_dev_deref(struct ds_dev *dev)
{
	BUG_ON(atomic_read(&dev->ref) <= 0);
	if (atomic_dec_and_test(&dev->ref))
		ds_dev_free(dev);
}

static int ds_dev_insert(struct ds_dev *cand)
{
	struct ds_dev *dev;
	int err = 0;

	mutex_lock(&dev_list_lock);
	list_for_each_entry(dev, &dev_list, dev_list) {
		if (0 == strncmp(dev->dev_name, cand->dev_name,
			strlen(cand->dev_name)+1)) {
			err = -EEXIST;
			break;
		}
	}
	if (!err)
		list_add_tail(&cand->dev_list, &dev_list);
	mutex_unlock(&dev_list_lock);
	return err;
}

static void ds_dev_release(struct ds_dev *dev)
{
	KLOG(KL_DBG, "releasing dev=%p bdev=%p", dev, dev->bdev);

	if (dev->ddev)
		dio_dev_deref(dev->ddev);

	if (dev->bdev)
		blkdev_put(dev->bdev, dev->fmode);
	KLOG(KL_INF, "released dev %s",
		dev->dev_name);
}

static void ds_dev_unlink(struct ds_dev *dev)
{
	mutex_lock(&dev_list_lock);
	list_del(&dev->dev_list);
	mutex_unlock(&dev_list_lock);
}

struct ds_dev *ds_dev_lookup(char *dev_name)
{
	struct ds_dev *dev;

	mutex_lock(&dev_list_lock);
	list_for_each_entry(dev, &dev_list, dev_list) {
		if (0 == strncmp(dev->dev_name, dev_name,
			strlen(dev_name)+1)) {
			ds_dev_ref(dev);
			mutex_unlock(&dev_list_lock);
			return dev;
		}
	}
	mutex_unlock(&dev_list_lock);
	return NULL;
}

int ds_dev_query(char *dev_name, struct ds_dev_info *info)
{
	struct ds_dev *dev;
	struct ds_sb *sb;

	dev = ds_dev_lookup(dev_name);
	if (!dev)
		return -ENOENT;

	memset(info, 0, sizeof(*info));
	if ((sb = dev->sb)) {
		ds_obj_id_copy(&info->sb_id, &sb->id);
		info->size = sb->size;
		info->blocks = sb->nr_blocks;
		info->used_blocks = atomic64_read(&sb->used_blocks);
		info->inodes_tree_block = sb->inodes_tree_block;
		info->bm_block = sb->bm_block;
	 	info->bm_blocks = sb->bm_blocks;
		info->bsize = sb->bsize;
		info->used_size = sb->bsize*atomic64_read(&sb->used_blocks);
		info->free_size = info->size - info->used_size;
	}

	snprintf(info->dev_name, sizeof(info->dev_name),
		"%s", dev->dev_name);

	if (dev->bdev) {
		info->major = MAJOR(dev->bdev->bd_dev);
		info->minor = MINOR(dev->bdev->bd_dev);
	}

	ds_dev_deref(dev);
	return 0;	
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
	if (len == 0 || len >= DS_NAME_MAX_SZ) {
		KLOG(KL_ERR, "len=%d", len);
		return NULL;
	}

	dev = kmem_cache_alloc(ds_dev_cachep, GFP_NOIO);
	if (!dev) {
		KLOG(KL_ERR, "dev alloc failed");
		return NULL;
	}

	memset(dev, 0, sizeof(*dev));
	atomic_set(&dev->ref, 1);

	snprintf(dev->dev_name, sizeof(dev->dev_name), "%s", dev_name);

	dev->bdev = blkdev_get_by_path(dev->dev_name,
		fmode, dev);
	if ((err = IS_ERR(dev->bdev))) {
		dev->bdev = NULL;
		KLOG(KL_ERR, "bkdev_get_by_path failed err %d", err);
		ds_dev_deref(dev);
		return NULL;
	}
	dev->fmode = fmode;
	dev->bsize = DS_BLOCK_SIZE;

	dev->ddev = dio_dev_create(dev->bdev, dev->bsize, 64);
	if (!dev->ddev) {
		KLOG(KL_ERR, "cant create ddev");	
		blkdev_put(dev->bdev, dev->fmode);
		dev->bdev = NULL;
		ds_dev_deref(dev);
		return NULL;
	}

	return dev;
}

static int ds_dev_start(struct ds_dev *dev, int format)
{
	int err;
	struct ds_sb *sb;

	BUG_ON(dev->sb);
	if (!format)
		err = ds_sb_load(dev, &sb);
	else
		err = ds_sb_format(dev, &sb);

	if (err) {
		KLOG(KL_ERR, "check or format err %d", err);
		return err;
	}

	err = ds_sb_insert(sb);
	if (err) {
		ds_sb_deref(sb);
		KLOG(KL_ERR, "sb insert err=%d", err);
		return err;
	}

	dev->sb = sb;
	return 0;
}

static void ds_dev_stop(struct ds_dev *dev)
{
	dev->stopping = 1;
	ds_sb_stop(dev->sb);
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
		ds_dev_deref(dev);
		return err;
	}

	err = ds_dev_start(dev, format);
	if (err) {
		KLOG(KL_ERR, "ds_dev_insert err %d", err);
		ds_dev_unlink(dev);		
		ds_dev_release(dev);
		ds_dev_deref(dev);
		return err;
	}

	KLOG(KL_INF, "inserted dev %s", dev->dev_name);

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
		KLOG(KL_DBG, "removed dev %s %p sb %p",
			dev->dev_name, dev, dev->sb);
		ds_dev_deref(dev);
		err = 0;
	} else {
		KLOG(KL_ERR, "dev with name %s not found", dev_name);
		err = -ENOENT;
	}

	return err;
}

static void ds_dev_release_all(void)
{
	struct ds_dev *dev;
	struct ds_dev *tmp;
	mutex_lock(&dev_list_lock);
	list_for_each_entry_safe(dev, tmp, &dev_list, dev_list) {
		ds_dev_stop(dev);
		ds_dev_release(dev);
		list_del(&dev->dev_list);
		ds_dev_deref(dev);
	}
	mutex_unlock(&dev_list_lock);
}

int ds_dev_init(void)
{
	int err;
	
	ds_dev_cachep = kmem_cache_create("ds_dev_cache", sizeof(struct ds_dev), 0,
			SLAB_MEM_SPREAD, NULL);
	if (!ds_dev_cachep) {
		KLOG(KL_ERR, "cant create cache");
		err = -ENOMEM;
		goto out;
	}

	return 0;
out:
	return err;
}

void ds_dev_finit(void)
{
	ds_dev_release_all();
	kmem_cache_destroy(ds_dev_cachep);
}
