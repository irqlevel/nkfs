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
#include <linux/buffer_head.h>
#include <asm/uaccess.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <linux/delay.h>
#include <linux/rbtree.h>
#include <net/sock.h>

#include <include/ds_cmd.h>
#include <include/ds_obj_id.h>
#include <include/ds_image.h>
#include <include/ds_net_cmd.h>

#include <crtlib/include/crtlib.h>

#include <inc/helpers.h>
#include <inc/klog.h>
#include <inc/ksocket.h>
#include <inc/btree.h>
#include <inc/amap.h>
#include <inc/dev.h>
#include <inc/dev_io.h>
#include <inc/net.h>
#include <inc/random.h>
#include <inc/vfile.h>
#include <inc/super.h>
#include <inc/balloc.h>

#define SECTOR_SHIFT 9

#define __BTREE_TEST__	0
#define __SHA_TEST__	0

#define DS_BUG_ON(cond)					\
	do {						\
		if (cond) {				\
			KLOG(KL_ERR, "BUG_ON()");	\
			klog_sync();			\
		}					\
		BUG_ON(cond);				\
	} while (0);					\

#define DS_BUG()				\
	do {					\
		if (cond) {			\
			KLOG(KL_ERR, "BUG()");	\
			klog_sync();		\
		}				\
		BUG();				\
	} whole (0);				\

