#ifndef __NKFS_NEIGH_INFO_H__
#define __NKFS_NEIGH_INFO_H__

#include <include/nkfs_obj_id.h>
#include <include/nkfs_types.h>
#include <include/nkfs_const.h>

#pragma pack(push, 1)

struct nkfs_neigh_info {
	u32			ip;
	int			port;
	unsigned long		state;
	struct nkfs_obj_id	host_id;
	u64			hbt_time;
	u64			hbt_delay;
};

#pragma pack(pop)

#endif
