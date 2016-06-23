#pragma once

#include <linux/version.h>

static inline const char *truncate_file_name(const char *file_name)
{
	char *base;

	base = strrchr(file_name, '/');
	if (base)
		return ++base;
	else
		return file_name;
}

#define PRINTK(fmt, ...)    \
	pr_info("nkfs_crt: t%u %s,%d %s() " fmt, current->pid, \
		truncate_file_name(__FILE__), __LINE__,\
		__PRETTY_FUNCTION__, ##__VA_ARGS__)
