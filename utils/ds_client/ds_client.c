#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <memory.h>
#include <errno.h>
#include <malloc.h>
#include <crtlib/include/crtlib.h>
#include <utils/ucrt/include/ucrt.h>
#include <include/ds_client.h> /* client lib */

#define SERV_IP   "127.0.0.1"
#define SERV_PORT 9900

static void CLOG_test(int err)
{
	CLOG(CL_INF, "Hello from ds client");
	/* translate error code to string description */
	CLOG(CL_INF, "err %x - %s", err, ds_error(err));
}
static void obj_id_output(struct ds_obj_id *id)
{
	char *obj_id_str;
	
	obj_id_str = ds_obj_id_to_str(id);
	if (!obj_id_str) {
		CLOG(CL_ERR, "cant convert obj id to string");
	} else {
		/* Log obj id */
		CLOG(CL_INF, "generated obj id %s", id);
		crt_free(obj_id_str);
	}
	
}
int main(int argc, const char *argv[])
{
	/* test data */
	char *msg="teststringteststringteststringteststringteststring";
	char *input_buff;
	uint64_t client_data_len;
	/* represent client connection */
	struct con_handle con; 
	/* for error codes */
	int err;
	/* represent client data */
	struct ds_obj_id *obj_id;
	char *obj_data;
	uint64_t obj_size;
	uint64_t obj_off;
	/* 
	 * set CLOG log path : NULL and log name ds_client.log
	 * NULL means use current working dir as path
	*/
	ucrt_log_set_path(NULL, "ds_client.log");
	/* set CLOG log level */
	ucrt_log_set_level(CL_DBG);

	CLOG_test(DS_E_BUF_SMALL);

	/* connect to neighbour in network group */
	err = ds_connect(&con,SERV_IP,SERV_PORT); 
	if (err) {
		CLOG(CL_ERR, "cant connect to host");
		return 0;
	}

	obj_id = ds_obj_id_gen();
	if (!obj_id) {
		CLOG(CL_ERR, "cant generate obj id");
		goto out_con_free;
	} 
	/* print object ID */
	obj_id_output(obj_id);

	client_data_len = crt_strlen(msg);

	obj_data = crt_malloc(client_data_len);
	if (!obj_data) {
		CLOG(CL_ERR, "cant allocate requested space");
		goto out_id_free;
	} 

	crt_memcpy(obj_data,msg,client_data_len);
	obj_off = 0;
	/* calculate overall object as a sum of all elements */
	obj_size = client_data_len + sizeof(obj_size) + sizeof(obj_off) + sizeof(struct ds_obj_id); 
	
	input_buff = crt_malloc(obj_size);
	if (!input_buff) {
		goto out_buff;
	}
	/* reserver space for object on server */
	err = ds_create_object(&con,obj_id,obj_size);
	if (err) {
		/* cant reserve space for object on storage */
		CLOG(CL_ERR, "err %x - %s",err,ds_error(err));
		goto out_all;
	}
	/* send object to first node */
	err = ds_put_object(&con,obj_id,obj_data,obj_size);
	if(err) {
		/* failed to send object */
		CLOG(CL_ERR, "err %x - %s",err,ds_error(err));
		goto out_all;
	}
	/* receive the same object which was send previosly from another node */
	err = ds_get_object(&con,obj_id,input_buff,obj_size);
	if (err) {
		/* failed to get object */
		CLOG(CL_ERR, "err %x - %s",err,ds_error(err));
		goto out_all;
	}
	out_all:
		crt_free(input_buff);
	out_buff:
		crt_free(obj_data);
	out_id_free:
		crt_free(obj_id);
	out_con_free:
		ds_close(&con);
	return 0;
}
