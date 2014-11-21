#pragma once

#define DIO_ASYNC		0
#define DIO_SYNC		1

#define DIO_RW_FLAGS_READ	0
#define DIO_CONTEXT_NULL	NULL
#define	DIO_COMPCLB_NULL	NULL

typedef void (*ds_dev_io_complete_t)(int err, struct ds_dev *dev,
		void *context, struct page *page, u64 off,
		int rw_flags);

struct ds_dev_io {
	struct ds_dev 		*dev;
	struct bio    		*bio;
	struct page		*page;
	void			*context;
	u64			off;
	struct list_head	io_list;
	ds_dev_io_complete_t	complete_clb;
	struct completion	*complete;
	int			rw_flags;
	int			err;
};

int ds_dev_io_touch0_page(struct ds_dev *dev);

int ds_dev_io_page(struct ds_dev *dev, void *context, struct page *page, u64 off,
		int rw_flags, int sync, ds_dev_io_complete_t complete_clb);


