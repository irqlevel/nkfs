#include <include/ds_client.h>
#include <include/ds_net.h>

static void con_fail(struct ds_con *con, int err)
{
	con->err = err;
}

static int con_check(struct ds_con *con)
{
	return con->err;
}

static int con_send(struct ds_con *con, void *buf, u32 size)
{
	ssize_t sent = 0, total_sent = 0;
	int err;

	if ((err = con_check(con))) {
		CLOG(CL_ERR, "con err %d", err);
		goto out;
	}

	do {
		sent = send(con->sock, (char *)buf + total_sent,
				size - total_sent, 0);
		if (sent < 0) {
			err = errno;
			CLOG(CL_ERR, "send err %d", err);
			con_fail(con, err);
			goto out;
		}
		total_sent+= sent;
	} while (total_sent < size);
	err = 0;
out:
	return err;
}

static int con_recv(struct ds_con *con, void *buf, u32 size)
{
	ssize_t recvd = 0, total_recvd = 0;
	int err;

	if ((err = con_check(con))) {
		CLOG(CL_ERR, "con err %d", err);
		goto out;
	}

	do {
		recvd = recv(con->sock, (char *)buf + total_recvd,
				size - total_recvd, 0);
		if (recvd < 0) {
			err = errno;
			CLOG(CL_ERR, "recv err %d", err);
			con_fail(con, err);
			goto out;
		}
		total_recvd+= recvd;
	} while (total_recvd < size);
	err = 0;
out:
	return err;
}

static int con_init(struct ds_con *con)
{
	int sock;
	int err;

	memset(con, 0, sizeof(*con));	
	sock = socket(AF_INET,SOCK_STREAM,0);
	if (sock < 0) {
		CLOG(CL_ERR, "con_handle_init() -> socket() failed");
		err =  DS_E_CON_INIT_FAILED;
		con_fail(con, err);
		goto out;
	} 
	con->sock = sock;
	err = 0;
out:
	return err;
}

int ds_connect(struct ds_con *con, char *ip, int port)
{
	struct sockaddr_in serv_addr;
	int err;
	
	err = con_init(con);
	if (err) {
		CLOG(CL_INF, "ds_connect() -> err %x - %s", err, ds_error(err));
		return err;
	}
	
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	
	err = inet_aton(ip,(struct in_addr*)&(serv_addr.sin_addr.s_addr));
	if(!err) { 
		CLOG(CL_ERR, "ds_connect() -> inet_aton(), invalid address");
		err = -EFAULT;
		goto out;
	}
	
	crt_memset(&(serv_addr.sin_zero), 0, sizeof(serv_addr.sin_zero));

	err = connect(con->sock,(struct sockaddr*)&serv_addr,sizeof(struct sockaddr));
	if (err) {
		CLOG(CL_ERR, "ds_connect() -> connect(), connection failed");
		err = ENOTCONN;
		goto out;
	}

	err = 0;
out:
	close(con->sock);
	return err;
}

int ds_send_object(struct ds_con *con, struct ds_obj_id *id,
		u64 off, void *data, u32 data_size)
{
	struct ds_net_cmd cmd, reply;
	int err;

	net_cmd_init(&cmd);
	cmd.data_size = data_size;
	cmd.off = off;
	cmd.cmd = DS_NET_CMD_OBJ_SEND;
	memcpy(&cmd.obj_id, id, sizeof(*id));

	err = con_send(con, &cmd, sizeof(cmd));
	if (err) {
		CLOG(CL_ERR, "con_send err %d", err);
		goto out;
	}

	err = con_send(con, data, data_size);
	if (err) {
		CLOG(CL_ERR, "con_send err %d", err);
		goto out;
	}

	err = con_recv(con, &reply, sizeof(reply));
	if (err) {
		CLOG(CL_ERR, "con_recv err %d", err);
		goto out;
	}

        if ((err = net_cmd_check_sign(&reply))) {
                CLOG(CL_ERR, "reply invalid sign err %d", err);
                goto out;
        }

        if ((err = net_cmd_check_unique(&reply, cmd.unique))) {
                CLOG(CL_ERR, "reply invalid unique %lld vs %lld err %d",
			err, reply.unique, cmd.unique);
                goto out;
        }

	err = cmd.err;
	if (err) {
		CLOG(CL_ERR, "obj put err %d", err);
	}
out:
	return err;
}

int ds_recv_object(struct ds_con *con, struct ds_obj_id *id, u64 off,
		void *data, u32 data_size, u32 *recv_size)
{
	struct ds_net_cmd cmd, reply;
	int err;

	net_cmd_init(&cmd);
	cmd.data_size = data_size;
	cmd.off = off;
	cmd.cmd = DS_NET_CMD_OBJ_RECV;
	memcpy(&cmd.obj_id, id, sizeof(*id));

	err = con_send(con, &cmd, sizeof(cmd));
	if (err) {
		CLOG(CL_ERR, "con_send err %d", err);
		goto out;
	}

	err = con_recv(con, &reply, sizeof(reply));
	if (err) {
		CLOG(CL_ERR, "con_recv err %d", err);
		goto out;
	}

        if ((err = net_cmd_check_sign(&reply))) {
                CLOG(CL_ERR, "reply invalid sign err %d", err);
                goto out;
        }

        if ((err = net_cmd_check_unique(&reply, cmd.unique))) {
                CLOG(CL_ERR, "reply invalid unique %lld vs %lld err %d",
			err, reply.unique, cmd.unique);
                goto out;
        }

	err = cmd.err;
	if (err) {
		CLOG(CL_ERR, "obj get err %d", err);
		goto out;
	}

	if (cmd.data_size > data_size) {
		CLOG(CL_ERR, "recv data_size %d vs data_size %d",
				cmd.data_size, data_size);
		goto out;
	}
	
	err = con_recv(con, data, cmd.data_size);
	if (err) {
		CLOG(CL_ERR, "obj get err %d", err);
		goto out;
	}
	
	*recv_size = cmd.data_size;
out:
	return err;
}

int ds_delete_object(struct ds_con *con, struct ds_obj_id *id)
{
	struct ds_net_cmd cmd, reply;
	int err;

	net_cmd_init(&cmd);
	cmd.cmd = DS_NET_CMD_OBJ_DELETE;	
	memcpy(&cmd.obj_id, id, sizeof(*id));

	err = con_send(con, &cmd, sizeof(cmd));
	if (err) {
		CLOG(CL_ERR, "con_send err %d", err);
		goto out;
	}

	err = con_recv(con, &reply, sizeof(reply));
	if (err) {
		CLOG(CL_ERR, "con_recv err %d", err);
		goto out;
	}

        if ((err = net_cmd_check_sign(&reply))) {
                CLOG(CL_ERR, "reply invalid sign err %d", err);
                goto out;
        }

        if ((err = net_cmd_check_unique(&reply, cmd.unique))) {
                CLOG(CL_ERR, "reply invalid unique %lld vs %lld err %d",
			err, reply.unique, cmd.unique);
                goto out;
        }


	err = cmd.err;
	if (err) {
		CLOG(CL_ERR, "obj create err %d", err);
	}
out:
	return err;
}

void ds_close(struct ds_con *con)
{
	close(con->sock);
}

