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
#include <include/ds_cmd_packet.h>

/* Represent host connection */
struct con_handle {
	int sock;
};

int  ds_connect(struct con_handle *con,char *ip,int port);

void ds_close(struct con_handle *con);

int  ds_create_object(struct con_handle *con, struct ds_obj_id *id, uint64_t obj_size); 

int  ds_put_object(struct con_handle *con,struct ds_obj_id *id,char *data, uint64_t data_size);

int  ds_get_object(struct con_handle *con,struct ds_obj_id *id,char *data, uint64_t data_size);

int  ds_delete_object(struct con_handle *con,struct ds_obj_id *id);
