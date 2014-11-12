#include <ds_priv.h>

#define __SUBCOMPONENT__ "ds-file"

int file_write(struct file *file, const void *buf, u32 len, loff_t *off)
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


int file_sync(struct file *file)
{
	int err = vfs_fsync(file, 0);
	if (err) {
		klog(KL_ERR, "vfs_fsync err=%d", err);
	}
	return err;
}

int file_read(struct file *file, const void *buf, u32 len, loff_t *off)
{	
	int ret;
	mm_segment_t old_fs;
	u32 pos = 0;
	old_fs = get_fs();
	set_fs(get_ds());
	while (pos < len) {
		ret = vfs_read(file, (char *)buf + pos, len - pos, off);
		if (ret < 0) {
			klog(KL_ERR, "ret=%d, pos=%d, len=%d, off=%lld", ret, pos, len, *off);
			goto out;
		}

		if (ret == 0) {
			klog(KL_ERR, "ret=%d, pos=%d, len=%d, off=%lld", ret, pos, len, *off);	
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

