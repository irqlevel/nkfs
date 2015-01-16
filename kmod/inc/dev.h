#pragma once

struct ds_dev {
	atomic_t		ref;
	struct list_head 	dev_list;
	struct block_device 	*bdev;
	struct task_struct  	*thread;
	struct list_head	io_list;
	spinlock_t		io_lock;
	int			fmode;
	int			stopping;
	int			bsize;
	struct	ds_sb		*sb;
	char			dev_name[DS_NAME_MAX_SZ]; 
};

int ds_dev_add(char *dev_name, int format);
int ds_dev_remove(char *dev_name);
int ds_dev_query(char *dev_name, struct ds_dev_info *info);
struct ds_dev *ds_dev_create(char *dev_name, int fmode);

void ds_dev_ref(struct ds_dev *dev);
void ds_dev_deref(struct ds_dev *dev);

struct ds_dev *ds_dev_lookup(char *dev_name);

int ds_dev_init(void);
void ds_dev_finit(void);
