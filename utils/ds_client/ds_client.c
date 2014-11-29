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
	ds_connect(&con,"127.0.0.1",9900);
	/* generate object id and output it */
		client_obj->id = ds_obj_id_gen();
		income_obj->id = ds_obj_id_gen();
		if (!(client_obj->id) && !(income_obj->id)) {
				CLOG(CL_ERR, "cant generate obj id");
		} else {
				char *obj_id_s = ds_obj_id_to_str(client_obj->id);
				if (!obj_id_s) {
						CLOG(CL_ERR, "cant convert obj id to str");
		        } else {
						/* Log obj id */
						CLOG(CL_INF, "generated obj id %s", obj_id_s);
						crt_free(obj_id_s);
		        }
		}
	/* 
	 * After creating object on client side 
	 * do the same on server side
	 */
	if(ds_create_object(&con[0],*(client_obj->id),3000))
		CLOG(CL_ERR, "cant reserve space for object on storage");
		
		crt_memcpy(client_obj->data,msg,strlen(msg));
		client_obj->size = sizeof(*(client_obj->data));
		client_obj->data_off = 0;
											
		/* Send object to first node */
		if(ds_put_object(&con[0],*(client_obj->id),client_obj->data,client_obj->size,&client_obj->data_off))
				CLOG(CL_ERR, "failed to send object");
		/* Receive object from another node. 40 byte 
		if(ds_get_object(&con[1],&income_obj.id,,
				CLOG(CL_ERR, "failed to send object");
		*/
	ds_close(&con);
	return 0;
}
