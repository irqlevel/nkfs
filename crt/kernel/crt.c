#include "crt.h"
#include "malloc.h"
#include "page_alloc.h"

#include <crt/include/random.h>
#include <crt/include/nk8.h>

#include <linux/fs.h>
#include <linux/file.h>
#include <linux/delay.h>
#include <linux/module.h>

static struct file *dev_random;
static struct file *dev_urandom;
static struct workqueue_struct *crt_wq;

static int crt_random_buf_read(void *buf, __u32 len, int urandom)
{
	loff_t off = 0;
	int err;

	err = vfile_read((urandom) ? dev_urandom : dev_random, buf, len, &off);
	return err;
}

int crt_random_buf(void *buf, size_t len)
{
	return crt_random_buf_read(buf, len, 1);
}
EXPORT_SYMBOL(crt_random_buf);

size_t crt_strlen(const char *s)
{
	return strlen(s);
}
EXPORT_SYMBOL(crt_strlen);

int crt_random_init(void)
{
	dev_random = filp_open("/dev/random", O_RDONLY, 0);
	if (!dev_random)
		return -ENOMEM;
	dev_urandom = filp_open("/dev/urandom", O_RDONLY, 0);
	if (!dev_urandom) {
		fput(dev_random);
		return -ENOMEM;
	}

	return 0;
}

void crt_random_release(void)
{
	fput(dev_random);
	fput(dev_urandom);
}

void crt_msleep(u32 ms)
{
	msleep_interruptible(ms);
}
EXPORT_SYMBOL(crt_msleep);

void *crt_file_open(char *path)
{
	return filp_open(path, O_APPEND|O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR);
}
EXPORT_SYMBOL(crt_file_open);

int crt_file_read(void *file, const void *buf, u32 len, loff_t *off)
{
	return vfile_read((struct file *)file, buf, len, off);
}
EXPORT_SYMBOL(crt_file_read);

int crt_file_write(void *file, const void *buf, u32 len, loff_t *off)
{
	return vfile_write((struct file *)file, buf, len, off);
}
EXPORT_SYMBOL(crt_file_write);

int crt_file_sync(void *file)
{
	return vfile_sync((struct file *)file);
}
EXPORT_SYMBOL(crt_file_sync);

void crt_file_close(void *file)
{
	filp_close((struct file *)file, NULL);
}
EXPORT_SYMBOL(crt_file_close);

static int __init crt_init(void)
{
	int err = -EINVAL;

	pr_info("nkfs_crt: initing\n");

	err = crt_kmalloc_init();
	if (err)
		goto out;

	err = crt_page_alloc_init();
	if (err)
		goto rel_kmalloc;

	err = crt_random_init();
	if (err)
		goto rel_page_alloc;

	rand_test();

	crt_wq = alloc_workqueue("crt_wq",
			WQ_MEM_RECLAIM|WQ_UNBOUND, 1);
	if (!crt_wq) {
		err = -ENOMEM;
		goto rel_rnd;
	}

	err = nk8_init();
	if (err) {
		goto del_wq;
	}

	pr_info("nkfs_crt: inited\n");
	return 0;

del_wq:
	destroy_workqueue(crt_wq);
rel_rnd:
	crt_random_release();
rel_page_alloc:
	crt_page_alloc_deinit();
rel_kmalloc:
	crt_kmalloc_deinit();
out:
	return err;
}

static void __exit crt_exit(void)
{
	pr_info("nkfs_crt: exiting\n");
	destroy_workqueue(crt_wq);
	nk8_release();
	crt_random_release();
	crt_page_alloc_deinit();
	crt_kmalloc_deinit();
	pr_info("nkfs_crt: exited\n");
}

module_init(crt_init);
module_exit(crt_exit);
MODULE_LICENSE("GPL");
