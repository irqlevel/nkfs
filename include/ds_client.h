#pragma once

#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <crt/include/crt.h>
#include <errno.h>
#include <include/ds_obj_id.h>
#include <include/types.h>

#pragma pack(push, 1)
struct ds_con {
	int sock;
	int err;
};
#pragma pack(pop)

int  ds_connect(struct ds_con *con, char *ip, int port);

void ds_close(struct ds_con *con);

int  ds_put_object(struct ds_con *con, struct ds_obj_id *id, u64 off, void *data, u32 data_size);

int  ds_get_object(struct ds_con *con,struct ds_obj_id *id, u64 off, void *data, u32 data_size);

int  ds_delete_object(struct ds_con *con,struct ds_obj_id *id);
