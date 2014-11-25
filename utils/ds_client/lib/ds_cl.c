#include "include/ds_client.h"
#include "include/ds_packet.h"

int con_handle_init(struct con_handle *connection)
{
		int sock;
		
		sock = socket(AF_INET,SOCK_STREAM,0);
		if (sock == -1) {
				CLOG(CL_ERR, "con_handle_init() -> socket() failed");
				return 1;
		}
		else {
				connection->sock = sock;
				return 0;
		}
}

int ds_connect(struct con_handle *con,char *ip,int port)
{
		struct sockaddr_in serv_addr;
		int32_t ret;
		
		if (con_handle_init(con)) {
				CLOG(CL_ERR, "ds_connect() -> create connection failed");
				return 1;
		}
		
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_port = htons(port);
	
		ret=inet_aton(ip,&(serv_addr.sin_addr.s_addr));
		if(!ret) { 
				CLOG(CL_ERR, "ds_connect() -> inet_aton() failed, invalid address");
				return -EFAULT;
		}
		
		crt_memset(&(serv_addr.sin_zero),0,8);
	
		ret = connect(con->sock,(struct sockaddr*)&serv_addr,sizeof(struct sockaddr));
		if (ret == -1) {
				CLOG(CL_ERR, "ds_connect() -> connect() failed");
				return -ENOTCONN;
		}
		return 0;
} 
int  ds_put_object(struct con_handle *con,struct ds_obj_id id, char *data, uint64_t data_size, uint64_t *off)
{
		struct ds_packet *pack;
		
		pack = crt_malloc(sizeof(struct ds_packet));
		pack->data = crt_malloc(data_size);
		pack->obj_id = crt_malloc(sizeof(struct ds_obj_id)); 
		
		pack->cmd = DS_PKT_OBJ_PUT;
		crt_memcpy(pack->data,data,data_size);
		pack->data_off=&off;
		*(pack->obj_id)=id;
		pack->data_size = data_size;
		
		if((send(sock,pack,sizeof(*pack),0))<0) {
				CLOG(CL_ERR, "ds_put_object() -> send() packet failed to send");
				goto out;
		} 
		if((send(sock,pack->data,pack->data_size,0))<0) {
				CLOG(CL_ERR, "ds_put_object() -> send() packet data failed to send");
				goto out;
		} 
		
		dspack_release(pack);
		return 0;
		out:
			dspack_release(pack);
			return -DS_E_PUT_FLD;	
}

int  ds_create_object(struct con_handle *con, struct ds_obj_id obj_id, uint64_t obj_size);
{		
		struct ds_packet *cr_pack;
		
		cr_pack = crt_malloc(sizeof(struct ds_packet));
		/*
		 * Object size that client requests 
		 * is the only data in packet
		 */
		cr_pack->data= crt_malloc(sizeof(uint64_t));
		cr_pack->obj_id = crt_malloc(sizeof(struct ds_obj_id));
		cr_pack->cmd = DS_PKT_OBJ_CREATE;
		
		*(cr_pack->data)=obj_size;
		*(cr_pack->obj_id) = obj_id;
		cr_pack->data_off = 0;
		cr_pack->data_size = sizeof(uint64_t);
		
		/* Packet data represents size of future object */
		if((send(sock,cr_pack,sizeof(*cr_pack),0))<0) {
				CLOG(CL_ERR, "ds_create_object() -> send() packet failed to send");
				goto out;
		} 
		if((send(sock,cr_pack->data,cr_pack->data_size,0))<0) {
				CLOG(CL_ERR, "ds_create_object() -> send() packet failed to send");
				goto out;
		} 
		
		dspack_release(cr_pack);
		return 0;
		out:
				dspack_release(cr_pack);
				return -DS_E_CRT_FLD;
}
int  ds_delete_object(struct con_handle *con,struct ds_obj_id obj_id)
{
		/* Not implemented */
		return -ENOSYS;
}
int  ds_get_object(struct con_handle *con,struct ds_obj_id id, char *data, uint64_t *data_size, uint64_t *off)
{
		/*NOT IMPLEMENTED*/
		return -ENOSYS;
}
int ds_close(struct con_handle *con)
{
		close(con->sock);
}

void dspack_release(struct ds_packet *pack)
{
	crt_free(pack->obj_id);
	crt_free(pack->data);
	crt_free(pack);
}
