#pragma once

enum {
	DS_NEIGH_INITED,
	DS_NEIGH_CONNECTED,
};

#pragma pack(push, 1)
struct ds_neigh_info {
	struct ds_obj_id	host_id;
	u32			ip;
	int			port;	
};

struct ds_host_info {
	struct ds_obj_id	net_id;
	struct ds_obj_id	host_id;
	int			nr_neighs;
	struct ds_neigh_info	neighs[128];
};

struct ds_host_work {
	struct work_struct	work;
	struct ds_host		*host;
	void			*data;
};

struct ds_host {
	struct ds_obj_id	net_id;
	struct ds_obj_id	host_id;
	struct rb_root		neighs;
	rwlock_t		neighs_lock;
	struct list_head	neigh_list;
	struct timer_list 	timer;
	struct workqueue_struct *wq;
	int			neighs_active;
	int			stopping;
};

struct ds_neigh {
	struct list_head	list;
	atomic_t		ref;
	struct ds_obj_id	host_id;
	struct rb_node		neighs_link;
	struct ds_host		*host;
	struct ds_con		*con;
	u32			ip;
	int			port;
	int			state;
	struct work_struct	work;
	atomic_t		work_used;
	void			*work_data;
};

#pragma pack(pop)

void ds_neigh_ref(struct ds_neigh *neigh);
void ds_neigh_deref(struct ds_neigh *neigh);

#define NEIGH_REF(n)	\
	ds_neigh_ref(n);

#define NEIGH_DEREF(n)	\
	ds_neigh_deref(n);

int ds_route_init(void);
void ds_route_finit(void);

int ds_neigh_add(u32 ip, int port);
int ds_neigh_remove(u32 ip, int port);

