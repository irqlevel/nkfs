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
	struct task_struct 	*thread;
	struct socket 		*sock;
	struct list_head	con_list;
	struct ds_server	*server;
	int			err;
};

int ds_server_start(u32 ip, int port);
int ds_server_stop(u32 ip, int port);
void ds_server_finit(void);
int ds_server_init(void);
