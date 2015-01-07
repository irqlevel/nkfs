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

#include <include/ds_cmd.h>
#include <crtlib/include/crtlib.h>
#include <utils/ucrt/include/ucrt.h>

#define SERVER_START_OPT "--server_start"
#define SERVER_STOP_OPT "--server_stop"
#define DEV_ADD_OPT "--dev_add"
#define DEV_REM_OPT "--dev_rem"
#define DEV_QUERY_OPT "--dev_query"
#define DEV_FORMAT_OPT "--format"

#define DEV_OBJ_TEST_OPT "--dev_obj_test"

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

static int ds_dev_add(const char *dev_name, int format)
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

static int ds_dev_rem(const char *dev_name)
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

static int ds_dev_query(const char *dev_name,
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

static int ds_obj_insert(struct ds_obj_id *sb_id, struct ds_obj_id *obj_id,
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

static int ds_obj_find(struct ds_obj_id *sb_id, struct ds_obj_id *obj_id,
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

static int ds_obj_delete(struct ds_obj_id *sb_id, struct ds_obj_id *obj_id)
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

static int ds_sb_obj_test(struct ds_obj_id *sb_id)
{
	int err;
	struct ds_obj_id *obj_id;
	u64 value, val_find;

	obj_id = ds_obj_id_create();
	if (!obj_id) {
		err = -ENOMEM;
		goto out;
	}
	value = get_random_u64();

	err = ds_obj_insert(sb_id, obj_id, value, 0);
	if (err) {
		goto cleanup;
	} 

	err = ds_obj_find(sb_id, obj_id, &val_find);
	if (err) {
		goto cleanup;
	} 

	if (value != val_find) {
		goto cleanup;	
	}

	err = ds_obj_delete(sb_id, obj_id);
	if (err) {
		goto cleanup;
	}

cleanup:
	crt_free(obj_id);
out:
	return err;
}

static int ds_dev_obj_test(const char *dev_name)
{
	struct ds_obj_id sb_id;
	int err;
	int i;
	
	err = ds_dev_query(dev_name, &sb_id);
	if (err)
		goto out;
	
	for (i = 0; i < 50000; i++)
		err = ds_sb_obj_test(&sb_id);
out:
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
		int format = 0;
		if (argc < 3) {
			usage();
			err = -EINVAL;
			goto out;
		}
		dev_name = argv[2];
		if (argc > 3 && (strncmp(argv[3], DEV_FORMAT_OPT, strlen(DEV_FORMAT_OPT)+1) == 0))
			format = 1;
		printf("adding dev=%s format=%d\n", dev_name, format);
		err = ds_dev_add(dev_name, format);
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
	} else if (strncmp(argv[1], DEV_QUERY_OPT, strlen(DEV_QUERY_OPT) + 1) == 0) {
		const char *dev_name = NULL;
		struct ds_obj_id sb_id;
		if (argc != 3) {
			usage();
			err = -EINVAL;
			goto out;
		}
		dev_name = argv[2];
		printf("query dev=%s\n", dev_name);
		err = ds_dev_query(dev_name, &sb_id);
		if (!err) {
			char *ssb_id = NULL;
			ssb_id = ds_obj_id_to_str(&sb_id);
			if (!ssb_id) {
				err = -ENOMEM;
				goto out;		
			}
			printf("queried dev=%s sb_id=%s\n", dev_name, ssb_id);
			if (ssb_id)
				crt_free(ssb_id);
	
		}
		goto out;
	} else if (strncmp(argv[1], DEV_OBJ_TEST_OPT, strlen(DEV_OBJ_TEST_OPT) + 1) == 0) {
		const char *dev_name = NULL;
		if (argc != 3) {
			usage();
			err = -EINVAL;
			goto out;
		}
		dev_name = argv[2];
		printf("obj test dev=%s\n", dev_name);
		err = ds_dev_obj_test(dev_name);
		if (!err) {
			printf("obj test dev=%s PASSED\n", dev_name);	
		} else {
			printf("obj test dev=%s FAILED err %d\n", dev_name, err);		
		}
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

