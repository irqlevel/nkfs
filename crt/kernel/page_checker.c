#include "page_checker.h"
#include "helpers.h"
#include "malloc.h"

#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/crc32.h>
#include <linux/stacktrace.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/highmem.h>

#define PAGE_CHECKER_STACK_ENTRIES 10
#define PAGE_CHECKER_NR_LISTS 9973
#define PAGE_CHECKER_SIGN1 0xBEDABEDA
#define PAGE_CHECKER_SIGN2 0xCBDACBDA

struct page_entry {
	struct list_head link;
	gfp_t flags;
	struct page *page;
#ifdef __PAGE_CHECKER_STACK_TRACE__
	struct stack_trace stack;
	unsigned long stack_entries[PAGE_CHECKER_STACK_ENTRIES];
#endif
#ifdef __PAGE_CHECKER_DELAY_FREE__
	u32 crc32;
	ktime_t time_to_live;
#endif
};

struct page_checker {
	struct list_head entries_list[PAGE_CHECKER_NR_LISTS];
	spinlock_t	 entries_list_lock[PAGE_CHECKER_NR_LISTS];
	atomic64_t	 nr_allocs;
	atomic64_t	 nr_frees;
#ifdef __PAGE_CHECKER_DELAY_FREE__
	struct task_struct *delay_check_thread;
	struct list_head delay_entries_list[PAGE_CHECKER_NR_LISTS];
	spinlock_t	 delay_entries_list_lock[PAGE_CHECKER_NR_LISTS];
#endif
};

static struct page_checker g_page_checker;

#ifdef __PAGE_CHECKER_DELAY_FREE__

static void release_entry(struct page_checker *checker,
			  struct page_entry *entry)
{
	struct page *page = entry->page;

#ifdef __PAGE_CHECKER_FILL_CC__
	{
		void *va;

		va = kmap_atomic(page);
		memset(va, 0xCC, PAGE_SIZE);
		kunmap_atomic(va);
	}
#endif

#ifdef __PAGE_CHECKER_PRINTK__
	PRINTK("free entry %p page %p\n", entry, entry->page);
#endif

	put_page(entry->page);
	crt_kfree(entry);
}

static void delay_check(struct page_checker *checker,
			struct page_entry *entry)
{
	void *va;
	u32 crc32;

	va = kmap_atomic(entry->page);
	crc32 = crc32_le(~0, va, PAGE_SIZE);
	kunmap_atomic(va);

	BUG_ON(entry->crc32 != crc32);

#ifdef __PAGE_CHECKER_PRINTK__
	PRINTK("delay check entry %p page %p\n", entry, entry->page);
#endif
}

static int page_checker_delay_thread(void *data)
{
	struct page_checker *checker = (struct page_checker *)data;
	unsigned long irq_flags;
	struct list_head free_list;
	struct page_entry *curr, *tmp;
	unsigned long i;

	PRINTK("starting\n");

	while (!kthread_should_stop()) {
		msleep(100);

		INIT_LIST_HEAD(&free_list);
		for (i = 0; i < ARRAY_SIZE(checker->delay_entries_list); i++) {

			INIT_LIST_HEAD(&free_list);
			spin_lock_irqsave(&checker->delay_entries_list_lock[i],
					  irq_flags);
			list_for_each_entry_safe(curr, tmp,
				&checker->delay_entries_list[i],
				link) {
				delay_check(checker, curr);
				if (ktime_compare(curr->time_to_live,
						  ktime_get()) >= 0) {
					list_del(&curr->link);
					list_add(&curr->link, &free_list);
				}
			}
			spin_unlock_irqrestore(
				&checker->delay_entries_list_lock[i],
				irq_flags);
		}

		list_for_each_entry_safe(curr, tmp, &free_list, link) {
			list_del_init(&curr->link);
			release_entry(checker, curr);
		}
	}

	PRINTK("stopping\n");

	return 0;
}
#endif

int page_checker_init(void)
{
	struct page_checker *checker = &g_page_checker;
	unsigned long i;

	PRINTK("page checker init\n");

	atomic64_set(&checker->nr_allocs, 0);
	atomic64_set(&checker->nr_frees, 0);

	for (i = 0; i < ARRAY_SIZE(checker->entries_list); i++) {
		INIT_LIST_HEAD(&checker->entries_list[i]);
		spin_lock_init(&checker->entries_list_lock[i]);
	}

#ifdef __PAGE_CHECKER_DELAY_FREE__
	{
		struct task_struct *thread;

		for (i = 0; i < ARRAY_SIZE(checker->delay_entries_list); i++) {
			INIT_LIST_HEAD(&checker->delay_entries_list[i]);
			spin_lock_init(&checker->delay_entries_list_lock[i]);
		}

		thread = kthread_create(page_checker_delay_thread, checker,
					"%s", "nkfs_crt_page_checker_thread");
		if (IS_ERR(thread))
			return PTR_ERR(thread);

		get_task_struct(thread);
		checker->delay_check_thread = thread;
		wake_up_process(thread);
	}
#endif
	return 0;
}

struct page *page_checker_alloc_page(gfp_t flags)
{
	struct page_checker *checker = &g_page_checker;
	struct page_entry *entry;
	unsigned long i;
	unsigned long irq_flags;
	struct page *page;

	page = alloc_page(flags);
	if (!page)
		return NULL;

	entry = crt_kmalloc(sizeof(*entry), flags);
	if (!entry) {
		put_page(page);
		return NULL;
	}

