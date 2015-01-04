#pragma once

#include <stdarg.h>
#define KL_INV		0
#define KL_DBG		1
#define KL_INF		2
#define KL_WRN		3
#define KL_ERR		4
#define KL_MAX		5

void klog(int level, const char *log_name, const char *subcomp, const char *file, int line, const char *func, const char *fmt, ...);
void klog_v(int level, const char *log_name, const char *subcomp, const char *file, int line, const char *func, const char *fmt, va_list args);

int klog_init(void);

void klog_release(void);

#define KL_LOG_ENABLED	1
#define KL_LOG_SRC	1
#define KL_LOG_LEVEL	KL_DBG

#if KL_LOG_ENABLED

#if KL_LOG_SRC

#define KLOG(level, fmt, ...) {					\
	if ((level) >= KL_LOG_LEVEL) {				\
		klog((level), __LOGNAME__, __SUBCOMPONENT__,	\
		__FILE__, __LINE__, __FUNCTION__,		\
		(fmt), ##__VA_ARGS__);				\
	}							\
}

#else

#define KLOG(level, fmt, ...) {					\
	if ((level) >= KL_LOG_LEVEL) {				\
		klog((level), __LOGNAME__, __SUBCOMPONENT__,	\
		" ", " ", " ",					\
		(fmt), ##__VA_ARGS__);				\
	}							\
}

#endif

#else

#define KLOG(level, fmt, ...)

#endif

