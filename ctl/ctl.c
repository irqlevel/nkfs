#include "ctl.h"
#include <stdlib.h>

static int nkfs_ctl_open(int *fd)
{
	int dev_fd = -1;
	int err = -EINVAL;

	dev_fd = open("/dev/"NKFS_CTL_DEV_NAME, 0);
	if (dev_fd == -1) {
		err = errno;
		printf("cant open nkfs ctl device, err=%d\n", err);
		return err;
	}
	*fd = dev_fd;
	return 0;
}

int nkfs_dev_add(const char *dev_name, int format)
{
	int err = -EINVAL;
	struct nkfs_ctl cmd;
	int fd;

	err = nkfs_ctl_open(&fd);
	if (err)
		return err;

	memset(&cmd, 0, sizeof(cmd));
	snprintf(cmd.u.dev_add.dev_name, sizeof(cmd.u.dev_add.dev_name),
		"%s", dev_name);

	cmd.u.dev_add.format = format;
	err = ioctl(fd, IOCTL_NKFS_DEV_ADD, &cmd);
	if (err)
		goto out;

	err = cmd.err;
	if (err)
		goto out;

out:
	close(fd);
	return err;
}

int nkfs_dev_rem(const char *dev_name)
{
	int err = -EINVAL;
	struct nkfs_ctl cmd;
	int fd;

	err = nkfs_ctl_open(&fd);
	if (err)
		return err;

	memset(&cmd, 0, sizeof(cmd));
	snprintf(cmd.u.dev_remove.dev_name, sizeof(cmd.u.dev_remove.dev_name),
		"%s", dev_name);
	err = ioctl(fd, IOCTL_NKFS_DEV_REMOVE, &cmd);
	if (err)
		goto out;

	err = cmd.err;
	if (err)
		goto out;

out:
	close(fd);
	return err;
}

int nkfs_dev_query(const char *dev_name, struct nkfs_dev_info *info)
{
	int err = -EINVAL;
	struct nkfs_ctl cmd;
	int fd;

	err = nkfs_ctl_open(&fd);
	if (err)
		return err;

	memset(&cmd, 0, sizeof(cmd));
	snprintf(cmd.u.dev_query.dev_name, sizeof(cmd.u.dev_query.dev_name),
		"%s", dev_name);
	err = ioctl(fd, IOCTL_NKFS_DEV_QUERY, &cmd);
	if (err)
		goto out;

	err = cmd.err;
	if (err)
		goto out;

	memcpy(info, &cmd.u.dev_query.info, sizeof(*info));
out:
	close(fd);
	return err;
}

int nkfs_server_stop(u32 bind_ip, int port)
{
	int err = -EINVAL;
	struct nkfs_ctl cmd;
	int fd;

	err = nkfs_ctl_open(&fd);
	if (err)
		return err;
	
	memset(&cmd, 0, sizeof(cmd));
	cmd.u.server_stop.bind_ip = bind_ip;
	cmd.u.server_stop.port = port;
	err = ioctl(fd, IOCTL_NKFS_SRV_STOP, &cmd);
	if (err)
		goto out;

	err = cmd.err;
	if (err)
		goto out;

out:
	close(fd);
	return err;
}

int nkfs_server_start(u32 bind_ip, u32 ext_ip, int port)
{
	int err = -EINVAL;
	struct nkfs_ctl cmd;
	int fd;

	err = nkfs_ctl_open(&fd);
	if (err)
		return err;
	
	memset(&cmd, 0, sizeof(cmd));

	cmd.u.server_start.bind_ip = bind_ip;
	cmd.u.server_start.ext_ip = ext_ip;
	cmd.u.server_start.port = port;
	err = ioctl(fd, IOCTL_NKFS_SRV_START, &cmd);
	if (err)
		goto out;

	err = cmd.err;
	if (err)
		goto out;

out:
	close(fd);
	return err;
}

int nkfs_neigh_add(u32 ip, int port)
{
	int err = -EINVAL;
	struct nkfs_ctl cmd;
	int fd;

	err = nkfs_ctl_open(&fd);
	if (err)
		return err;
	
	memset(&cmd, 0, sizeof(cmd));

	cmd.u.neigh_add.ip = ip;
	cmd.u.neigh_add.port = port;
	err = ioctl(fd, IOCTL_NKFS_NEIGH_ADD, &cmd);
	if (err)
		goto out;

	err = cmd.err;
	if (err)
		goto out;
out:
	close(fd);
	return err;
}

int nkfs_neigh_remove(u32 ip, int port)
{

	int err = -EINVAL;
	struct nkfs_ctl cmd;
	int fd;

	err = nkfs_ctl_open(&fd);
	if (err)
		return err;
	
	memset(&cmd, 0, sizeof(cmd));

	cmd.u.neigh_remove.ip = ip;
	cmd.u.neigh_remove.port = port;
	err = ioctl(fd, IOCTL_NKFS_NEIGH_REMOVE, &cmd);
	if (err)
		goto out;

	err = cmd.err;
	if (err)
		goto out;
out:
	close(fd);
	return err;
}

int nkfs_neigh_info(struct nkfs_neigh_info *neighs,
		int max_nr_neighs, int *pnr_neighs)
{

	int err = -EINVAL;
	struct nkfs_ctl cmd;
	int fd;
	int i;

	*pnr_neighs = 0;
	err = nkfs_ctl_open(&fd);
	if (err)
		return err;

	memset(&cmd, 0, sizeof(cmd));

	err = ioctl(fd, IOCTL_NKFS_NEIGH_INFO, &cmd);
	if (err)
		goto out;

	err = cmd.err;
	if (err)
		goto out;

	if (cmd.u.neigh_info.nr_neighs > max_nr_neighs) {
		err = NKFS_E_LIMIT;
		goto out;
	}

	for (i = 0; i < cmd.u.neigh_info.nr_neighs; i++) {
		memcpy(&neighs[i], &cmd.u.neigh_info.neighs[i],
				sizeof(neighs[i]));
	}

	*pnr_neighs = i;
out:
	close(fd);
	return err;
}

int nkfs_klog_ctl(int level, int sync)
{
	int err = -EINVAL;
	struct nkfs_ctl cmd;
	int fd;

	err = nkfs_ctl_open(&fd);
	if (err)
		return err;
	
	memset(&cmd, 0, sizeof(cmd));

	cmd.u.klog_ctl.level = level;
	cmd.u.klog_ctl.sync = sync;
	err = ioctl(fd, IOCTL_NKFS_KLOG_CTL, &cmd);
	if (err)
		goto out;

	err = cmd.err;
	if (err)
		goto out;
out:
	close(fd);
	return err;
}
