#pragma once

struct nkfs_server {
	struct task_struct	*thread;
	struct socket		*sock;
	struct mutex		lock;
	struct list_head	srv_list;
	struct list_head	con_list;
	struct mutex		con_list_lock;
	u32			bind_ip;
	u32			ext_ip;
	int			port;
	int			stopping;
	int			err;
	struct completion	comp;
};

struct nkfs_con {
	struct socket		*sock;
	struct list_head	list;
	struct nkfs_server	*server;
	struct task_struct	*thread;
	int			err;
};

u16 nkfs_con_peer_port(struct nkfs_con *con);
u16 nkfs_con_self_port(struct nkfs_con *con);
u32 nkfs_con_peer_addr(struct nkfs_con *con);
u32 nkfs_con_self_addr(struct nkfs_con *con);

int nkfs_server_start(u32 bind_ip, u32 ext_ip, int port);
int nkfs_server_stop(u32 bind_ip, int port);
int nkfs_server_select_one(u32 *pip, int *pport);

void nkfs_server_finit(void);
int nkfs_server_init(void);

int nkfs_con_connect(u32 ip, int port, struct nkfs_con **pcon);
int nkfs_con_send(struct nkfs_con *con, void *buffer, u32 nob);
int nkfs_con_recv(struct nkfs_con *con, void *buffer, u32 nob);

int nkfs_con_send_pkt(struct nkfs_con *con, struct nkfs_net_pkt *pkt);
int nkfs_con_send_reply(struct nkfs_con *con,
		struct nkfs_net_pkt *reply, int err);

int nkfs_con_recv_pkt(struct nkfs_con *con,
		struct nkfs_net_pkt *pkt);

void nkfs_con_close(struct nkfs_con *con);
void nkfs_con_fail(struct nkfs_con *con, int err);

int nkfs_ip_port_cmp(u32 ip1, int port1, u32 ip2, int port2);
