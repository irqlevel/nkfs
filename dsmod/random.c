#include <ds_priv.h>

#define __SUBCOMPONENT__ "ds-random"
static struct file *dev_random;
static struct file *dev_urandom;

int ds_random_buf_read(void *buf, __u32 len, int urandom)
{
	loff_t off = 0;
	int err;

	err = file_read((urandom) ? dev_urandom : dev_random, buf, len, &off);
	return err;
}

int ds_random_init(void)
{
	dev_random = filp_open("/dev/random", O_RDONLY, 0);
	if (!dev_random)
		return -ENOMEM;
	dev_urandom = filp_open("/dev/urandom", O_RDONLY, 0);
	if (!dev_urandom) {
		fput(dev_random);
		return -ENOMEM;
	}

	return 0;
}

void ds_random_release(void)
{
	fput(dev_random);
	fput(dev_urandom);
}

