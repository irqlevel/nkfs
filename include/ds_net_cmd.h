#pragma once

#include <stdint.h>
#include <crtlib/include/crtlib.h>

#define DS_NET_CMD_OBJ_CREATE	1
#define DS_NET_CMD_OBJ_DELETE	2
#define DS_NET_CMD_OBJ_PUT	3
#define DS_NET_CMD_OBJ_GET	4

struct ds_net_cmd {
	struct ds_obj_id	obj_id;
	uint64_t		obj_size;
	uint32_t		data_size;
	uint64_t		off;
	int			cmd;
	int			err;
};
