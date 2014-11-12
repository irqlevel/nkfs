#include "klog.h"
#include <linux/kernel.h>       /* Needed for KERN_INFO */
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/list.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/mempool.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/fs.h>

#include <stdarg.h>


#define KLOG_MSG_BYTES	200
#define KLOG_NAME_BYTES	20
#define KLOG_PATH "/var/log/"

struct klog_msg {
	struct list_head 	msg_list;
	int			level;
	char			log_name[KLOG_NAME_BYTES];
	char 			data[KLOG_MSG_BYTES];
};

static LIST_HEAD(klog_msg_list);
static DEFINE_SPINLOCK(klog_msg_lock);

static struct kmem_cache *klog_msg_cache;
static mempool_t *klog_msg_pool;

static long klog_stopping = 0;

static struct task_struct *klog_thread;
static DECLARE_WAIT_QUEUE_HEAD(klog_thread_wait);

static int klog_write_msg2(char **buff, int *left, const char *fmt, va_list args)
{
    	int res;

    	if (*left < 0)
        	return -1;

    	res = vsnprintf(*buff,*left,fmt,args);
    	if (res >= 0) {
        	*buff+=res;
        	*left-=res;
        	return 0;
    	} else {
        	return -2;
    	}
}

static int klog_write_msg(char **buff, int *left, const char *fmt, ...)
{
    	va_list args;
    	int res;

    	va_start(args,fmt);
    	res = klog_write_msg2(buff, left,fmt,args);
    	va_end(args);
    	return res;
}

static char * truncate_file_path(const char *filename)
{
    	char *temp, *curr = (char *)filename;
    	while((temp = strchr(curr,'/'))) {
    	    curr = ++temp;
    	}
    	return curr;
}

struct klog_msg * klog_msg_alloc(void)
{
	return mempool_alloc(klog_msg_pool, GFP_ATOMIC);
}

static void klog_msg_free(struct klog_msg *msg)
{
	mempool_free(msg, klog_msg_pool);
}

static void klog_msg_queue(struct klog_msg *msg)
{
	unsigned long irqf;
	int queued = 0;
	
	if (klog_stopping)
		return;

	spin_lock_irqsave(&klog_msg_lock, irqf);
	if (!klog_stopping) {
		list_add_tail(&msg->msg_list, &klog_msg_list);
		queued = 1;
	}
	spin_unlock_irqrestore(&klog_msg_lock, irqf);

	if (queued)
		wake_up_interruptible(&klog_thread_wait);
}

static void klog_msg_printk(struct klog_msg *msg)
{
	switch (msg->level) {
		case KL_INF_L: 
    			printk(KERN_INFO "%s", msg->data);
			break;
		case KL_ERR_L:
    			printk(KERN_ERR "%s", msg->data);
			break;
		case KL_WRN_L:
    			printk(KERN_WARNING "%s", msg->data);
			break;
		case KL_DBG_L:
    			printk(KERN_DEBUG "%s", msg->data);
			break;
		default:	
	    		printk(KERN_INFO "%s", msg->data);
			break;
	}
}

static char *klog_full_path(char *log_name)
{
	char *full_path = NULL;
	int path_len = strlen(KLOG_PATH);
	int name_len = strlen(log_name);
	int len = path_len + name_len;
	
	full_path = kmalloc((len + 1)*sizeof(char), GFP_KERNEL);
	if (!full_path) {
		printk(KERN_ERR "klog : cant alloc mem for path");
		return NULL;
	}
	memcpy(full_path, KLOG_PATH, path_len*sizeof(char));
	memcpy(full_path + path_len, log_name, name_len*sizeof(char));
	full_path[len] = '\0';

	return full_path;
}

static void klog_msg_write(struct klog_msg *msg)
{
	struct file * file = NULL;
	loff_t pos = 0;
	int wrote;
	int size;
	int error;
	char *path = NULL;

	path = klog_full_path(msg->log_name);
	if (!path)
		return;
	
	file = filp_open(path, O_APPEND|O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR);
	if (!file) {
		printk(KERN_ERR "klog : cant open log file");
		goto cleanup;	
	}
	size = strlen(msg->data);	
	wrote = vfs_write(file, msg->data, size, &pos);
	if (wrote != size) {
		printk(KERN_ERR "klog : vfs_write result=%d, should be %d", wrote, size);
	}
	error = vfs_fsync(file, 0);
	if (error < 0)
		printk(KERN_ERR "klog : vfs_fsync err=%d", error);

	filp_close(file, NULL);

cleanup:
	kfree(path);
}

