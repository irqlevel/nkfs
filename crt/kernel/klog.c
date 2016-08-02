#include "crt.h"
#include "helpers.h"

#include <linux/kthread.h>
#include <linux/fs.h>

#define KLOG_MSG_BYTES	256

struct klog_msg {
	struct list_head	list;
	struct completion	*comp;
	char			data[KLOG_MSG_BYTES];
	int			len;
};

static LIST_HEAD(klog_msg_list);
static DEFINE_SPINLOCK(klog_msg_lock);

static long klog_stopping;

static struct task_struct *klog_thread;
static DECLARE_WAIT_QUEUE_HEAD(klog_thread_wait);

static char *klog_level_s[] = {"INV", "DBG3", "DBG2", "DBG1", "DBG", "INF",
			       "WRN", "ERR", "FTL", "TST", "MAX"};

static int klog_write_msg2(char **buff, int *left, const char *fmt,
			   va_list args)
{
	int res;

	if (*left < 0)
		return -1;

	res = vsnprintf(*buff, *left, fmt, args);
	if (res >= 0) {
		*buff += res;
		*left -= res;
		return 0;
	}

	return -1;
}

static int klog_write_msg(char **buff, int *left, const char *fmt, ...)
{
	va_list args;
	int res;

	va_start(args, fmt);
	res = klog_write_msg2(buff, left, fmt, args);
	va_end(args);
	return res;
}

static atomic_t klog_nr_msg;

struct klog_msg *klog_msg_alloc(void)
{
	struct klog_msg *msg;

	msg = crt_kmalloc(sizeof(*msg), GFP_ATOMIC);
	if (msg) {
		memset(msg, 0, sizeof(*msg));
		atomic_inc(&klog_nr_msg);
	}
	return msg;
}

static void klog_msg_free(struct klog_msg *msg)
{
	crt_kfree(msg->comp);
	crt_kfree(msg);
	atomic_dec(&klog_nr_msg);
}

static int  klog_msg_queue(struct klog_msg *msg)
{
	unsigned long irqf;
	int queued = 0;

	if (klog_stopping) {
		pr_err("klog : stopping drop one msg\n");
		klog_msg_free(msg);
	}

	spin_lock_irqsave(&klog_msg_lock, irqf);
	if (!klog_stopping) {
		list_add_tail(&msg->list, &klog_msg_list);
		queued = 1;
	}
	spin_unlock_irqrestore(&klog_msg_lock, irqf);

	if (!queued) {
		pr_err("klog : stopping drop one msg\n");
		klog_msg_free(msg);
	} else {
		wake_up_interruptible(&klog_thread_wait);
	}

	return queued;
}

static int klog_file_sync(void)
{
	int err;
	struct file *file = NULL;

	file = filp_open(KLOG_PATH, O_APPEND|O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR);
	if (!file) {
		pr_err("klog : cant open log file\n");
		err = -EIO;
		goto cleanup;
	}

	err = vfile_sync(file);
	if (err) {
		pr_err("klog : vfile_sync err %d\n", err);
		goto cleanup;
	}

cleanup:
	if (file)
		filp_close(file, NULL);

	return err;
}

static void klog_file_write(void *buf, u32 len)
{
	struct file *file = NULL;
	loff_t pos = 0;
	int err;

	file = filp_open(KLOG_PATH, O_APPEND|O_WRONLY|O_CREAT,
			S_IRUSR|S_IWUSR);
	if (!file) {
		pr_err("klog : cant open log file\n");
		err = -EIO;
		goto cleanup;
	}

	err = vfile_write(file, buf, len, &pos);
	if (err) {
		pr_err("klog : vfile_write err %d\n", err);
		goto cleanup;
	}

cleanup:
	if (file)
		filp_close(file, NULL);
}

#define KLOG_BUF_MAX (64*1024)

static void klog_msg_list_write(struct list_head *msg_list)
{
	struct klog_msg *msg, *tmp;
	u32 size, pos;
	void *buf;

	size = 0;
	list_for_each_entry(msg, msg_list, list) {
		size += (msg->len + 1);
	}

	if (size > KLOG_BUF_MAX)
		size = KLOG_BUF_MAX;
	buf = crt_kmalloc(size, GFP_KERNEL);
	if (!buf) {
		pr_err("klog: cant alloc buf for klog\n");
		goto cleanup;
	}

	pos = 0;
	while (1) {
		list_for_each_entry_safe(msg, tmp, msg_list, list) {
			if ((pos + msg->len + 1) <= size) {
				memcpy((char *)buf + pos, msg->data, msg->len);
				memcpy((char *)buf + pos + msg->len, "\n", 1);
				pos += (msg->len + 1);
				list_del_init(&msg->list);
				klog_msg_free(msg);
			} else {
				break;
			}
		}

		if (pos > 0)
			klog_file_write(buf, pos);

		pos = 0;
		if (list_empty(msg_list))
			break;
	}

	crt_kfree(buf);
cleanup:
	list_for_each_entry_safe(msg, tmp, msg_list, list) {
		list_del_init(&msg->list);
		klog_msg_free(msg);
		pr_err("klog: dropped one msg for file log\n");
	}
}

