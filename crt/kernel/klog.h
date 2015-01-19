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

void klog(int level, const char *subcomp, const char *file, int line, const char *func, const char *fmt, ...);
void klog_v(int level, const char *subcomp, const char *file, int line, const char *func, const char *fmt, va_list args);

void klog_sync(void);

int klog_init(void);
void klog_release(void);

#define KLOG_PATH "/var/log/ds.log"
#define KLOG_ENABLED		1
#define KLOG_SRC		1
#define KLOG_LEVEL		KL_DBG
#define KLOG_PRINTK_LEVEL	KL_INF
#define KLOG_FILE		1

#if KLOG_ENABLED

#if KLOG_SRC

#define KLOG(level, fmt, ...) {					\
	if ((level) >= KLOG_LEVEL) {				\
		klog((level), __SUBCOMPONENT__,			\
		__FILE__, __LINE__, __FUNCTION__,		\
		(fmt), ##__VA_ARGS__);				\
	}							\
}

#else

#define KLOG(level, fmt, ...) {					\
	if ((level) >= KLOG_LEVEL) {				\
		klog((level), __SUBCOMPONENT__,			\
		" ", " ", " ",					\
		(fmt), ##__VA_ARGS__);				\
	}							\
}

#endif

#else

#define KLOG(level, fmt, ...)

#endif

