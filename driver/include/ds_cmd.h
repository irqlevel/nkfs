#pragma once
#include <linux/ioctl.h>

#define IOC_MAGIC 0xED000000

#define IOCTL_DS_DEV_ADD	_IO(IOC_MAGIC, 1)
#define IOCTL_DS_DEV_REMOVE	_IO(IOC_MAGIC, 2)
#define IOCTL_DS_SRV_START	_IO(IOC_MAGIC, 3)
#define IOCTL_DS_SRV_STOP	_IO(IOC_MAGIC, 4)

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
