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
/* Describe object, client data */
struct object {
		struct ds_obj_id *id;
		char             *data;
		uint64_t		 data_off;
		uint64_t         size;
};
/* Represent host connection */
struct con_handle {
		int	sock;
};
/*
 * ds_put_object() packs object in ds_packet structure and send to server
 * ds_get_object() make object from received ds_packet structure  
*/
int  ds_connect(struct con_handle *con,char *ip,int port);
int  ds_close(struct con_handle *con);
int  ds_create_object(struct con_handle *con, struct ds_obj_id obj_id, uint64_t obj_size); 
int  ds_put_object(struct con_handle *con,struct ds_obj_id obj_id, char *data, uint64_t data_size, uint64_t off);
int  ds_get_object(struct con_handle *con,struct ds_obj_id obj_id, char *data, uint64_t data_size, uint64_t off);
int  ds_delete_object(struct con_handle *con,struct ds_obj_id obj_id);
