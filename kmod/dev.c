#include "inc/nkfs_priv.h"

#define __SUBCOMPONENT__ "dev"

static DEFINE_MUTEX(dev_list_lock);
static LIST_HEAD(dev_list);

static struct kmem_cache *nkfs_dev_cachep;

static void nkfs_dev_free(struct nkfs_dev *dev)
{
	if (dev->sb)
		nkfs_sb_deref(dev->sb);

	KLOG(KL_DBG, "dev %s %p sb %p",
		dev->dev_name, dev, dev->sb);

	kmem_cache_free(nkfs_dev_cachep, dev);
}

void nkfs_dev_ref(struct nkfs_dev *dev)
{
	atomic_inc(&dev->ref);
}

void nkfs_dev_deref(struct nkfs_dev *dev)
{
	NKFS_BUG_ON(atomic_read(&dev->ref) <= 0);
	if (atomic_dec_and_test(&dev->ref))
		nkfs_dev_free(dev);
}

static int nkfs_dev_insert(struct nkfs_dev *cand)
{
	struct nkfs_dev *dev;
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

static void nkfs_dev_release(struct nkfs_dev *dev)
{
	KLOG(KL_DBG, "releasing dev=%p bdev=%p", dev, dev->bdev);

	if (dev->ddev)
		dio_dev_deref(dev->ddev);

	if (dev->bdev)
		blkdev_put(dev->bdev, dev->fmode);
	KLOG(KL_INF, "released dev %s",
		dev->dev_name);
}

static void nkfs_dev_unlink(struct nkfs_dev *dev)
{
	mutex_lock(&dev_list_lock);
	list_del(&dev->dev_list);
	mutex_unlock(&dev_list_lock);
}

struct nkfs_dev *nkfs_dev_lookup(char *dev_name)
{
	struct nkfs_dev *dev;

	mutex_lock(&dev_list_lock);
	list_for_each_entry(dev, &dev_list, dev_list) {
		if (0 == strncmp(dev->dev_name, dev_name,
			strlen(dev_name)+1)) {
			nkfs_dev_ref(dev);
			mutex_unlock(&dev_list_lock);
			return dev;
		}
	}
	mutex_unlock(&dev_list_lock);
	return NULL;
}

int nkfs_dev_query(char *dev_name, struct nkfs_dev_info *info)
{
	struct nkfs_dev *dev;
	struct nkfs_sb *sb;

	dev = nkfs_dev_lookup(dev_name);
	if (!dev)
		return -ENOENT;

	memset(info, 0, sizeof(*info));
	if ((sb = dev->sb)) {
		nkfs_obj_id_copy(&info->sb_id, &sb->id);
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

	nkfs_dev_deref(dev);
	return 0;
}

static struct nkfs_dev *nkfs_dev_lookup_unlink(char *dev_name)
{
	struct nkfs_dev *dev;

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

struct nkfs_dev *nkfs_dev_create(char *dev_name, int fmode)
{
	struct nkfs_dev *dev;
	int len;
	int err;

	len = strlen(dev_name);
	if (len == 0 || len >= NKFS_NAME_MAX_SZ) {
		KLOG(KL_ERR, "len=%d", len);
		return NULL;
	}

	dev = kmem_cache_alloc(nkfs_dev_cachep, GFP_NOIO);
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
		nkfs_dev_deref(dev);
		return NULL;
	}
	dev->fmode = fmode;
	dev->bsize = NKFS_BLOCK_SIZE;

	dev->ddev = dio_dev_create(dev->bdev, dev->bsize, 64);
	if (!dev->ddev) {
		KLOG(KL_ERR, "cant create ddev");
		blkdev_put(dev->bdev, dev->fmode);
		dev->bdev = NULL;
		nkfs_dev_deref(dev);
		return NULL;
	}

	return dev;
}

static int nkfs_dev_start(struct nkfs_dev *dev, int format)
{
	int err;
	struct nkfs_sb *sb;

	NKFS_BUG_ON(dev->sb);
	if (!format)
		err = nkfs_sb_load(dev, &sb);
	else
		err = nkfs_sb_format(dev, &sb);

	if (err) {
		KLOG(KL_ERR, "check or format err %d", err);
		return err;
	}

	err = nkfs_sb_insert(sb);
	if (err) {
		nkfs_sb_deref(sb);
		KLOG(KL_ERR, "sb insert err=%d", err);
		return err;
	}

	dev->sb = sb;
	return 0;
}

static void nkfs_dev_stop(struct nkfs_dev *dev)
{
	dev->stopping = 1;
	nkfs_sb_stop(dev->sb);
}

int nkfs_dev_add(char *dev_name, int format)
{
	int err;
	struct nkfs_dev *dev;

	KLOG(KL_DBG, "inserting dev %s", dev_name);
	dev = nkfs_dev_create(dev_name, FMODE_READ|FMODE_WRITE|FMODE_EXCL);
	if (!dev) {
		return -ENOMEM;
	}

	err = nkfs_dev_insert(dev);
	if (err) {
		KLOG(KL_ERR, "nkfs_dev_insert err %d", err);
		nkfs_dev_release(dev);
		nkfs_dev_deref(dev);
		return err;
	}

	err = nkfs_dev_start(dev, format);
	if (err) {
		KLOG(KL_ERR, "nkfs_dev_insert err %d", err);
		nkfs_dev_unlink(dev);
		nkfs_dev_release(dev);
		nkfs_dev_deref(dev);
		return err;
	}

	KLOG(KL_INF, "inserted dev %s", dev->dev_name);

	return err;
}

int nkfs_dev_remove(char *dev_name)
{
	int err;
	struct nkfs_dev *dev;

	KLOG(KL_DBG, "removing dev %s", dev_name);
	dev = nkfs_dev_lookup_unlink(dev_name);
	if (dev) {
		nkfs_dev_stop(dev);
		nkfs_dev_release(dev);
		KLOG(KL_DBG, "removed dev %s %p sb %p",
			dev->dev_name, dev, dev->sb);
		nkfs_dev_deref(dev);
		err = 0;
	} else {
		KLOG(KL_ERR, "dev with name %s not found", dev_name);
		err = -ENOENT;
	}

	return err;
}

static void nkfs_dev_release_all(void)
{
	struct nkfs_dev *dev;
	struct nkfs_dev *tmp;
	mutex_lock(&dev_list_lock);
	list_for_each_entry_safe(dev, tmp, &dev_list, dev_list) {
		nkfs_dev_stop(dev);
		nkfs_dev_release(dev);
		list_del(&dev->dev_list);
		nkfs_dev_deref(dev);
	}
	mutex_unlock(&dev_list_lock);
}

int nkfs_dev_init(void)
{
	int err;

	nkfs_dev_cachep = kmem_cache_create("nkfs_dev_cache",
					    sizeof(struct nkfs_dev), 0,
					    SLAB_MEM_SPREAD, NULL);
	if (!nkfs_dev_cachep) {
		KLOG(KL_ERR, "cant create cache");
		err = -ENOMEM;
		goto out;
	}

	return 0;
out:
	return err;
}

void nkfs_dev_finit(void)
{
	nkfs_dev_release_all();
	kmem_cache_destroy(nkfs_dev_cachep);
}
