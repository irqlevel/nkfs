#pragma once

#define SECTOR_SHIFT 9

struct ds_dev {
	char			*dev_name;
	struct list_head 	dev_list;
	struct block_device 	*bdev;
	struct task_struct  	*thread;
	struct list_head	io_list;
	spinlock_t		io_lock;
	int			stopping;
};

struct ds_con {
	struct task_struct 	*thread;
	struct socket 		*sock;
	struct list_head	con_list;
	struct ds_server	*server;
};

struct ds_server {
	struct task_struct	*thread;
	struct socket		*sock;
	struct mutex		lock;
	struct list_head	srv_list;
	struct list_head	con_list;
	struct mutex		con_list_lock;
	int			port;
	int			stopping;
};

struct ds_dev_io {
	struct ds_dev 		*dev;
	struct bio    		*bio;
	struct list_head	io_list;
	int			err;
	void			(*clb)(struct ds_dev_io *io);
	struct completion	*complete;
};

int ds_dev_io_touch0_page(struct ds_dev *dev);
int ds_dev_io_page(struct ds_dev *dev, struct page *page, __u64 off,
	int bi_flags, int rw_flags, void (*clb)(struct ds_dev_io *io), int wait);

int ds_dev_add(char *dev_name);
int ds_dev_remove(char *dev_name);
void ds_dev_release_all(void);


int ds_server_start(int port);
int ds_server_stop(int port);
void ds_server_stop_all(void);

