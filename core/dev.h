#ifndef __NKFS_CORE_DEV_H__
#define __NKFS_CORE_DEV_H__

#include <linux/atomic.h>

#include <include/nkfs_dev_info.h>

struct nkfs_dev {
	atomic_t		ref;
	struct list_head	dev_list;
	struct block_device	*bdev;
	int			fmode;
	int			stopping;
	int			bsize;
	struct	nkfs_sb		*sb;
	struct	dio_dev		*ddev;
	char			dev_name[NKFS_NAME_MAX_SZ];
};

int nkfs_dev_add(char *dev_name, int format);
int nkfs_dev_remove(char *dev_name);
int nkfs_dev_query(char *dev_name, struct nkfs_dev_info *info);
struct nkfs_dev *nkfs_dev_create(char *dev_name, int fmode);

void nkfs_dev_ref(struct nkfs_dev *dev);
void nkfs_dev_deref(struct nkfs_dev *dev);

struct nkfs_dev *nkfs_dev_lookup(char *dev_name);

int nkfs_dev_init(void);
void nkfs_dev_finit(void);

#endif
