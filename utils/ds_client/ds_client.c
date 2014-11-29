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

#define CON_NUM 3

int main(int argc, const char *argv[])
{
	char *msg="teststringteststringteststringteststringteststring";
	char *client_buff;
	uint64_t client_data_len;
	int err = DS_E_BUF_SMALL;
	int i;
	struct con_handle con; 
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
	CLOG(CL_INF, "Hello from ds client");
	/* translate error code to string description */
	CLOG(CL_INF, "err %x - %s", err, ds_error(err));
	/* Connect to neighbour in network group */
	if(ds_connect(&con,"127.0.0.1",9900)); {
		CLOG(CL_ERR, "cant connect to host");
		return 0;
	}
	/* generate object id and output it */
	obj_id = ds_obj_id_gen();
	if (!obj_id) {
		CLOG(CL_ERR, "cant generate obj id");
		goto out_con_free;
	} else {
		char *obj_id_s = ds_obj_id_to_str(obj_id);
		if (!obj_id_s) {
			CLOG(CL_ERR, "cant convert obj id to str");
		} else {
			/* Log obj id */
			CLOG(CL_INF, "generated obj id %s", obj_id_s);
			crt_free(obj_id_s);
		}
	}
	client_data_len = crt_strlen(msg);
	obj_data = crt_malloc(client_data_len);
	if (!obj_data) {
		CLOG(CL_ERR, "cant allocate requested space");
		goto out_id_free;
	} 
	crt_memcpy(obj_data,msg,client_data_len);
	obj_off = 0;

	/* Calculate overall object size in bytes */
	obj_size = client_data_len + 2*sizeof(uint64_t) + sizeof(struct ds_obj_id); 
	/* Reserver space for object on server */
	err = ds_create_object(&con,*obj_id,obj_size));
	if (err) {
		CLOG(CL_ERR, "cant reserve space for object on storage, err %x - %s",err,ds_error(err));
		goto out_all;
	}
	/* Send object to first node */
	err = ds_put_object(&con,obj_id,obj_data,obj_size);
	if(err) {
		CLOG(CL_ERR, "failed to send object, err %x - %s",err,ds_error(err));
		goto out_all;
	}
	/* Receive the same object which was send previosly from another node */
	err = ds_get_object(&con,obj_id,client_buff,obj_size);
	if (err) {
		CLOG(CL_ERR, "failed to get object, err %x - %s");
		goto out_all;
	}
	*/
	out_all:
		crt_free(obj_data);
	out_id_free:
		crt_free(obj_id_s);
		crt_free(obj_id);
	out_con_free:
		ds_close(&con);
	return 0;
}
