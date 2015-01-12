#include "crt.h"

static struct file *dev_random;
static struct file *dev_urandom;

void *crt_malloc(size_t size)
{
	return kmalloc(size, GFP_NOIO);
}
EXPORT_SYMBOL(crt_malloc);

void * crt_memset(void *ptr, int value, size_t len)
{
	return memset(ptr, value, len);
}
EXPORT_SYMBOL(crt_memset);

void * crt_memcpy(void *ptr1, const void *ptr2, size_t len)
{
	return memcpy(ptr1, ptr2, len);
}
EXPORT_SYMBOL(crt_memcpy);

int crt_memcmp(const void *ptr1, const void *ptr2, size_t len)
{
	return memcmp(ptr1, ptr2, len);
}
EXPORT_SYMBOL(crt_memcmp);

void crt_free(void *ptr)
{
	kfree(ptr);
}
EXPORT_SYMBOL(crt_free);

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

void crt_log(int level, const char *file, int line,
	const char *func, const char *fmt, ...)
{
	va_list args;
    	va_start(args,fmt);
    	klog_v(level, "crt", file, line, func, fmt, args);
    	va_end(args);
}

EXPORT_SYMBOL(crt_log);

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

static int __init crt_init(void)
{	
	int err = -EINVAL;
	err = klog_init();
	if (err)
		goto out;

	err = crt_random_init();
	if (err) {
		goto rel_klog;
	}
	printk("ds_crt: inited\n");
	return 0;
rel_klog:
	klog_release();
out:
	return err;
}

static void __exit crt_exit(void)
{
	crt_random_release();
	klog_release();
	printk("ds_crt: exited\n");
}

module_init(crt_init);
module_exit(crt_exit);
MODULE_LICENSE("GPL");
