#include "ctl.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static void prepare_logging()
{
	crt_log_set_path(NULL, "nkfs_ctl.log");
	crt_log_set_level(CL_DBG);
	crt_log_enable_printf(1);
}

static void usage(char *program)
{
	printf("Usage: %s [-d device] [-f format] [-s srv ip] [-p srv port]"
		" command{dev_add, dev_rem, dev_query, srv_start, srv_stop,"
		" neigh_add, neigh_remove, klog_sync}\n",
		program);
}

static int cmd_equal(char *cmd, const char *val)
{
	return (strncmp(cmd, val, strlen(val) + 1) == 0) ? 1 : 0;
}

static int output_dev_info(struct nkfs_dev_info *info)
{
	char *hex_sb_id;
	hex_sb_id = nkfs_obj_id_str(&info->sb_id);
	if (!hex_sb_id)
		return -ENOMEM;

	printf("dev_name : %s\n", info->dev_name);
	printf("major : %u\n", info->major);
	printf("minor : %u\n", info->minor);	
	printf("sb_id : %s\n", hex_sb_id);	
	printf("size : %llu\n", (unsigned long long)info->size);
	printf("used_size : %llu\n", (unsigned long long)info->used_size);
	printf("free_size : %llu\n", (unsigned long long)info->free_size);
	printf("bsize : %u\n", info->bsize);
	printf("blocks : %llu\n", (unsigned long long)info->blocks);
	printf("used_blocks : %llu\n", (unsigned long long)info->used_blocks);
	printf("inodes_tree_block : %llu\n",
		(unsigned long long)info->inodes_tree_block);
	printf("bm_block : %llu\n", (unsigned long long)info->bm_block);
	printf("bm_blocks : %llu\n", (unsigned long long)info->bm_blocks);
	crt_free(hex_sb_id);
	return 0;
}

static int do_cmd(char *prog, char *cmd, char *server, int server_port,
	char *dev_name, int format)
{
	int err;
	if (cmd_equal(cmd, "dev_add")) {
		if (dev_name == NULL) {
			printf("device not specified\n");
			usage(prog);
			return -EINVAL;
		}
		err = nkfs_dev_add(dev_name, format);
		if (err) {
			printf("cant add device %s err %d\n", dev_name, err);
		}
	} else if (cmd_equal(cmd, "dev_rem")) {
		if (dev_name == NULL) {
			printf("device not specified\n");
			usage(prog);
			return -EINVAL;
		}
		err = nkfs_dev_rem(dev_name);
		if (err) {
			printf("cant rem device %s err %d\n", dev_name, err);
		}
	} else if (cmd_equal(cmd, "dev_query")) {
		struct nkfs_dev_info info;
		if (dev_name == NULL) {
			printf("device not specified\n");
			usage(prog);
			return -EINVAL;
		}
		err = nkfs_dev_query(dev_name, &info);
		if (err) {
			printf("cant query device %s err %d\n", dev_name, err);
			return err;
		}
		err = output_dev_info(&info);
		if (err) {
			printf("cant output dev info err %d\n", err);
		}
	} else if (cmd_equal(cmd, "srv_start")) {
		struct in_addr addr;	
		u32 ip;

		if (server_port <= 0 || server_port > 64000) {
			printf("server port %d invalid\n", server_port);
			usage(prog);
			return -EINVAL;
		}

		if (!server) {
			printf("server ip not specified\n");
			usage(prog);
			return -EINVAL;
		}

		if (!inet_aton(server, &addr)) {
			printf("server ip %s is not valid\n", server);
			usage(prog);
			return -EINVAL;
		}
		ip = ntohl(addr.s_addr);

		if (ip == 0) {
			printf("please specify exact ip, your ip is %s\n", server);
			usage(prog);
			return -EINVAL;
		}

		err = nkfs_server_start(ip, server_port);
		if (err) {
			printf("cant start server %s:%d err %d\n",
				server, server_port, err);
		}
	} else if (cmd_equal(cmd, "srv_stop")) {
		struct in_addr addr;	
		u32 ip;

		if (server_port <= 0 || server_port > 64000) {
			printf("server port %d invalid\n", server_port);
			usage(prog);
			return -EINVAL;
		}

		if (!server) {
			printf("server ip not specified\n");
			usage(prog);
			return -EINVAL;
		}

		if (!inet_aton(server, &addr)) {
			printf("server ip %s is not valid\n", server);
			usage(prog);
			return -EINVAL;
		}
		ip = ntohl(addr.s_addr);
		err = nkfs_server_stop(ip, server_port);
		if (err) {
			printf("cant stop server %s:%d err %d\n",
				server, server_port, err);
		}
	} else if (cmd_equal(cmd, "neigh_add")) {
		struct in_addr s_addr;	
		u32 s_ip;

		if (server_port <= 0 || server_port > 64000) {
			printf("server port %d invalid\n", server_port);
			usage(prog);
			return -EINVAL;
		}

		if (!server) {
			printf("server ip not specified\n");
			usage(prog);
			return -EINVAL;
		}

		if (!inet_aton(server, &s_addr)) {
			printf("server ip %s is not valid\n", server);
			usage(prog);
			return -EINVAL;
		}

		s_ip = ntohl(s_addr.s_addr);
		err = nkfs_neigh_add(s_ip, server_port);
		if (err) {
			printf("cant add neighbour ->%s:%d err %d\n",
				server, server_port, err);
		}
	} else if (cmd_equal(cmd, "neigh_remove")) {
		struct in_addr s_addr;	
		u32 s_ip;

		if (server_port <= 0 || server_port > 64000) {
			printf("server port %d invalid\n", server_port);
			usage(prog);
			return -EINVAL;
		}

		if (!server) {
			printf("server ip not specified\n");
			usage(prog);
			return -EINVAL;
		}

		if (!inet_aton(server, &s_addr)) {
			printf("server ip %s is not valid\n", server);
			usage(prog);
			return -EINVAL;
		}
		s_ip = ntohl(s_addr.s_addr);
		err = nkfs_neigh_remove(s_ip, server_port);
		if (err) {
			printf("cant remove neighbour %s:%d err %d\n",
				server, server_port, err);
		}
	} else if (cmd_equal(cmd, "klog_sync")) {
		err = nkfs_klog_ctl(0, 1);
		if (err)
			printf("can't sync klog err %d\n", err);		
	} else {
		printf("unknown cmd\n");
		usage(prog);
		return -EINVAL;
	}

	return err;	
}

int main(int argc, char *argv[])
{
    	int err = -EINVAL;
 	int format = 0;
	int opt;
	char *dev_name = NULL;
	char *cmd = NULL;
	char *server = NULL;
	char *prog = argv[0];
	int server_port = NKFS_SRV_PORT;

	prepare_logging();

	while ((opt = getopt(argc, argv, "r:t:s:p:fd:")) != -1) {
		switch (opt) {
			case 'f':
				format = 1;
				break;
			case 'd':
				dev_name = optarg;
				break;
			case 's':
				server = optarg;
				break;
			case 'p':
				server_port = atoi(optarg);				
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
	err = do_cmd(prog, cmd, server, server_port,
		dev_name, format);
	return err;
}
