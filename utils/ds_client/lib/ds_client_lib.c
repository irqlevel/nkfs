#include <include/ds_client.h>

static int con_handle_init(struct con_handle *connection)
{
	int sock;
	
	sock = socket(AF_INET,SOCK_STREAM,0);
	if (sock == -1) {
		CLOG(CL_ERR, "con_handle_init() -> socket() failed");
		return DS_E_CON_INIT_FAILED;
	} 

	connection->sock = sock;
	return 0;
}

int ds_connect(struct con_handle *con,char *ip,int port)
{
	struct sockaddr_in serv_addr;
	int err;
	
	err = con_handle_init(con);
	if (err) {
		CLOG(CL_INF, "ds_connect() -> err %x - %s", err, ds_error(err));
		return err;
	}
		
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	
	err = inet_aton(ip,(struct in_addr*)&(serv_addr.sin_addr.s_addr));
	if(!err) { 
		CLOG(CL_ERR, "ds_connect() -> inet_aton() failed, invalid address");
		close(con->sock);
		return EFAULT;
	}
		
	crt_memset(&(serv_addr.sin_zero),0,8);
	
	err = connect(con->sock,(struct sockaddr*)&serv_addr,sizeof(struct sockaddr));
	if (err<0) {
		CLOG(CL_ERR, "ds_connect() -> connect(), connection failed");
		close(con->sock);
		return ENOTCONN;
	}
	/* connect() return 0 on succeed */
	return err;
} 
int  ds_put_object(struct con_handle *con,struct ds_obj_id id, char *data, uint64_t data_size)
{
		struct ds_cmd *cmd;
		
		pack = crt_malloc(sizeof(struct ds_packet));
		pack->data = crt_malloc(data_size);
		if(!(pack->data)) 

		pack->error=0;
		pack->cmd = DS_PKT_OBJ_PUT;
		crt_memcpy(pack->data,data,data_size);
		pack->data_off=*off;
		*(pack->obj_id)=id;
		pack->data_size = data_size;
		
		if((send(con->sock,pack,sizeof(*pack),0))<0) {
				CLOG(CL_ERR, "ds_put_object() -> send() packet failed to send");
				goto out;
		} 
		if((send(con->sock,pack->data,pack->data_size,0))<0) {
				CLOG(CL_ERR, "ds_put_object() -> send() packet data failed to send");
				goto out;
		} 
		if((send(con->sock,pack->obj_id,sizeof(*(pack->obj_id)),0))<0) {
				CLOG(CL_ERR, "ds_put_object() -> send() object id failed to send");
				goto out;
		} 
		dspack_release(pack);
		return 0;
		out:
			dspack_release(pack);
			return DS_E_OBJ_PUT;	
}

int  ds_create_object(struct con_handle *con, struct ds_obj_id *obj_id, uint64_t obj_size)
{		
	struct ds_cmd cmd;
	/*
	 * Object size that client requests 
	 * is the only data in packet
	 */
	cmd.data = crt_malloc(sizeof(obj_size));
	if (!cmd.data) {
		CLOG(CL_ERR, "ds_create_object() -> failed to allocate space for cmd data");
		return DS_E_OBJ_CREATE;
	}
	/* Convert 64-bit int into char array. C99 feature. */
	snprintf(cmd.data,sizeof(obj_size),"%d",obj_size);

	cmd.data_size = sizeof(obj_size);
	cmd.obj_id = *obj_id;
	cmd.cmd = 1; /* 1 - create object */
	cmd.data_off = 0;
	cmd.error = 0;
	/* Command packet data represents size of future object */
	if((send(con->sock,cmd,sizeof(cmd),0))<0) {
		CLOG(CL_ERR, "ds_create_object() -> send(), cmd failed to send");
		goto out;
	} 
	if((send(con->sock,cmd_pack.data,cmd_pack.data_size,0))<0) {
		CLOG(CL_ERR, "ds_create_object() -> send(), cmd data failed to send");
		goto out;
	} 
	crt_free(cmd.data);
	return 0;
	out:
		crt_free(cmd.data);	
		return DS_E_CRT_FLD;
}
int  ds_delete_object(struct con_handle *con,struct ds_obj_id obj_id)
{
		struct ds_packet *delete_pack;
		
		delete_pack = crt_malloc(sizeof(struct ds_packet));
		delete_pack->data= NULL;
		delete_pack->obj_id = crt_malloc(sizeof(struct ds_obj_id));
		delete_pack->cmd = DS_PKT_OBJ_DEL;
		
		*(delete_pack->obj_id) = obj_id;
		delete_pack->data_off = 0;
		delete_pack->data_size = 0;
		delete_pack->error = 0;
		
		if((send(con->sock,delete_pack,sizeof(*delete_pack),0))<0) {
				CLOG(CL_ERR, "ds_delete_object() -> send() packet failed to send");
				goto out;
		} 
		if((send(con->sock,delete_pack->obj_id,sizeof(*(delete_pack->obj_id)),0))<0) {
				CLOG(CL_ERR, "ds_delete_object() -> send() packet failed to send");
				goto out;
		}
		
		dspack_release(delete_pack);
		return 0;
		out:
				dspack_release(delete_pack);
				return -DS_E_DEL_FLD;
}
int  ds_get_object(struct con_handle *con,struct ds_obj_id id, char *data, uint64_t data_size, uint64_t *off)
{
		if(recv(con->sock,data,data_size,0)<0) {
				CLOG(CL_ERR, "ds_get_object() -> recv() failed receive packet data");
		/*
		 * There will be packet parsing 
		 * data from another host will be held in struct ds_packet
		 */
		}
		return -ENOSYS;
}
void ds_close(struct con_handle *con)
{
		close(con->sock);
}


