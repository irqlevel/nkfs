#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <memory.h>
#include <errno.h>
#include <malloc.h>
#include <unistd.h>
#include <getopt.h>
#include <crt/include/crt.h>
#include <include/ds_client.h>
#include <include/ds_const.h>

#include "test.h"

static void prepare_logging()
{
	/* 
	 * set CLOG log path : NULL and log name ds_client.log
	 * NULL means use current working dir as path
	*/
	crt_log_set_path(NULL, "ds_client.log");
	/* set CLOG log level */
	crt_log_set_level(CL_DBG);
	crt_log_enable_printf(1);
}

static void usage(char *program)
{
	printf("Usage: %s [-f file path] [-i obj id] [-s srv ip]"
		"[-p srv port] command{put, get, query, delete}\n",
		program);
}

static int cmd_equal(char *cmd, const char *val)
{
	return (strncmp(cmd, val, strlen(val) + 1) == 0) ? 1 : 0;
}

static int do_file_put(char *server, int port, char *fpath)
{
	int err, fd;
	int bytes_read;
	int buf_size = 16*4096;
	struct ds_obj_id obj_id;
	struct ds_con con;
	u64 off;
	void *buf;
	char *hex_id = NULL;

	fd = open(fpath, O_RDONLY);
	if (fd < 0) {
		err = errno;
		CLOG(CL_ERR, "cant open file %s err %d", fpath, err);
		return err;
	}
	buf = crt_malloc(buf_size);
	if (!buf) {
		err = -ENOMEM;
		CLOG(CL_ERR, "no mem");
		goto close;
	}

	err = ds_connect(&con, server, port);
	if (err) {
		CLOG(CL_ERR, "cant connect to server %s:%d",
			server, port);
		goto free_buf;
	}

	err = ds_create_object(&con, &obj_id);
	if (err) {
		CLOG(CL_ERR, "cant create object %d", err);
		goto close_con;
	}

	hex_id = ds_obj_id_str(&obj_id);
	if (!hex_id) {
		CLOG(CL_ERR, "cant get string by id");
		err = -EINVAL;
		goto del_obj;
	}

	off = 0;
	while (1) {
		bytes_read = read(fd, buf, buf_size);
		if (bytes_read < 0) {
			err = errno;
			CLOG(CL_ERR, "read err %d", err);
			goto del_obj;
		}
		if (bytes_read == 0)
			break;
		err = ds_put_object(&con, &obj_id, off, buf, bytes_read);
		if (err) {
			CLOG(CL_ERR, "cant put obj off %llu len %d err %d",
				off, bytes_read, err);
			goto del_obj; 
		}
		off+= bytes_read;
	}
	printf("%s\n", hex_id);	
	err = 0;
	goto close_con;
del_obj:
	ds_delete_object(&con, &obj_id);
close_con:
	ds_close(&con);
free_buf:
	crt_free(buf);
close:
	close(fd);
	if (hex_id)
		crt_free(hex_id);

	return err;
}

static int do_file_get(char *server, int port, struct ds_obj_id *obj_id, char *fpath)
{
	int err, fd;
	int buf_size = 16*4096;
	struct ds_con con;
	u64 off;
	int wrote, wrote_bytes;
	u32 read;
	void *buf;

	fd = open(fpath, O_WRONLY|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR);
	if (fd < 0) {
		err = errno;
		CLOG(CL_ERR, "cant open file %s err %d", fpath, err);
		return err;
	}
	buf = crt_malloc(buf_size);
	if (!buf) {
		err = -ENOMEM;
		CLOG(CL_ERR, "no mem");
		goto close;
	}

	err = ds_connect(&con, server, port);
	if (err) {
		CLOG(CL_ERR, "cant connect to server %s:%d",
			server, port);
		goto free_buf;
	}
	off = 0;
	while (1) {
		err = ds_get_object(&con, obj_id, off, buf, buf_size, &read);
		if (err) {
			CLOG(CL_ERR, "cant get obj err %d", err);
			goto close_con;
		}

		if (!read)
			break;

		wrote = 0;
		do {		
			wrote_bytes = write(fd, (char *)buf + wrote,
					read - wrote);
			if (wrote_bytes < 0) {
				CLOG(CL_ERR, "cant wrote err %d", err);
				goto close_con;
			}
			wrote+= wrote_bytes;
		} while (wrote < read);

		off+= read;
	}

	err = 0;
close_con:
	ds_close(&con);
free_buf:
	crt_free(buf);
close:
	close(fd);
	return err;
}

