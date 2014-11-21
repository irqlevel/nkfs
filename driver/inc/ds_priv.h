#pragma once
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
#include <linux/file.h>
#include <linux/sort.h>
#include <asm/uaccess.h>

#include <include/ds_cmd.h>
#include <include/ds_obj_id.h>
#include <include/ds_image.h>

#include <crtlib/include/crtlib.h>

#include <inc/klog.h>
#include <inc/ksocket.h>
#include <inc/btree.h>
#include <inc/amap.h>

#define __LOGNAME__ "ds.log"
#define SECTOR_SHIFT 9

struct ds_image {
	__u32	magic;
	__u32	version;
	__u64	size;
	struct ds_obj_id id;
};

struct ds_dev {
	char			*dev_name;
	struct list_head 	dev_list;
	struct block_device 	*bdev;
	struct task_struct  	*thread;
	struct list_head	io_list;
	spinlock_t		io_lock;
	int			fmode;
	int			stopping;
	struct	ds_image	*image;
};

struct ds_con {
	struct task_struct 	*thread;
	struct socket 		*sock;
	struct list_head	con_list;
	struct ds_server	*server;
};

struct ds_server {
	struct task_struct	*thread;
	struct socket		*sock;
	struct mutex		lock;
	struct list_head	srv_list;
	struct list_head	con_list;
	struct mutex		con_list_lock;
	int			port;
	int			stopping;
};

typedef void (*ds_dev_io_complete_t)(int err, struct ds_dev *dev,
		void *context, struct page *page, u64 off,
		int rw_flags);

struct ds_dev_io {
	struct ds_dev 		*dev;
	struct bio    		*bio;
	struct page		*page;
	void			*context;
	u64			off;
	struct list_head	io_list;
	ds_dev_io_complete_t	complete_clb;
	struct completion	*complete;
	int			rw_flags;
	int			err;
};

int ds_dev_io_touch0_page(struct ds_dev *dev);

int ds_dev_io_page(struct ds_dev *dev, void *context, struct page *page, u64 off,
		int rw_flags, int sync, ds_dev_io_complete_t complete_clb);

int ds_dev_add(char *dev_name, int format);
int ds_dev_remove(char *dev_name);
void ds_dev_release_all(void);
struct ds_dev *ds_dev_create(char *dev_name, int fmode);

int ds_server_start(int port);
int ds_server_stop(int port);
void ds_server_stop_all(void);

int ds_random_init(void);
void ds_random_release(void);
int ds_random_buf_read(void *buf, __u32 len, int urandom);

int file_write(struct file *file, const void *buf, u32 len, loff_t *off);
int file_sync(struct file *file);
int file_read(struct file *file, const void *buf, u32 len, loff_t *off);

struct ds_obj_id *ds_obj_id_gen(void);
char *ds_obj_id_to_str(struct ds_obj_id *id);

int ds_image_check(struct ds_dev *dev);
int ds_image_format(struct ds_dev *dev);
void ds_image_dev_free(struct ds_dev *dev);
void ds_image_dev_stop(struct ds_dev *dev);

