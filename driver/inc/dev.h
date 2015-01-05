#pragma once

struct ds_dev {
	char			*dev_name;
	struct list_head 	dev_list;
	struct block_device 	*bdev;
	struct task_struct  	*thread;
	struct list_head	io_list;
	spinlock_t		io_lock;
	int			fmode;
	int			stopping;
	int			bsize;
	struct	ds_image	*image;
};

int ds_dev_add(char *dev_name, int format);
int ds_dev_remove(char *dev_name);
void ds_dev_release_all(void);
struct ds_dev *ds_dev_create(char *dev_name, int fmode);


