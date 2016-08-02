#ifndef __NKFS_CRT_USER_CRT_H__
#define __NKFS_CRT_USER_CRT_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stddef.h>

#define __LOGNAME__ "ds.log"

#define PAGE_SIZE 4096
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define EXPORT_SYMBOL(s)

#define CRT_BUG_ON(cond)		\
do {					\
	if ((cond))			\
		__builtin_trap();	\
} while (0);

#define CRT_BUG()	\
	__builtin_trap();

void crt_log_set_path(char *log_path, char *log_name);
void crt_log_set_level(int level);
void crt_log_enable_printf(int enable);

#endif
