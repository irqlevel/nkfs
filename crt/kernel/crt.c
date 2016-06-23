#include "crt.h"
#include "malloc_checker.h"
#include <crt/include/random.h>
#include <crt/include/nk8.h>

#define __SUBCOMPONENT__ "crt"
static struct file *dev_random;
static struct file *dev_urandom;
static struct workqueue_struct *crt_wq;

void *crt_malloc(size_t size)
{
	return crt_kmalloc(size, GFP_NOIO);
}
EXPORT_SYMBOL(crt_malloc);

void *crt_memset(void *ptr, int value, size_t len)
{
	return memset(ptr, value, len);
}
EXPORT_SYMBOL(crt_memset);

void *crt_memcpy(void *ptr1, const void *ptr2, size_t len)
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
	crt_kfree(ptr);
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

	va_start(args, fmt);
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

int crt_queue_work(work_func_t func)
{
	struct work_struct *work = NULL;

	work = crt_kmalloc(sizeof(*work), GFP_ATOMIC);
	if (!work) {
		KLOG(KL_ERR, "cant alloc work");
		return -ENOMEM;
	}

	INIT_WORK(work, func);
	if (!queue_work(crt_wq, work)) {
		crt_kfree(work);
		KLOG(KL_ERR, "cant queue work");
		return -ENOMEM;
	}
	return 0;
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

void *crt_kmalloc(size_t size, gfp_t flags)
{
#ifdef __MALLOC_CHECKER__
	return malloc_checker_kmalloc(size, flags);
#else
	return kmalloc(size, flags);
#endif
}
EXPORT_SYMBOL(crt_kmalloc);

void *crt_kcalloc(size_t n, size_t size, gfp_t flags)
{
#ifdef __MALLOC_CHECKER__
	return malloc_checker_kmalloc(n * size, flags);
#else
	return kmalloc(n * size, flags);
#endif
}
EXPORT_SYMBOL(crt_kcalloc);

void crt_kfree(void *ptr)
{
#ifdef __MALLOC_CHECKER__
	malloc_checker_kfree(ptr);
#else
	kfree(ptr);
#endif
}
EXPORT_SYMBOL(crt_kfree);

int crt_kmalloc_init(void)
{
#ifdef __MALLOC_CHECKER__
	return malloc_checker_init();
#else
	return 0;
#endif
}

void crt_kmalloc_deinit(void)
{
#ifdef __MALLOC_CHECKER__
	malloc_checker_deinit();
#endif
}

static int __init crt_init(void)
{
	int err = -EINVAL;

	pr_info("nkfs_crt: initing\n");

	err = crt_kmalloc_init();
	if (err)
		goto out;

	err = klog_init();
	if (err)
		goto rel_kmalloc;

	err = crt_random_init();
	if (err)
		goto rel_klog;

	rand_test();

	crt_wq = alloc_workqueue("crt_wq",
			WQ_MEM_RECLAIM|WQ_UNBOUND, 1);
	if (!crt_wq) {
		KLOG(KL_ERR, "cant create wq");
		err = -ENOMEM;
		goto rel_rnd;
	}

	KLOG(KL_INF, "nk8 initing");
	err = nk8_init();
	if (err) {
		KLOG(KL_ERR, "nk8 init err %d", err);
		goto del_wq;
	}

	KLOG(KL_INF, "inited");
	return 0;

del_wq:
	destroy_workqueue(crt_wq);
rel_rnd:
	crt_random_release();
rel_klog:
	klog_release();
rel_kmalloc:
	crt_kmalloc_deinit();
out:
	return err;
}

static void __exit crt_exit(void)
{
	KLOG(KL_INF, "exiting");
	destroy_workqueue(crt_wq);
	nk8_release();
	crt_random_release();
	klog_release();
	crt_kmalloc_deinit();
	pr_info("nkfs_crt: exited\n");
}

module_init(crt_init);
module_exit(crt_exit);
MODULE_LICENSE("GPL");
