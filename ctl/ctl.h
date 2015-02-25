#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <memory.h>
#include <errno.h>
#include <malloc.h>

#include <include/nkfs_ctl.h>
#include <include/nkfs_const.h>
#include <crt/include/crt.h>

int nkfs_dev_add(const char *dev_name, int format);
int nkfs_dev_rem(const char *dev_name);

int nkfs_dev_query(const char *dev_name,
			struct nkfs_dev_info *info);

int nkfs_server_stop(u32 ip, int port);
int nkfs_server_start(u32 ip, int port);

int nkfs_neigh_add(u32 d_ip, int d_port, u32 s_ip, int s_port);
int nkfs_neigh_remove(u32 d_ip, int d_port);

int nkfs_klog_ctl(int level, int sync);
