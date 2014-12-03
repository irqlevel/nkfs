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
#include <crtlib/include/crtlib.h>
#include <errno.h>
#include <utils/ucrt/include/ucrt.h>
#include <include/ds_obj_id.h>

/* Represent host connection */
struct ds_con {
	int sock;
	int err;
};

int  ds_connect(struct ds_con *con,char *ip,int port);

void ds_close(struct ds_con *con);

int  ds_create_object(struct ds_con *con, struct ds_obj_id *id, uint64_t obj_size); 

int  ds_put_object(struct ds_con *con, struct ds_obj_id *id, uint64_t off, void *data, uint32_t data_size);

int  ds_get_object(struct ds_con *con,struct ds_obj_id *id, uint64_t off, void *data, uint32_t data_size, uint32_t *read_size);

int  ds_delete_object(struct ds_con *con,struct ds_obj_id *id);
