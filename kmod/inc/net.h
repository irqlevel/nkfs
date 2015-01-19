#pragma once

struct ds_server {
	struct task_struct	*thread;
	struct socket		*sock;
	struct mutex		lock;
	struct list_head	srv_list;
	struct list_head	con_list;
	struct mutex		con_list_lock;
	u32			ip;
	int			port;
	int			stopping;
	int			err;
	struct completion	comp;
};

struct ds_con {
	struct socket 		*sock;
	struct list_head	list;
	struct ds_server	*server;
	struct task_struct 	*thread;	
	int			err;
};

int ds_server_start(u32 ip, int port);
int ds_server_stop(u32 ip, int port);
void ds_server_finit(void);
int ds_server_init(void);

int ds_con_connect(u32 ip, int port, struct ds_con **pcon);
int ds_con_send(struct ds_con *con, void *buffer, u32 nob);
int ds_con_recv(struct ds_con *con, void *buffer, u32 nob);

int ds_con_send_pkt(struct ds_con *con, struct ds_net_pkt *pkt);
int ds_con_send_reply(struct ds_con *con,
		struct ds_net_pkt *reply, int err);

int ds_con_recv_pkt(struct ds_con *con,
		struct ds_net_pkt *pkt);

void ds_con_close(struct ds_con *con);
void ds_con_fail(struct ds_con *con, int err);

int ds_ip_port_cmp(u32 ip1, int port1, u32 ip2, int port2);
