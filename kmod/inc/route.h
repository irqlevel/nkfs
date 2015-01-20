#pragma once

enum {
	DS_NEIGH_INIT,
	DS_NEIGH_VALID,
};

#pragma pack(push, 1)

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
	struct rb_root		neigh_ids;
	rwlock_t		neigh_ids_lock;
	int			neigh_ids_active;
};

struct ds_neigh_id {
	atomic_t		ref;
	struct ds_host		*host;
	struct list_head	neigh_ids_list;
	struct ds_obj_id	host_id;
	struct rb_node		neigh_ids_link;
};

struct ds_neigh {
	struct list_head	neigh_list;
	struct list_head	neigh_id_list;
	atomic_t		ref;
	struct ds_obj_id	host_id;
	struct rb_node		neighs_link;
	struct ds_host		*host;
	struct ds_con		*con;
	u32			d_ip;
	int			d_port;
	u32			s_ip;
	int			s_port;
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

void ds_neigh_id_ref(struct ds_neigh_id *neigh_id);
void ds_neigh_id_deref(struct ds_neigh_id *neigh_id);

#define NEIGH_ID_REF(n)	\
	ds_neigh_id_ref(n);

#define NEIGH_ID_DEREF(n)	\
	ds_neigh_id_deref(n);

int ds_route_init(void);
void ds_route_finit(void);

int ds_neigh_add(u32 d_ip, int d_port, u32 s_ip, int s_port);
int ds_neigh_remove(u32 d_ip, int d_port);

int ds_neigh_handshake(struct ds_obj_id *net_id,
	struct ds_obj_id *host_id, u32 d_ip, int d_port,
	u32 s_ip, int s_port, struct ds_obj_id *reply_host_id);
