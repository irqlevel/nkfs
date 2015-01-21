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
	struct rb_root		host_ids;
	rwlock_t		host_ids_lock;
	int			host_ids_active;
};

struct ds_host_id {
	atomic_t		ref;
	struct ds_host		*host;
	struct list_head	neigh_list;
	rwlock_t		neigh_list_lock;
	struct ds_obj_id	host_id;
	struct rb_node		host_ids_link;
};

struct ds_neigh {
	struct list_head	neigh_list;
	struct list_head	host_id_list;
	atomic_t		ref;
	struct ds_host_id	*host_id;
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

void ds_host_id_ref(struct ds_host_id *host_id);
void ds_host_id_deref(struct ds_host_id *host_id);

#define HOST_ID_REF(hid)							\
	do {									\
		ds_host_id_ref((hid));						\
		KLOG(KL_DBG, "ref hid %p ref %d",				\
			(hid), atomic_read(&(hid)->ref));			\
	} while (0);

#define HOST_ID_DEREF(hid)							\
	do {									\
		KLOG(KL_DBG, "deref hid %p ref %d",				\
			(hid), atomic_read(&(hid)->ref));			\
		ds_host_id_deref((hid));					\
	} while (0);

int ds_route_init(void);
void ds_route_finit(void);

int ds_neigh_add(u32 d_ip, int d_port, u32 s_ip, int s_port);
int ds_neigh_remove(u32 d_ip, int d_port);

int ds_neigh_handshake(struct ds_obj_id *net_id,
	struct ds_obj_id *host_id, u32 d_ip, int d_port,
	u32 s_ip, int s_port, struct ds_obj_id *reply_host_id);
