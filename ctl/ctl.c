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

int ds_get_object(struct ds_obj_id *obj_id, u64 off,
	void *buf, u32 len, u32 *pread)
{
	int err = -EINVAL;
	struct ds_ctl cmd;
	int fd;

	err = ds_ctl_open(&fd);
	if (err)
		return err;

	memset(&cmd, 0, sizeof(cmd));
	ds_obj_id_copy(&cmd.u.get_obj.obj_id, obj_id);
	cmd.u.get_obj.buf = buf;
	cmd.u.get_obj.len = len;
	cmd.u.get_obj.off = off;

	err = ioctl(fd, IOCTL_DS_GET_OBJ, &cmd);
	if (err)
		goto out;

	err = cmd.err;
	if (err)
		goto out;
	*pread = cmd.u.get_obj.read;
out:
	close(fd);
	return err;
}

int ds_put_object(struct ds_obj_id *obj_id, u64 off, void *buf, u32 len)
{
	int err = -EINVAL;
	struct ds_ctl cmd;
	int fd;

	err = ds_ctl_open(&fd);
	if (err)
		return err;

	memset(&cmd, 0, sizeof(cmd));
	ds_obj_id_copy(&cmd.u.put_obj.obj_id, obj_id);
	cmd.u.put_obj.buf = buf;
	cmd.u.put_obj.len = len;
	cmd.u.put_obj.off = off;

	err = ioctl(fd, IOCTL_DS_PUT_OBJ, &cmd);
	if (err)
		goto out;

	err = cmd.err;
	if (err)
		goto out;

out:
	close(fd);
	return err;
}

int ds_delete_object(struct ds_obj_id *obj_id)
{
	int err = -EINVAL;
	struct ds_ctl cmd;
	int fd;

	err = ds_ctl_open(&fd);
	if (err)
		return err;

	memset(&cmd, 0, sizeof(cmd));
	ds_obj_id_copy(&cmd.u.delete_obj.obj_id, obj_id);

	err = ioctl(fd, IOCTL_DS_DELETE_OBJ, &cmd);
	if (err)
		goto out;

	err = cmd.err;
	if (err)
		goto out;

out:
	close(fd);
	return err;
}

int ds_create_object(struct ds_obj_id *pobj_id)
{
	int err = -EINVAL;
	struct ds_ctl cmd;
	int fd;

	err = ds_ctl_open(&fd);
	if (err)
		return err;

	memset(&cmd, 0, sizeof(cmd));
	err = ioctl(fd, IOCTL_DS_CREATE_OBJ, &cmd);
	if (err)
		goto out;

	err = cmd.err;
	if (err)
		goto out;

	ds_obj_id_copy(pobj_id, &cmd.u.create_obj.obj_id);

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
