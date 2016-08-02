#ifndef __NKFS_HELPERS_H__
#define __NKFS_HELPERS_H__

#include <linux/module.h>
#include <linux/version.h>

static inline u64 nkfs_div_round_up(u64 x, u64 y)
{
	u64 tmp = x + y - 1;

	do_div(tmp, y);
	return tmp;
}

static inline u64 nkfs_div(u64 x, u64 y)
{
	u64 tmp = x;

	do_div(tmp, y);
	return tmp;
}

static inline u32 nkfs_mod(u64 x, u64 y)
{
	u64 tmp = x;

	return do_div(tmp, y);
}

#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 14, 0)

#define BIO_BI_SECTOR(bio)	((bio)->bi_sector)
#define BIO_BI_SIZE(bio)	((bio)->bi_size)

#else

#define BIO_BI_SECTOR(bio)	((bio)->bi_iter.bi_sector)
#define BIO_BI_SIZE(bio)	((bio)->bi_iter.bi_size)
#endif

#define NKFS_BUG_ON(cond)			\
do {						\
	if (cond) {				\
		KLOG(KL_ERR, "BUG_ON()");	\
		KLOG_SYNC();			\
	}					\
	BUG_ON(cond);				\
} while (0)					\

#define NKFS_BUG()	NKFS_BUG_ON(1)

#endif
