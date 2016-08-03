#include "crt.h"

#include <linux/fs.h>
#include <linux/uaccess.h>

int vfile_write(struct file *file, const void *buf, u32 len, loff_t *off)
{
	int ret;
	mm_segment_t old_fs;
	u32 pos = 0;

	old_fs = get_fs();
	set_fs(get_ds());
	while (pos < len) {
		ret = vfs_write(file, (char *)buf + pos, len - pos, off);
		if (ret < 0)
			goto out;
		if (ret == 0) {
			ret = -EIO;
			goto out;
		}
		pos += ret;
	}
	ret = 0;
out:
	set_fs(old_fs);
	return ret;
}


int vfile_sync(struct file *file)
{
	int err = vfs_fsync(file, 0);

	return err;
}

int vfile_read(struct file *file, const void *buf, u32 len, loff_t *off)
{
	int ret;
	mm_segment_t old_fs;
	u32 pos = 0;

	old_fs = get_fs();
	set_fs(get_ds());
	while (pos < len) {
		ret = vfs_read(file, (char *)buf + pos, len - pos, off);
		if (ret < 0) {
			goto out;
		}

		if (ret == 0) {
			ret = -EIO;
			goto out;
		}
		pos += ret;
	}
	ret = 0;
out:
	set_fs(old_fs);
	return ret;
}