static void klog_msg_queue_process(void)
{
	struct klog_msg *msg = NULL, *msg_comp = NULL;
	struct list_head msg_list;

	for (;;) {
		if (list_empty(&klog_msg_list))
			break;

		INIT_LIST_HEAD(&msg_list);
		msg_comp = NULL;
		spin_lock_irq(&klog_msg_lock);
		while (!list_empty(&klog_msg_list)) {
			msg = list_first_entry(&klog_msg_list,
					       struct klog_msg, list);
			list_del_init(&msg->list);
			if (msg->comp) {
				msg_comp = msg;
				break;
			}
			list_add_tail(&msg->list, &msg_list);
		}
		spin_unlock_irq(&klog_msg_lock);
		if (!list_empty(&msg_list))
			klog_msg_list_write(&msg_list);

		if (msg_comp) {
			klog_file_sync();
			pr_err("klog : synced\n");
			complete(msg_comp->comp);
		}
	}
}

static void klog_msg_list_drop(void)
{
	struct klog_msg *msg;

	while (!list_empty(&klog_msg_list)) {
		msg = list_first_entry(&klog_msg_list, struct klog_msg, list);
		list_del_init(&msg->list);
		if (msg->comp)
			complete(msg->comp);
		else
			klog_msg_free(msg);
	}
}

void klog_v(int level, const char *file, int line, const char *func,
	    const char *fmt, va_list args)
{
	struct klog_msg *msg = NULL;
	char *pos;
	int left, count;
	struct timespec ts;
	struct tm tm;
	char *level_s;

	if (level < 0 || level >= ARRAY_SIZE(klog_level_s)) {
		pr_err("klog : invalid level=%d\n", level);
		return;
	}

	level_s = klog_level_s[level];

	if (klog_stopping) {
		pr_err("klog : stopping\n");
		return;
	}

	msg = klog_msg_alloc();
	if (!msg) {
		pr_err("klog: cant alloc msg\n");
		return;
	}

	pos = msg->data;
	count = sizeof(msg->data)/sizeof(char);
	left = count - 1;

	getnstimeofday(&ts);
	time_to_tm(ts.tv_sec, 0, &tm);

	klog_write_msg(&pos, &left,
		"%04d-%02d-%02d %02d:%02d:%02d.%.06d %s t%d %s,%d %s() ",
		1900 + tm.tm_year, tm.tm_mon + 1, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec/1000, level_s,
		current->pid, truncate_file_name(file), line, func);

	klog_write_msg2(&pos, &left, fmt, args);

	msg->data[count-1] = '\0';
	msg->len = strlen(msg->data);
	if (level >= KLOG_PRINTK_LEVEL)
		pr_info("nkfs: %s\n", msg->data);
	klog_msg_queue(msg);
}
EXPORT_SYMBOL(klog_v);

void klog(int level, const char *file, int line, const char *func,
	  const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	klog_v(level, file, line, func, fmt, args);
	va_end(args);
}
EXPORT_SYMBOL(klog);

void klog_sync(void)
{
	struct klog_msg *msg;

	if (klog_stopping)
		return;

	msg = klog_msg_alloc();
	if (!msg)
		return;

	msg->comp = crt_kmalloc(sizeof(struct completion), GFP_ATOMIC);
	if (!msg->comp) {
		klog_msg_free(msg);
		return;
	}
	memset(msg->comp, 0, sizeof(struct completion));
	init_completion(msg->comp);
	if (klog_msg_queue(msg)) {
		wait_for_completion(msg->comp);
		klog_msg_free(msg);
	}
}
EXPORT_SYMBOL(klog_sync);

static int klog_thread_routine(void *data)
{
	while (!kthread_should_stop()) {
		wait_event_interruptible_timeout(klog_thread_wait,
			(!list_empty(&klog_msg_list)) ||
				kthread_should_stop(), msecs_to_jiffies(100));
		klog_msg_queue_process();
	}
	klog_msg_queue_process();
	klog_file_sync();
	return 0;
}

int klog_init(void)
{
	int error = -EINVAL;

	atomic_set(&klog_nr_msg, 0);

	klog_thread = kthread_create(klog_thread_routine, NULL, "klogger");
	if (IS_ERR(klog_thread)) {
		error = PTR_ERR(klog_thread);
		pr_err("klog: cant create thread\n");
		goto out;
	}
	wake_up_process(klog_thread);

	return 0;
out:
	return error;
}

void klog_release(void)
{
	klog_stopping = 1;
	kthread_stop(klog_thread);
	klog_msg_list_drop();
	klog_file_sync();
	BUG_ON(atomic_read(&klog_nr_msg));
}
