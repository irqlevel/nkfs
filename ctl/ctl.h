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

#include <include/ds_ctl.h>
#include <include/ds_const.h>
#include <crt/include/crt.h>

int ds_dev_add(const char *dev_name, int format);
int ds_dev_rem(const char *dev_name);

int ds_dev_query(const char *dev_name,
			struct ds_dev_info *info);

int ds_get_object(struct ds_obj_id *obj_id,
		u64 off, void *buf, u32 len, u32 *pread);

int ds_put_object(struct ds_obj_id *obj_id,
		u64 off, void *buf, u32 len);

int ds_delete_object(struct ds_obj_id *obj_id);
int ds_create_object(struct ds_obj_id *obj_id);

int ds_server_stop(u32 ip, int port);
int ds_server_start(u32 ip, int port);

int ds_neigh_add(u32 ip, int port);
int ds_neigh_remove(u32 ip, int port);
