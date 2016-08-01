#pragma once

#include <stdarg.h>
#define KL_INV		0
#define KL_DBG3		1
#define KL_DBG2		2
#define KL_DBG1		3
#define KL_DBG		4
#define KL_INF		5
#define KL_WRN		6
#define KL_ERR		7
#define KL_FTL		8
#define KL_TST		9
#define KL_MAX		10

void klog(int level, const char *file, int line, const char *func,
	const char *fmt, ...);

void klog_v(int level, const char *file, int line, const char *func,
	const char *fmt, va_list args);

void klog_sync(void);

int klog_init(void);

void klog_release(void);

#define KLOG_PATH "/var/log/nkfs.log"
#define KLOG_ENABLED		1
#define KLOG_SRC		1
#define KLOG_LEVEL		KL_DBG
#define KLOG_PRINTK_LEVEL	KL_INF
#define KLOG_FILE		1

#if KLOG_ENABLED

#if KLOG_SRC

#define KLOG(level, fmt, ...)					\
do {								\
	if ((level) >= KLOG_LEVEL) {				\
		klog((level), __FILE__, __LINE__, __func__,	\
		     (fmt), ##__VA_ARGS__);			\
	}							\
} while (0)

#else

#define KLOG(level, fmt, ...)					\
do {								\
	if ((level) >= KLOG_LEVEL) {				\
		klog((level), "??", 0, "??",			\
		     (fmt), ##__VA_ARGS__);			\
	}							\
} while (0)

#endif

#else

#define KLOG(level, fmt, ...)

#endif

#define KLOG_SHA256_SUM(level, sum)		\
	if ((level) >= KLOG_LEVEL) {		\
		char *hsum;			\
		hsum = sha256_sum_hex(sum);	\
		KLOG((level), "sum %s", hsum);	\
		if (hsum)			\
			crt_free(hsum);		\
	}

#define KLOG_NKFS_BTREE_KEY(level, key)					\
if ((level) >= KLOG_LEVEL) {						\
	char *hex;							\
	hex = bytes_hex((void *)(key), sizeof(struct nkfs_btree_key));	\
	KLOG((level), "key %s", hex);					\
	if (hex)							\
		crt_free(hex);						\
}

#define KLOG_BUF_SUM(level, buf, len)				\
	if ((level) >= KLOG_LEVEL) {				\
		struct sha256_sum bsum;				\
		char *hsum;					\
		char *be = NULL, *en = NULL;			\
		u32 llen = (len > 8) ? 8 : len;			\
		sha256(buf, len, &bsum, 0);			\
		hsum = sha256_sum_hex(&bsum);			\
		be = bytes_hex(buf, llen);			\
		en = bytes_hex((char *)buf + len - llen, llen);	\
		KLOG(KL_DBG3, "b %p len %u sum %s be %s en %s",	\
			buf, len, hsum, be, en);		\
		if (hsum)					\
			crt_free(hsum);				\
		if (be)						\
			crt_free(be);				\
		if (en)						\
			crt_free(en);				\
	}

#define KLOG_SYNC()					\
	do {						\
		KLOG(KL_INF, "klog sync requested");	\
		klog_sync();				\
	} while (0);
