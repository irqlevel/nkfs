#pragma once
#include <linux/ioctl.h>

#define DS_IOC_MAGIC 0xE1 
#define DS_NAME_MAX_SZ 256

#pragma pack(push, 1)

struct ds_cmd {
	int err;
	union {
		struct {
			char dev_name[DS_NAME_MAX_SZ];
		} dev_add;
		struct {
			char dev_name[DS_NAME_MAX_SZ];
		} dev_remove;
		struct  {
			int port;
		} server_start;
		struct {
			int port;
		} server_stop;
	} u;
};

#pragma pack(pop)

#define IOCTL_DS_DEV_ADD	_IOWR(DS_IOC_MAGIC, 1, struct ds_cmd *)
#define IOCTL_DS_DEV_REMOVE	_IOWR(DS_IOC_MAGIC, 2, struct ds_cmd *)
#define IOCTL_DS_SRV_START	_IOWR(DS_IOC_MAGIC, 3, struct ds_cmd *)
#define IOCTL_DS_SRV_STOP	_IOWR(DS_IOC_MAGIC, 4, struct ds_cmd *)

