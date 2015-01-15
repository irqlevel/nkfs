#pragma once

enum {
	DS_NEIGH_CONNECTED,
	DS_NEIGH_DOWN
};

struct ds_neigh {
	atomic_t		ref;
	struct ds_obj_id	host_id;
	struct rb_node		neighs_link;
	u32			ip;
	int			port;
	int			state;
};

struct ds_host {
	struct ds_obj_id	net_id;
	struct ds_obj_id	host_id;
	struct rb_root		neighs;
	rwlock_t		neighs_lock;
};