	memset(entry, 0, sizeof(*entry));
	entry->page = page;
	entry->flags = flags;
	INIT_LIST_HEAD(&entry->link);

#ifdef __PAGE_CHECKER_STACK_TRACE__
	entry->stack.nr_entries = 0;
	entry->stack.max_entries = ARRAY_SIZE(entry->stack_entries);
	entry->stack.entries = entry->stack_entries;
	entry->stack.skip = 2;
	save_stack_trace(&entry->stack);
#endif

	i = hash_pointer(page) % ARRAY_SIZE(checker->entries_list);
	spin_lock_irqsave(&checker->entries_list_lock[i], irq_flags);
	list_add(&entry->link, &checker->entries_list[i]);
	spin_unlock_irqrestore(&checker->entries_list_lock[i], irq_flags);

	atomic64_inc(&checker->nr_allocs);

#ifdef __PAGE_CHECKER_PRINTK__
	PRINTK("alloc entry %p page %p\n", entry, entry->page);
#endif

	return page;
}

static void check_and_release_entry(struct page_checker *checker,
				    struct page_entry *entry)
{
#ifdef __PAGE_CHECKER_FILL_CC__
	{
		void *va;

		va = kmap_atomic(entry->page);
		memset(va, 0xCC, PAGE_SIZE);
		kunmap_atomic(va);
	}
#endif

#ifdef __PAGE_CHECKER_PRINTK__
	PRINTK("free entry %p page %p\n", entry, entry->page);
#endif

#ifdef __PAGE_CHECKER_DELAY_FREE__
	{
		void *va;

		va = kmap_atomic(entry->page);
		entry->crc32 = crc32_le(~0, va, PAGE_SIZE);
		kunmap_atomic(va);
	}

	entry->time_to_live = ktime_add_ns(ktime_get(), 1000000000);
	{
		unsigned long irq_flags;
		unsigned long i;

		i = hash_pointer(entry->page) %
			ARRAY_SIZE(checker->delay_entries_list);
		spin_lock_irqsave(&checker->delay_entries_list_lock[i],
				  irq_flags);
		list_add(&entry->link, &checker->delay_entries_list[i]);
		spin_unlock_irqrestore(&checker->delay_entries_list_lock[i],
				       irq_flags);
	}
#else
	put_page(entry->page);
	crt_kfree(entry);
#endif
}

void page_checker_free_page(struct page *page)
{
	struct page_checker *checker = &g_page_checker;
	unsigned long i;
	unsigned long irq_flags;
	struct page_entry *curr, *tmp;
	struct list_head entries_list;

	WARN_ON(atomic_read(&page->_count) != 1);

	INIT_LIST_HEAD(&entries_list);
	i = hash_pointer(page) % ARRAY_SIZE(checker->entries_list);
	spin_lock_irqsave(&checker->entries_list_lock[i], irq_flags);
	list_for_each_entry_safe(curr, tmp, &checker->entries_list[i], link) {
		if (curr->page == page) {
			list_del(&curr->link);
			list_add(&curr->link, &entries_list);
		}
	}

	spin_unlock_irqrestore(&checker->entries_list_lock[i], irq_flags);

	list_for_each_entry_safe(curr, tmp, &entries_list, link) {
		list_del_init(&curr->link);
		check_and_release_entry(checker, curr);
		atomic64_inc(&checker->nr_frees);
	}
}

void page_checker_deinit(void)
{
	unsigned long i;
	unsigned long irq_flags;
	struct list_head entries_list;
	struct page_entry *curr, *tmp;
	struct page_checker *checker = &g_page_checker;

	PRINTK("page checker deinit: nr_allocs %ld nr_frees %ld\n",
	       atomic64_read(&checker->nr_allocs),
	       atomic64_read(&checker->nr_frees));

#ifdef __PAGE_CHECKER_DELAY_FREE__
	kthread_stop(checker->delay_check_thread);
	put_task_struct(checker->delay_check_thread);
#endif

	for (i = 0; i < ARRAY_SIZE(checker->entries_list); i++) {
		INIT_LIST_HEAD(&entries_list);
		spin_lock_irqsave(&checker->entries_list_lock[i], irq_flags);
		list_for_each_entry_safe(curr, tmp, &checker->entries_list[i],
					 link) {
			list_del(&curr->link);
			list_add(&curr->link, &entries_list);
		}
		spin_unlock_irqrestore(&checker->entries_list_lock[i],
				       irq_flags);

		list_for_each_entry_safe(curr, tmp, &entries_list, link) {
			list_del_init(&curr->link);
			PRINTK("leak entry %p page %p flags 0x%x\n",
			       curr, curr->page, curr->flags);
#ifdef __PAGE_CHECKER_STACK_TRACE__
			{
				char stack[512];

				snprint_stack_trace(stack, sizeof(stack),
						    &curr->stack, 0);
				stack[ARRAY_SIZE(stack) - 1] = '\0';
				PRINTK("leak entry stack %s\n", stack);
			}
#endif
			check_and_release_entry(checker, curr);
		}
	}

#ifdef __PAGE_CHECKER_DELAY_FREE__
	for (i = 0; i < ARRAY_SIZE(checker->delay_entries_list); i++) {
		INIT_LIST_HEAD(&entries_list);
		spin_lock_irqsave(&checker->delay_entries_list_lock[i],
				  irq_flags);
		list_for_each_entry_safe(curr, tmp,
				&checker->delay_entries_list[i],
				link) {
			list_del(&curr->link);
			list_add(&curr->link, &entries_list);
		}

		spin_unlock_irqrestore(&checker->delay_entries_list_lock[i],
				       irq_flags);

		list_for_each_entry_safe(curr, tmp, &entries_list, link) {
			list_del_init(&curr->link);
			delay_check(checker, curr);
			release_entry(checker, curr);
		}
	}
#endif
}
