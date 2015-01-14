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
		CLOG(CL_ERR, "socket() failed");
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
		CLOG(CL_INF, "err %x - %s", err, ds_error(err));
		return err;
	}
	
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	
	err = inet_aton(ip,(struct in_addr*)&(serv_addr.sin_addr.s_addr));
	if(!err) { 
		CLOG(CL_ERR, "inet_aton(), invalid address");
		err = -EFAULT;
		goto out;
	}
	
	crt_memset(&(serv_addr.sin_zero), 0, sizeof(serv_addr.sin_zero));

	err = connect(con->sock,(struct sockaddr*)&serv_addr,sizeof(struct sockaddr));
	if (err) {
		CLOG(CL_ERR, "connect(), connection failed");
		err = -ENOTCONN;
		goto out;
	}

	return 0;
out:
	close(con->sock);
	return err;
}

int ds_put_object(struct ds_con *con, struct ds_obj_id *obj_id,
		u64 off, void *data, u32 data_size)
{
	struct ds_net_pkt cmd, reply;
	struct sha256_sum sum;
	int err;

	net_pkt_zero(&cmd);
	cmd.dsize = data_size;
	cmd.u.put_obj.off = off;
	cmd.type = DS_NET_PKT_PUT_OBJ;
	ds_obj_id_copy(&cmd.u.put_obj.obj_id, obj_id);
	
	sha256(data, data_size, &sum, 0);
	memcpy(&cmd.dsum, &sum, sizeof(sum));

	net_pkt_sign(&cmd);

	err = con_send(con, &cmd, sizeof(cmd));
	if (err) {
		CLOG(CL_ERR, "send err %d", err);
		goto out;
	}

	err = con_send(con, data, data_size);
	if (err) {
		CLOG(CL_ERR, "send err %d", err);
		goto out;
	}

	err = con_recv(con, &reply, sizeof(reply));
	if (err) {
		CLOG(CL_ERR, "recv err %d", err);
		goto out;
	}

        if ((err = net_pkt_check(&reply))) {
                CLOG(CL_ERR, "reply invalid sign err %d", err);
                goto out;
        }

	err = reply.err;
	if (err) {
		CLOG(CL_ERR, "reply err %d", err);
	}
out:
	return err;
}

int ds_get_object(struct ds_con *con, struct ds_obj_id *obj_id,
		u64 off, void *data, u32 data_size)
{
	struct ds_net_pkt cmd, reply;
	struct sha256_sum dsum;
	int err;

	net_pkt_zero(&cmd);
	cmd.dsize = data_size;
	cmd.u.get_obj.off = off;
	cmd.type = DS_NET_PKT_GET_OBJ;
	ds_obj_id_copy(&cmd.u.get_obj.obj_id, obj_id);

	net_pkt_sign(&cmd);

	err = con_send(con, &cmd, sizeof(cmd));
	if (err) {
		CLOG(CL_ERR, "send err %d", err);
		goto out;
	}

	err = con_recv(con, &reply, sizeof(reply));
	if (err) {
		CLOG(CL_ERR, "recv err %d", err);
		goto out;
	}

        if ((err = net_pkt_check(&reply))) {
                CLOG(CL_ERR, "reply invalid sign err %d", err);
                goto out;
        }

	err = reply.err;
	if (err) {
		CLOG(CL_ERR, "reply err %d", err);
		goto out;
	}

	if (reply.dsize != cmd.dsize) {
		CLOG(CL_ERR, "invalid size");
		err = -EINVAL;
		goto out;
	}

	err = con_recv(con, data, cmd.dsize);
	if (err) {
		CLOG(CL_ERR, "obj get err %d", err);
		goto out;
	}

	sha256(data, cmd.dsize, &dsum, 0);
	if ((err = net_pkt_check_dsum(&reply, &dsum))) {
		CLOG(CL_ERR, "obj inv dsum %d", err);
		goto out;
	}
out:
	return err;
}

int ds_delete_object(struct ds_con *con, struct ds_obj_id *id)
{
	struct ds_net_pkt cmd, reply;
	int err;

	net_pkt_zero(&cmd);
	cmd.type = DS_NET_PKT_DELETE_OBJ;
	ds_obj_id_copy(&cmd.u.delete_obj.obj_id, id);

	net_pkt_sign(&cmd);

	err = con_send(con, &cmd, sizeof(cmd));
	if (err) {
		CLOG(CL_ERR, "send err %d", err);
		goto out;
	}

	err = con_recv(con, &reply, sizeof(reply));
	if (err) {
		CLOG(CL_ERR, "recv err %d", err);
		goto out;
	}

        if ((err = net_pkt_check(&reply))) {
                CLOG(CL_ERR, "reply invalid sign err %d", err);
                goto out;
        }

	err = reply.err;
	if (err) {
		CLOG(CL_ERR, "reply err %d", err);
	}
out:
	return err;
}

int ds_create_object(struct ds_con *con, struct ds_obj_id *pobj_id)
{
	struct ds_net_pkt cmd, reply;
	int err;

	net_pkt_zero(&cmd);
	cmd.type = DS_NET_PKT_CREATE_OBJ;
	net_pkt_sign(&cmd);

	err = con_send(con, &cmd, sizeof(cmd));
	if (err) {
		CLOG(CL_ERR, "send err %d", err);
		goto out;
	}

	err = con_recv(con, &reply, sizeof(reply));
	if (err) {
		CLOG(CL_ERR, "recv err %d", err);
		goto out;
	}

        if ((err = net_pkt_check(&reply))) {
                CLOG(CL_ERR, "reply invalid sign err %d", err);
                goto out;
        }

	err = reply.err;
	if (err) {
		CLOG(CL_ERR, "reply err %d", err);
		goto out;
	}

	ds_obj_id_copy(pobj_id, &reply.u.create_obj.obj_id);
out:
	return err;
}


int ds_echo(struct ds_con *con)
{
	struct ds_net_pkt cmd, reply;
	int err;

	net_pkt_zero(&cmd);
	cmd.type = DS_NET_PKT_ECHO;
	net_pkt_sign(&cmd);

	err = con_send(con, &cmd, sizeof(cmd));
	if (err) {
		CLOG(CL_ERR, "send err %d", err);
		goto out;
	}

	err = con_recv(con, &reply, sizeof(reply));
	if (err) {
		CLOG(CL_ERR, "recv err %d", err);
		goto out;
	}

        if ((err = net_pkt_check(&reply))) {
                CLOG(CL_ERR, "reply invalid sign err %d", err);
                goto out;
        }

	err = reply.err;
	if (err) {
		CLOG(CL_ERR, "reply err %d", err);
	}
out:
	return err;
}

void ds_close(struct ds_con *con)
{
	close(con->sock);
}