static int output_obj_info(struct ds_obj_info *info)
{
	char *hex_obj_id, *hex_sb_id;
	hex_obj_id = ds_obj_id_str(&info->obj_id);
	if (!hex_obj_id)
		return -ENOMEM;
	hex_sb_id = ds_obj_id_str(&info->sb_id);
	if (!hex_sb_id) {
		crt_free(hex_obj_id);
	}

	printf("obj_id : %s\n", hex_obj_id);
	printf("size : %llu\n", (unsigned long long)info->size);
	printf("block : %llu\n", (unsigned long long)info->block);
	printf("bsize : %u\n", info->bsize);
	printf("device : %s\n", info->dev_name);
	printf("sb_id : %s\n", hex_sb_id);

	crt_free(hex_obj_id);
	crt_free(hex_sb_id);

	return 0;
}

static int do_obj_query(char *server, int port, struct ds_obj_id *id)
{
	int err;
	struct ds_obj_info info;
	struct ds_con con;

	err = ds_connect(&con, server, port);
	if (err) {
		CLOG(CL_ERR, "cant connect to server %s:%d",
			server, port);
	}

	err = ds_query_object(&con, id, &info);
	if (err) {
		CLOG(CL_ERR, "cant query obj err %d", err);
		goto close_con;
	}

	err = output_obj_info(&info);
	if (err) {
		CLOG(CL_ERR, "cant output obj info err %d", err);
		goto close_con;
	}

close_con:
	ds_close(&con);
	return err;
}

static int do_obj_delete(char *server, int port, struct ds_obj_id *id)
{
	int err;
	struct ds_con con;

	err = ds_connect(&con, server, port);
	if (err) {
		CLOG(CL_ERR, "cant connect to server %s:%d",
			server, port);
	}

	err = ds_delete_object(&con, id);
	if (err) {
		CLOG(CL_ERR, "cant delete obj err %d", err);
		goto close_con;
	}

close_con:
	ds_close(&con);
	return err;
}

static int do_cmd(char *prog, char *cmd, char *server, int port,
	char *fpath, char *obj_id)
{
	int err;

	if (cmd_equal(cmd, "put")) {
		if (fpath == NULL) {
			printf("file path not specified\n");
			usage(prog);
			return -EINVAL;
		}
		return do_file_put(server, port, fpath);
	} else if (cmd_equal(cmd, "get")) {
		struct ds_obj_id *id;
		if (fpath == NULL) {
			printf("file path not specified\n");
			usage(prog);
			return -EINVAL;
		}

		if (obj_id == NULL) {
			printf("obj id not specified\n");
			usage(prog);
			return -EINVAL;
		}

		id = ds_obj_id_by_str(obj_id);
		if (id == NULL) {
			printf("cant convert string to obj id\n");
			usage(prog);
			return -EINVAL;
		}
		
		err = do_file_get(server, port, id, fpath);
		crt_free(id);
		return err;
	} else if (cmd_equal(cmd, "query")) {
		struct ds_obj_id *id;
		if (obj_id == NULL) {
			printf("obj id not specified\n");
			usage(prog);
			return -EINVAL;
		}

		id = ds_obj_id_by_str(obj_id);
		if (id == NULL) {
			printf("cant convert string to obj id\n");
			usage(prog);
			return -EINVAL;
		}
		
		err = do_obj_query(server, port, id);
		crt_free(id);
		return err;
	} else if (cmd_equal(cmd, "delete")) {
		struct ds_obj_id *id;
		if (obj_id == NULL) {
			printf("obj id not specified\n");
			usage(prog);
			return -EINVAL;
		}

		id = ds_obj_id_by_str(obj_id);
		if (id == NULL) {
			printf("cant convert string to obj id\n");
			usage(prog);
			return -EINVAL;
		}
		
		err = do_obj_delete(server, port, id);
		crt_free(id);
		return err;
	} else if (cmd_equal(cmd, "obj_test")) {
		err = obj_test(30, 1, 51145);
	} else {
		printf("unknown cmd %s\n", cmd);
		usage(prog);
		return -EINVAL;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	int err;
	int opt;
	char *fpath = NULL;
	char *obj_id = NULL;
	char *cmd = NULL;
	char *server = "127.0.0.1";
	char *prog = argv[0];
	int port = DS_SRV_PORT;

	prepare_logging();
	while ((opt = getopt(argc, argv, "f:i:s:p:")) != -1) {
		switch (opt) {
			case 'f':
				fpath = optarg;
				break;
			case 'i':
				obj_id = optarg;
				break;
			case 's':
				server = optarg;
				break;
			case 'p':
				port = atoi(optarg);				
				break;
			default:
				usage(prog);
				exit(-EINVAL);
		}
	}

	if (optind >= argc) {
		printf("expected more args\n");
		usage(prog);
		exit(-EINVAL);
	}	
	cmd = argv[optind];
	err = do_cmd(prog, cmd, server, port, fpath, obj_id);
	return err;
}