static void klog_msg_queue_process(void)
{
	struct klog_msg *msg = NULL;
	unsigned long irqf;

	for (;;) {
		if (list_empty(&klog_msg_list))
			break;
		msg = NULL;
		spin_lock_irqsave(&klog_msg_lock, irqf);
		if (!list_empty(&klog_msg_list)) {
			msg = list_first_entry(&klog_msg_list, struct klog_msg, msg_list);
			list_del(&msg->msg_list);		
		}
		spin_unlock_irqrestore(&klog_msg_lock, irqf);
		if (msg) {
			klog_msg_write(msg);
			klog_msg_free(msg);
		}
	} 
}

static char *klog_level_s[] = {"INV", "DBG", "INF" , "WRN" , "ERR", "MAX"};

static int klog_level = KL_INV_L;

void klog(int level, const char *log_name, const char *subcomp, const char *file, int line, const char *func, const char *fmt, ...)
{
	
    	struct klog_msg *msg = NULL;
    	char *pos;
    	int left, count, len, log_name_count;
    	va_list args;
    	struct timespec ts;
	struct tm tm;
	char *level_s;
	
	if (level <= KL_INV_L || level >= KL_MAX_L) {
		printk(KERN_ERR "klog : invalid level=%d", level);
		return;
	}	

	if (level < 0 || level >= sizeof(klog_level_s)/sizeof(klog_level_s[0])) {
		printk(KERN_ERR "klog : invalid level=%d", level);
		return;
	}

	if (level < klog_level)
		return;

	level_s = klog_level_s[level];
	
	if (klog_stopping) {
		printk(KERN_ERR "klog : stopping");
		return;
	}

	msg = klog_msg_alloc();
	if (!msg) {
		printk(KERN_ERR "klog: cant alloc msg");
		return;
	}

	log_name_count = sizeof(msg->log_name)/sizeof(char);
	snprintf(msg->log_name, log_name_count-1, "%s", log_name);
	msg->log_name[log_name_count-1] = '\0';
 
	pos = msg->data;
	count = sizeof(msg->data)/sizeof(char);
	left = count - 1;

    	getnstimeofday(&ts);
	time_to_tm(ts.tv_sec, 0, &tm);

    	klog_write_msg(&pos,&left,"%04d-%02d-%02d %02d:%02d:%02d.%.9ld - %s - %s - %d - %s %d %s() - ", 1900+tm.tm_year, tm.tm_mon+1, tm.tm_mday, 
			tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec, level_s, subcomp, current->pid, truncate_file_path(file), line, func);

    	va_start(args,fmt);
    	klog_write_msg2(&pos,&left,fmt,args);
    	va_end(args);
	
    	msg->data[count-1] = '\0';

	len = strlen(msg->data);
	if (len == (count -1)) {
		msg->data[len-1] = '\n';
		msg->data[len] = '\0';
	} else {
		msg->data[len] = '\n';
		msg->data[len+1] = '\0';
	}

	klog_msg_printk(msg);
	klog_msg_queue(msg);	
}

static int klog_thread_routine(void *data)
{
	while (!kthread_should_stop()) {
		wait_event_interruptible_timeout(klog_thread_wait, (!list_empty(&klog_msg_list)) || kthread_should_stop(), msecs_to_jiffies(100));
		klog_msg_queue_process();	
	}

	klog_msg_queue_process();	
	return 0;
}

int klog_init(int level)
{
	int error = -EINVAL;

	klog_msg_cache = kmem_cache_create("klog_msg_cache", sizeof(struct klog_msg), 0, SLAB_HWCACHE_ALIGN, NULL);
	if (klog_msg_cache == NULL) {
		printk(KERN_ERR "klog: cant create mem_cache");
		error = -ENOMEM;
		goto out;
	}
	klog_msg_pool = mempool_create_slab_pool(1024, klog_msg_cache);
	if (klog_msg_pool == NULL) {
		printk(KERN_ERR "klog: cant create mempool");
		error = -ENOMEM;
		goto out_cache_del;
	}
	
	klog_thread = kthread_create(klog_thread_routine, NULL, "klogger");
	if (IS_ERR(klog_thread)) {
		error = PTR_ERR(klog_thread);
		printk(KERN_ERR "klog: cant create thread");
		goto out_pool_del;
	}
	klog_level = level;
	wake_up_process(klog_thread);

	return 0;
out_pool_del:
	mempool_destroy(klog_msg_pool);
out_cache_del:
	kmem_cache_destroy(klog_msg_cache);	
out:

	return error;
}

void klog_release(void)
{
	klog_stopping = 1;	
	kthread_stop(klog_thread);
	mempool_destroy(klog_msg_pool);
	kmem_cache_destroy(klog_msg_cache);
	klog_level = KL_INV_L;
}

