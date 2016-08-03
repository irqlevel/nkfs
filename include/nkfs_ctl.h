#ifndef __NKFS_CTL_H__
#define __NKFS_CTL_H__

#include <linux/ioctl.h>
#include <include/nkfs_obj_id.h>
#include <include/nkfs_dev_info.h>
#include <include/nkfs_neigh_info.h>
#include <include/nkfs_types.h>
#include <include/nkfs_const.h>

#define NKFS_CTL_DEV_NAME "nkfs_ctl"
#define NKFS_IOC_MAGIC 0xE1

#pragma pack(push, 1)

struct nkfs_ctl {
	int err;
	union {
		struct {
			char dev_name[NKFS_NAME_MAX_SZ];
			int format;
		} dev_add;
		struct {
			char dev_name[NKFS_NAME_MAX_SZ];
		} dev_remove;
		struct {
			char dev_name[NKFS_NAME_MAX_SZ];
			struct nkfs_dev_info info;
		} dev_query;
		struct  {
			u32 bind_ip;
			u32 ext_ip;
			int port;
		} server_start;
		struct {
			u32 bind_ip;
			int port;
		} server_stop;
		struct {
			u32 ip;
			int port;
		} neigh_add;
		struct {
			u32 ip;
			int port;
		} neigh_remove;
		struct {
			int			nr_neighs;
			struct nkfs_neigh_info	neighs[NKFS_ROUTE_MAX_NEIGHS];
		} neigh_info;
		struct {
			int sync;
			int level;
		} klog_ctl;
	} u;
};

#pragma pack(pop)

#define IOCTL_NKFS_DEV_ADD	_IOWR(NKFS_IOC_MAGIC, 1, struct nkfs_ctl *)
#define IOCTL_NKFS_DEV_REMOVE	_IOWR(NKFS_IOC_MAGIC, 2, struct nkfs_ctl *)
#define IOCTL_NKFS_DEV_QUERY	_IOWR(NKFS_IOC_MAGIC, 3, struct nkfs_ctl *)

#define IOCTL_NKFS_SRV_START	_IOWR(NKFS_IOC_MAGIC, 4, struct nkfs_ctl *)
#define IOCTL_NKFS_SRV_STOP	_IOWR(NKFS_IOC_MAGIC, 5, struct nkfs_ctl *)

#define IOCTL_NKFS_NEIGH_ADD	_IOWR(NKFS_IOC_MAGIC, 6, struct nkfs_ctl *)
#define IOCTL_NKFS_NEIGH_REMOVE	_IOWR(NKFS_IOC_MAGIC, 7, struct nkfs_ctl *)
#define IOCTL_NKFS_NEIGH_INFO	_IOWR(NKFS_IOC_MAGIC, 8, struct nkfs_ctl *)

#endif
