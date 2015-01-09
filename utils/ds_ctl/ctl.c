#include "ctl.h"

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
	struct ds_cmd cmd;
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
	struct ds_cmd cmd;
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
	struct ds_cmd cmd;
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

int ds_obj_insert(struct ds_obj_id *sb_id, struct ds_obj_id *obj_id,
			u64 value, int replace)
{
	int err = -EINVAL;
	struct ds_cmd cmd;
	int fd;

	err = ds_ctl_open(&fd);
	if (err)
		return err;

	memset(&cmd, 0, sizeof(cmd));
	memcpy(&cmd.u.obj_insert.sb_id, sb_id, sizeof(*sb_id));
	memcpy(&cmd.u.obj_insert.obj_id, obj_id, sizeof(*obj_id));
	cmd.u.obj_insert.value = value;
	cmd.u.obj_insert.replace = replace;

	err = ioctl(fd, IOCTL_DS_OBJ_INSERT, &cmd);
	if (err)
		goto out;

	err = cmd.err;
	if (err)
		goto out;
out:
	close(fd);
	return err;
}

int ds_obj_find(struct ds_obj_id *sb_id, struct ds_obj_id *obj_id,
			u64 *pvalue)
{
	int err = -EINVAL;
	struct ds_cmd cmd;
	int fd;

	err = ds_ctl_open(&fd);
	if (err)
		return err;

	memset(&cmd, 0, sizeof(cmd));
	memcpy(&cmd.u.obj_find.sb_id, sb_id, sizeof(*sb_id));
	memcpy(&cmd.u.obj_find.obj_id, obj_id, sizeof(*obj_id));

	err = ioctl(fd, IOCTL_DS_OBJ_FIND, &cmd);
	if (err)
		goto out;

	err = cmd.err;
	if (err)
		goto out;

	*pvalue = cmd.u.obj_find.value;
out:
	close(fd);
	return err;
}

int ds_obj_delete(struct ds_obj_id *sb_id, struct ds_obj_id *obj_id)
{
	int err = -EINVAL;
	struct ds_cmd cmd;
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
	struct ds_cmd cmd;
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


int ds_server_stop(int port)
{
	int err = -EINVAL;
	struct ds_cmd cmd;
	int fd;

	err = ds_ctl_open(&fd);
	if (err)
		return err;
	
	memset(&cmd, 0, sizeof(cmd));

	cmd.u.server_start.port = port;
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

int ds_server_start(int port)
{
	int err = -EINVAL;
	struct ds_cmd cmd;
	int fd;

	err = ds_ctl_open(&fd);
	if (err)
		return err;
	
	memset(&cmd, 0, sizeof(cmd));

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
