#pragma once

#include <include/ds_obj_id.h>
#include <include/ds_types.h>
#include <include/ds_obj_info.h>

#pragma pack(push, 1)
struct ds_con {
	int sock;
	int err;
};
#pragma pack(pop)

int  ds_connect(struct ds_con *con, char *ip, int port);
void ds_close(struct ds_con *con);

int  ds_put_object(struct ds_con *con, struct ds_obj_id *id, u64 off, void *data, u32 data_size);
int  ds_get_object(struct ds_con *con,struct ds_obj_id *id, u64 off,
	void *data, u32 data_size, u32 *pread);

int  ds_delete_object(struct ds_con *con,struct ds_obj_id *id);
int  ds_create_object(struct ds_con *con,struct ds_obj_id *id);

int ds_echo(struct ds_con *con);

int ds_query_object(struct ds_con *con, struct ds_obj_id *id,
	struct ds_obj_info *info);
