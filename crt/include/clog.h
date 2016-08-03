#ifndef __NKFS_CRT_CLOG_H__
#define __NKFS_CRT_CLOG_H__

#define CL_INV	0
#define CL_DBG3	1
#define CL_DBG2	2
#define CL_DBG1	3
#define CL_DBG	4
#define CL_INF	5
#define CL_WRN	6
#define CL_ERR	7
#define CL_FTL	8
#define CL_TST	9
#define CL_MAX	10

#ifdef __KERNEL__
#define CLOG_ENABLED	0
#else
#define CLOG_ENABLED	1
#endif

#define CLOG_SRC	1
#define CLOG_LEVEL	CL_DBG

#if CLOG_ENABLED
#if CLOG_SRC
#define CLOG(level, fmt, ...)  {	\
	if (level >= CLOG_LEVEL)	\
		crt_log(level, __FILE__, __LINE__, __FUNCTION__,\
				fmt, ##__VA_ARGS__);	\
}
#else
#define CLOG(level, fmt, ...)  {	\
	if (level >= CLOG_LEVEL)	\
		crt_log(level, "???", 0, "???", fmt, ##__VA_ARGS__);	\
}
#endif
#else
#define CLOG(level, fmt, ...)
#endif

#endif
