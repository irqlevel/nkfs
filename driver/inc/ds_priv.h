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
#include <include/ds_net_cmd.h>

#include <crtlib/include/crtlib.h>

#include <inc/klog.h>
#include <inc/ksocket.h>
#include <inc/btree.h>
#include <inc/amap.h>
#include <inc/dev.h>
#include <inc/dev_io.h>
#include <inc/net.h>
#include <inc/random.h>
#include <inc/file.h>
#include <inc/image.h>

#define __LOGNAME__ "ds.log"
#define SECTOR_SHIFT 9


