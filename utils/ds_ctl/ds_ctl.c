#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <memory.h>
#include <errno.h>
#include <malloc.h>

#include <ds_cmd.h>

#define SERVER_START_OPT "--server_start"
#define SERVER_STOP_OPT "--server_stop"
#define DEV_ADD_OPT "--dev_add"
#define DEV_REM_OPT "--dev_rem"

static void usage(void)
{
	printf("Usage:\nds_ctl " SERVER_START_OPT " PORT\nds_ctl " SERVER_STOP_OPT " PORT\n"\
		"ds_ctl " DEV_ADD_OPT " DEV_NAME\n"\
		"ds_ctl " DEV_REM_OPT " DEV_NAME\n");
}

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

static int ds_dev_add(const char *dev_name)
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

static int ds_dev_rem(const char *dev_name)
{
	int err = -EINVAL;
	struct ds_cmd cmd;
	int fd;

	err = ds_ctl_open(&fd);
	if (err)
		return err;

	memset(&cmd, 0, sizeof(cmd));
	snprintf(cmd.u.dev_remove.dev_name, sizeof(cmd.u.dev_remove.dev_name), "%s",
		dev_name);
	err = ioctl(fd, IOCTL_DS_DEV_REMOVE, &cmd);
	printf("err %d\n", err);
	if (err)
		goto out;

	err = cmd.err;
	printf("err %d\n", err);
	if (err)
		goto out;

out:
	close(fd);
	return err;
}

static int ds_server_stop(int port)
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


static int ds_server_start(int port)
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

int main(int argc, char *argv[])
{
    	int err = -EINVAL;
    
    	if (argc < 2) {
    		usage();
    	    	err = -EINVAL;
		goto out;
    	}
    
    	if (strncmp(argv[1], SERVER_START_OPT, strlen(SERVER_START_OPT) + 1) == 0) {
		int port = -1;
		if (argc != 3) {
			usage();
			err = -EINVAL;
			goto out;
		}
		port = strtol(argv[2], NULL, 10);
		printf("starting server port=%d\n", port);
		err = ds_server_start(port);
		if (!err)
			printf("started server with port=%d\n", port);
		goto out;
    	} else if (strncmp(argv[1], SERVER_STOP_OPT, strlen(SERVER_STOP_OPT) + 1) == 0) {
		int port = -1;
		if (argc != 3) {
			usage();
			err = -EINVAL;
			goto out;
		}
		port = strtol(argv[2], NULL, 10);
		printf("stopping server port=%d\n", port);
		err = ds_server_stop(port);
		if (!err)
			printf("stopped server port=%d\n", port);
		goto out;	
	} else if (strncmp(argv[1], DEV_ADD_OPT, strlen(DEV_ADD_OPT) + 1) == 0) {
		const char *dev_name = NULL;
		if (argc != 3) {
			usage();
			err = -EINVAL;
			goto out;
		}
		dev_name = argv[2];
		printf("adding dev=%s\n", dev_name);
		err = ds_dev_add(dev_name);
		if (!err)
			printf("added dev=%s\n", dev_name);
		goto out;
	} else if (strncmp(argv[1], DEV_REM_OPT, strlen(DEV_REM_OPT) + 1) == 0) {
		const char *dev_name = NULL;
		if (argc != 3) {
			usage();
			err = -EINVAL;
			goto out;
		}
		dev_name = argv[2];
		printf("removing dev=%s\n", dev_name);
		err = ds_dev_rem(dev_name);
		if (!err)
			printf("removed dev=%s\n", dev_name);
		goto out;
	} else {
		usage();
		err = -EINVAL;
		goto out;
	}

out:
	if (err)
		printf("error - %d\n", err);
	else
		printf("success\n");

	return err;
}

