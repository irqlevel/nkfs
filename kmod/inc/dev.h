#pragma once

struct ds_dev {
	atomic_t		ref;
	struct list_head 	dev_list;
	struct block_device 	*bdev;
	int			fmode;
	int			stopping;
	int			bsize;
	struct	ds_sb		*sb;
	struct	dio_dev		*ddev;
	char			dev_name[NKFS_NAME_MAX_SZ]; 
};

int ds_dev_add(char *dev_name, int format);
int ds_dev_remove(char *dev_name);
int ds_dev_query(char *dev_name, struct nkfs_dev_info *info);
struct ds_dev *ds_dev_create(char *dev_name, int fmode);

void ds_dev_ref(struct ds_dev *dev);
void ds_dev_deref(struct ds_dev *dev);

struct ds_dev *ds_dev_lookup(char *dev_name);

int ds_dev_init(void);
void ds_dev_finit(void);
