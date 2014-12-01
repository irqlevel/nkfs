#include <include/ds_client.h>
/* 
 * Handle error according to code from server  
 * tested, work properly 
 * if server sends just integer value 
 */
static int err_code_handle(struct con_handle con,int err)
{
	ssize_t count_bytes;

	count_bytes = recv(con.sock,&err,sizeof(int),0);

	if (!count_bytes) {
		CLOG(CL_ERR,"err_code_hanlde() -> connection has been closed");
		return 1;
	}
	if (count_bytes<0) {
		CLOG(CL_ERR,"err_code_hanlde() -> failed to receive error code from server");
		return 1;
	}
	
	if(err) {
		
		switch(err) {
			/*
		case 10:
		...
		case 26:
		*/
		}
	}
	
	return 0;
	
}

static int send_cmd_packet(struct con_handle *con,struct ds_cmd *cmd)
{
	int err = 1;

	if((send(con->sock,&cmd,sizeof(cmd),0))<0) {
		CLOG(CL_ERR, "send(),cmd failed to send");
		goto out;
	} 

	if((send(con->sock,cmd->data,cmd->data_size,0))<0) {
		CLOG(CL_ERR, "send(),cmd data failed to send");
		goto out;
	} 

	if(err_code_handle(*con,cmd->error))
		goto out;

	err = 0;
	out:
		return err;
}

static int con_handle_init(struct con_handle *con)
{
	int sock;
	
	sock = socket(AF_INET,SOCK_STREAM,0);
	if (sock<0) {
		CLOG(CL_ERR, "con_handle_init() -> socket() failed");
		return DS_E_CON_INIT_FAILED;
	} 

	con->sock = sock;

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
		CLOG(CL_ERR, "ds_connect() -> inet_aton(), invalid address");
		err = EFAULT;
		goto out;
	}
		
	crt_memset(&(serv_addr.sin_zero),0,8);
	
	err = connect(con->sock,(struct sockaddr*)&serv_addr,sizeof(struct sockaddr));
	if (err<0) {
		CLOG(CL_ERR, "ds_connect() -> connect(), connection failed");
		err = ENOTCONN;
		goto out;
	}
	
	err = 0;
	out:
		close(con->sock);
		return err;
} 

int  ds_put_object(struct con_handle *con,struct ds_obj_id *id, char *data, uint64_t data_size)
{
	struct ds_cmd cmd;
	int err = DS_E_OBJ_PUT;

	cmd.data = crt_malloc(data_size);
	if(!cmd.data) {
		CLOG(CL_ERR, "ds_put_object() -> failed to allocate space for cmd data");
		return err;
	} 

	crt_memcpy(cmd.data,data,data_size);
	cmd.data_size = data_size;
	cmd.obj_id = *id;
	cmd.cmd = 2; /* 2 - put object */
	cmd.data_off = 0;
	cmd.error = 0;
	
	if(send_cmd_packet(con,&cmd))
		return err;

	crt_free(cmd.data);
	return 0;
}

int  ds_create_object(struct con_handle *con, struct ds_obj_id *id, uint64_t obj_size)
{		
	struct ds_cmd cmd;
	int err = DS_E_OBJ_CREATE;
	/*
	 * Object size that client requests 
	 * is the only data in packet
	 */
	cmd.data = crt_malloc(sizeof(obj_size));

	if (!cmd.data) {
		CLOG(CL_ERR, "ds_create_object() -> failed to allocate space for cmd data");
		return err;
	}
	/* Convert 64-bit int into char array. C99 feature. */
	snprintf(cmd.data,sizeof(obj_size),"%d",(int)obj_size);

	cmd.data_size = sizeof(obj_size);
	cmd.obj_id = *id;
	cmd.cmd = 1; /* 1 - create object */
	cmd.data_off = 0;
	cmd.error = 0;
	
	if(send_cmd_packet(con,&cmd))
		goto out;
	
	err = 0;
	out:
		crt_free(cmd.data);	
		return err;
}

int  ds_delete_object(struct con_handle *con,struct ds_obj_id *id)
{
	struct ds_cmd cmd;
	int err = DS_E_OBJ_DELETE;

	cmd.data = crt_malloc(sizeof(struct ds_obj_id));
	if (!cmd.data) {
		CLOG(CL_ERR, "ds_delete_object() -> failed to allocate space for cmd data");
		return err;
	}
	
	crt_memcpy(cmd.data,id->bytes,sizeof(id->bytes));
	cmd.data_size = sizeof(struct ds_obj_id);
	cmd.obj_id = *id;
	cmd.cmd = 3; /* 3 - delete object */
	cmd.data_off = 0;
	cmd.error = 0;
	
	if(send_cmd_packet(con,&cmd))
		goto out;
	
	err = 0;

	out:
		crt_free(cmd.data);	
		return err;
}

int  ds_get_object(struct con_handle *con,struct ds_obj_id *id, char *data, uint64_t data_size)
{
	struct ds_cmd cmd;
	ssize_t count_bytes;
	int err = DS_E_OBJ_GET;
	/*
	 * 1) Send object id that client requested and get command
	 * 2) Receive data of requested object
	*/
	cmd.data = crt_malloc(sizeof(struct ds_obj_id));
	if (!cmd.data) {
		CLOG(CL_ERR, "ds_get_object() -> failed to allocate space for cmd data");
		return err;
	}
	
	crt_memcpy(cmd.data,id->bytes,sizeof(id->bytes));
	cmd.data_size = sizeof(struct ds_obj_id);
	cmd.obj_id = *id;
	cmd.cmd = 4; /* 4 - get object */
	cmd.data_off = 0;
	cmd.error = 0;
	
	if(send_cmd_packet(con,&cmd))
		goto out;

	count_bytes = recv(con->sock,data,data_size,0);
	if (!count_bytes) {
		CLOG(CL_ERR, "ds_get_object()->recv(),connection has been closed");
		goto out;
	}

	if (count_bytes<0) {
		CLOG(CL_ERR, "ds_get_object()->recv(),failed receive object data");
		goto out;
	}

	err = 0;
	out:
		crt_free(cmd.data); 
		return err;
}

void ds_close(struct con_handle *con)
{
	close(con->sock);
}


