#ifndef __CRT_KERNEL_VFILE_H__
#define __CRT_KERNEL_VFILE_H__

#include <linux/fs.h>

int vfile_write(struct file *file, const void *buf, u32 len, loff_t *off);
int vfile_sync(struct file *file);
int vfile_read(struct file *file, const void *buf, u32 len, loff_t *off);

#endif
