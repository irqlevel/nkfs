#pragma once

enum {
	NKFS_NEIGH_INIT,
	NKFS_NEIGH_VALID,
};

#pragma pack(push, 1)

struct nkfs_host_work {
	struct work_struct	work;
	struct nkfs_host		*host;
	void			*data;
};

struct nkfs_host {
	struct nkfs_obj_id	net_id;
	struct nkfs_obj_id	host_id;
	struct rb_root		neighs;
	rwlock_t		neighs_lock;
	struct list_head	neigh_list;
	struct timer_list 	timer;
	struct workqueue_struct *wq;
	int			neighs_active;
	int			stopping;
	struct rb_root		host_ids;
	rwlock_t		host_inkfs_lock;
	int			host_inkfs_active;
};

struct nkfs_host_id {
	atomic_t		ref;
	struct nkfs_host		*host;
	struct list_head	neigh_list;
	rwlock_t		neigh_list_lock;
	struct nkfs_obj_id	host_id;
	struct rb_node		host_inkfs_link;
};

struct nkfs_neigh {
	struct list_head	neigh_list;
	struct list_head	host_id_list;
	atomic_t		ref;
	struct nkfs_host_id	*host_id;
	struct rb_node		neighs_link;
	struct nkfs_host		*host;
	struct nkfs_con		*con;
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

void nkfs_neigh_ref(struct nkfs_neigh *neigh);
void nkfs_neigh_deref(struct nkfs_neigh *neigh);

#define NEIGH_REF(n)	\
	nkfs_neigh_ref(n);

#define NEIGH_DEREF(n)	\
	nkfs_neigh_deref(n);

void nkfs_host_id_ref(struct nkfs_host_id *host_id);
void nkfs_host_id_deref(struct nkfs_host_id *host_id);

#define HOST_ID_REF(hid)							\
	do {									\
		nkfs_host_id_ref((hid));						\
		KLOG(KL_DBG, "ref hid %p ref %d",				\
			(hid), atomic_read(&(hid)->ref));			\
	} while (0);

#define HOST_ID_DEREF(hid)							\
	do {									\
		KLOG(KL_DBG, "deref hid %p ref %d",				\
			(hid), atomic_read(&(hid)->ref));			\
		nkfs_host_id_deref((hid));					\
	} while (0);

int nkfs_route_init(void);
void nkfs_route_finit(void);

int nkfs_neigh_add(u32 d_ip, int d_port, u32 s_ip, int s_port);
int nkfs_neigh_remove(u32 d_ip, int d_port);

int nkfs_neigh_handshake(struct nkfs_obj_id *net_id,
	struct nkfs_obj_id *host_id, u32 d_ip, int d_port,
	u32 s_ip, int s_port, struct nkfs_obj_id *reply_host_id);
