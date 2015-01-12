#pragma once

#include <include/types.h>

#define DS_NET_CMD_OBJ_CREATE	1
#define DS_NET_CMD_OBJ_DELETE	2
#define DS_NET_CMD_OBJ_PUT	3
#define DS_NET_CMD_OBJ_GET	4

#define DS_NET_CMD_SIGN	(0xFCFEFFFABEDABEADull)

#pragma pack(push, 1)
struct ds_net_cmd {
	u64			sign; /* here store and check sign */
	u64			unique; /*server should reply the same */
	struct ds_obj_id	obj_id;
	u64			obj_size;
	u64			off;
	u32			data_size;
	int			cmd;
	int			err;
};
#pragma pack(pop)
