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
#define OBJ_SIZE 65535

int main(int argc, const char *argv[])
{
		int err = DS_E_BUF_SMALL;
		int i,j;
		char *msg="teststringteststringteststringteststringteststring";
		/* Object with data that client want to send */
		struct object *client_obj;
		/* object received from another client */
		struct object *income_obj;
		/*
		 * Create an array of connections 
		 * In future there will be function for dynamic allocation
		 * every time computer connects to the network and becomes neighbour
		 * con_handle struct will be added 
		 */
		struct con_handle con[CON_NUM]; 
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
		/* Create two objects */
		client_obj = crt_malloc(sizeof(struct object));
	    income_obj = crt_malloc(sizeof(struct object));
		income_obj->data=crt_malloc(OBJ_SIZE);
		income_obj->data=crt_malloc(OBJ_SIZE);
		/* Connect to two neighbours in network group */
		ds_connect(&con[0],"192.168.1.200",9999);
		ds_connect(&con[1],"192.168.1.245",8700);
		
		if(ds_create_object(&con[0],&client_obj->id,sizeof(*(client_obj->data))))
				CLOG(CL_ERR, "cant reserve space for object on storage");
		
		/* generate object id and output it */
		client_obj.id = ds_obj_id_gen();
		income_obj.id = ds_obj_id_gen();
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
		
		/* Fill object data | lenght is 50*/
		strcpy(client_obj->data,msg,strlen(msg));
		client_obj->size = sizeof(*client_obj->data);
		client_obj->data_off = 0;
											
		/* Send object to first node */
		if(ds_put_object(&con[0],&client_obj->id,client_obj->data,client_obj->size,&client_obj->data_off))
				CLOG(CL_ERR, "failed to send object");
		/* Receive object from another node. 40 byte 
		if(ds_get_object(&con[1],&income_obj.id,,
				CLOG(CL_ERR, "failed to send object");
		*/
		
		crt_free(income_obj->id);
		crt_free(client_obj->id);
		/* Disconnect from all hosts */
		for(i=0;i<CON_NUM;i++)
				ds_close(&con[i].sock);
		
		return 0;
}
