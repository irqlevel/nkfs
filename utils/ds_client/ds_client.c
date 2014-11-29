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
	int err = DS_E_BUF_SMALL;
	int i;
	char *msg="teststringteststringteststringteststringteststring";
	struct con_handle con; 
	/* represent client data */
	struct ds_obj_id *obj_id;
	char *obj_data;
	int64_t obj_size;
	int64_t obj_off;
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
	obj_size = strlen(msg);
	crt_memcpy(obj_data,msg,data_size);
	data_off = 0;
	/* Reserve space on server */
	if(ds_create_object(&con[0],*obj_id,)) {
		CLOG(CL_ERR, "cant reserve space for object on storage");
		return 0;
	}
	
	/* Send object to first node */

	if(ds_put_object(&con[0],*(client_obj->id),client_obj->data,client_obj->size,&client_obj->data_off)) {
		CLOG(CL_ERR, "failed to send object");
		return 0;
	}
		/* Receive object from another node. 40 byte 
		if(ds_get_object(&con[1],&income_obj.id,,
				CLOG(CL_ERR, "failed to send object");
		*/
	ds_close(&con);
	return 0;
}
