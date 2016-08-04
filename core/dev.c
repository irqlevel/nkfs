#include "dev.h"
#include "super.h"
#include "dio.h"
#include "helpers.h"
#include "trace.h"

#include <include/nkfs_image.h>
#include <crt/include/crt.h>
#include <linux/fs.h>

static DEFINE_MUTEX(dev_list_lock);
static LIST_HEAD(dev_list);

static void nkfs_dev_free(struct nkfs_dev *dev)
{
	if (dev->sb)
		nkfs_sb_deref(dev->sb);

	crt_kfree(dev);
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
	if (dev->ddev)
		dio_dev_deref(dev->ddev);

	if (dev->bdev)
		blkdev_put(dev->bdev, dev->fmode);
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
	sb = dev->sb;
	if (sb) {
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
		return NULL;
	}

	dev = crt_kmalloc(sizeof(*dev), GFP_NOIO);
	if (!dev) {
		return NULL;
	}

	memset(dev, 0, sizeof(*dev));
	atomic_set(&dev->ref, 1);

	snprintf(dev->dev_name, sizeof(dev->dev_name), "%s", dev_name);

	dev->bdev = blkdev_get_by_path(dev->dev_name,
		fmode, dev);
	err = IS_ERR(dev->bdev);
	if (err) {
		nkfs_error(err, "Can't get device by path %s", dev->dev_name);
		dev->bdev = NULL;
		nkfs_dev_deref(dev);
		return NULL;
	}
	dev->fmode = fmode;
	dev->bsize = NKFS_BLOCK_SIZE;

	dev->ddev = dio_dev_create(dev->bdev, dev->bsize, 64);
	if (!dev->ddev) {
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
		return err;
	}

	err = nkfs_sb_insert(sb);
	if (err) {
		nkfs_sb_deref(sb);
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

	dev = nkfs_dev_create(dev_name, FMODE_READ|FMODE_WRITE|FMODE_EXCL);
	if (!dev)
		return -ENOMEM;

	err = nkfs_dev_insert(dev);
	if (err) {
		nkfs_dev_release(dev);
		nkfs_dev_deref(dev);
		return err;
	}

	err = nkfs_dev_start(dev, format);
	if (err) {
		nkfs_dev_unlink(dev);
		nkfs_dev_release(dev);
		nkfs_dev_deref(dev);
		return err;
	}

	return err;
}

int nkfs_dev_remove(char *dev_name)
{
	int err;
	struct nkfs_dev *dev;

	dev = nkfs_dev_lookup_unlink(dev_name);
	if (dev) {
		nkfs_dev_stop(dev);
		nkfs_dev_release(dev);
		nkfs_dev_deref(dev);
		err = 0;
	} else {
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
	return 0;
}

void nkfs_dev_finit(void)
{
	nkfs_dev_release_all();
}
