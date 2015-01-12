#include "ctl.h"
#include <stdlib.h>

static int ds_ctl_open(int *fd)
{
	int dev_fd = -1;
	int err = -EINVAL;

	dev_fd = open("/dev/ds_ctl", 0);
	if (dev_fd == -1) {
		err = errno;
		printf("cant open ds ctl device, err=%d\n", err);
		return err;
	}
	*fd = dev_fd;
	return 0;
}

int ds_dev_add(const char *dev_name, int format)
{
	int err = -EINVAL;
	struct ds_ctl cmd;
	int fd;

	err = ds_ctl_open(&fd);
	if (err)
		return err;

	memset(&cmd, 0, sizeof(cmd));
	snprintf(cmd.u.dev_add.dev_name, sizeof(cmd.u.dev_add.dev_name),
		"%s", dev_name);

	cmd.u.dev_add.format = format;
	err = ioctl(fd, IOCTL_DS_DEV_ADD, &cmd);
	if (err)
		goto out;

	err = cmd.err;
	if (err)
		goto out;

out:
	close(fd);
	return err;
}

int ds_dev_rem(const char *dev_name)
{
	int err = -EINVAL;
	struct ds_ctl cmd;
	int fd;

	err = ds_ctl_open(&fd);
	if (err)
		return err;

	memset(&cmd, 0, sizeof(cmd));
	snprintf(cmd.u.dev_remove.dev_name, sizeof(cmd.u.dev_remove.dev_name),
		"%s", dev_name);
	err = ioctl(fd, IOCTL_DS_DEV_REMOVE, &cmd);
	if (err)
		goto out;

	err = cmd.err;
	if (err)
		goto out;

out:
	close(fd);
	return err;
}

int ds_dev_query(const char *dev_name,
			struct ds_obj_id *psb_id)
{
	int err = -EINVAL;
	struct ds_ctl cmd;
	int fd;

	err = ds_ctl_open(&fd);
	if (err)
		return err;

	memset(&cmd, 0, sizeof(cmd));
	snprintf(cmd.u.dev_query.dev_name, sizeof(cmd.u.dev_query.dev_name),
		"%s", dev_name);
	err = ioctl(fd, IOCTL_DS_DEV_QUERY, &cmd);
	if (err)
		goto out;

	err = cmd.err;
	if (err)
		goto out;

	memcpy(psb_id, &cmd.u.dev_query.sb_id, sizeof(struct ds_obj_id));
	err = 0;

out:
	close(fd);
	return err;
}

int ds_obj_write(struct ds_obj_id *sb_id, struct ds_obj_id *obj_id,
		u64 off, void *buf, u32 len)
{
	int err = -EINVAL;
	struct ds_ctl cmd;
	int fd;

	err = ds_ctl_open(&fd);
	if (err)
		return err;

	memset(&cmd, 0, sizeof(cmd));
	memcpy(&cmd.u.obj_write.sb_id, sb_id, sizeof(*sb_id));
	memcpy(&cmd.u.obj_write.obj_id, obj_id, sizeof(*obj_id));
	cmd.u.obj_write.buf = buf;
	cmd.u.obj_write.len = len;
	cmd.u.obj_write.off = off;

	err = ioctl(fd, IOCTL_DS_OBJ_WRITE, &cmd);
	if (err)
		goto out;

	err = cmd.err;
	if (err)
		goto out;

out:
	close(fd);
	return err;
}

int ds_obj_read(struct ds_obj_id *sb_id, struct ds_obj_id *obj_id,
		u64 off, void *buf, u32 len)
{
	int err = -EINVAL;
	struct ds_ctl cmd;
	int fd;

	err = ds_ctl_open(&fd);
	if (err)
		return err;

	memset(&cmd, 0, sizeof(cmd));
	memcpy(&cmd.u.obj_read.sb_id, sb_id, sizeof(*sb_id));
	memcpy(&cmd.u.obj_read.obj_id, obj_id, sizeof(*obj_id));
	cmd.u.obj_read.buf = buf;
	cmd.u.obj_read.len = len;
	cmd.u.obj_read.off = off;

	err = ioctl(fd, IOCTL_DS_OBJ_READ, &cmd);
	if (err)
		goto out;

	err = cmd.err;
	if (err)
		goto out;

out:
	close(fd);
	return err;
}

int ds_obj_delete(struct ds_obj_id *sb_id, struct ds_obj_id *obj_id)
{
	int err = -EINVAL;
	struct ds_ctl cmd;
	int fd;

	err = ds_ctl_open(&fd);
	if (err)
		return err;

	memset(&cmd, 0, sizeof(cmd));
	memcpy(&cmd.u.obj_delete.sb_id, sb_id, sizeof(*sb_id));
	memcpy(&cmd.u.obj_delete.obj_id, obj_id, sizeof(*obj_id));

	err = ioctl(fd, IOCTL_DS_OBJ_DELETE, &cmd);
	if (err)
		goto out;

	err = cmd.err;
	if (err)
		goto out;

out:
	close(fd);
	return err;
}

int ds_obj_tree_check(struct ds_obj_id *sb_id)
{
	int err = -EINVAL;
	struct ds_ctl cmd;
	int fd;

	err = ds_ctl_open(&fd);
	if (err)
		return err;

	memset(&cmd, 0, sizeof(cmd));
	memcpy(&cmd.u.obj_tree_check.sb_id, sb_id, sizeof(*sb_id));
	err = ioctl(fd, IOCTL_DS_OBJ_TREE_CHECK, &cmd);
	if (err)
		goto out;

	err = cmd.err;
	if (err)
		goto out;

out:
	close(fd);
	return err;
}


int ds_server_stop(u32 ip, int port)
{
	int err = -EINVAL;
	struct ds_ctl cmd;
	int fd;

	err = ds_ctl_open(&fd);
	if (err)
		return err;
	
	memset(&cmd, 0, sizeof(cmd));
	cmd.u.server_stop.ip = ip;
	cmd.u.server_stop.port = port;
	err = ioctl(fd, IOCTL_DS_SRV_STOP, &cmd);
	if (err)
		goto out;

	err = cmd.err;
	if (err)
		goto out;

out:
	close(fd);
	return err;
}

int ds_server_start(u32 ip, int port)
{
	int err = -EINVAL;
	struct ds_ctl cmd;
	int fd;

	err = ds_ctl_open(&fd);
	if (err)
		return err;
	
	memset(&cmd, 0, sizeof(cmd));

	cmd.u.server_start.ip = ip;
	cmd.u.server_start.port = port;
	err = ioctl(fd, IOCTL_DS_SRV_START, &cmd);
	if (err)
		goto out;

	err = cmd.err;
	if (err)
		goto out;

out:
	close(fd);
	return err;
}
