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
	printf("Usage: %s [-d device] [-f format] [-b bind ip] [-e ext ip] [-p port]"
		" command{dev_add, dev_rem, dev_query, srv_start, srv_stop,"
		" neigh_add, neigh_remove, neigh_info, klog_sync}\n",
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

static int output_neigh_info(struct nkfs_neigh_info *neigh)
{
	char *host_id;
	struct in_addr addr;

	host_id = nkfs_obj_id_str(&neigh->host_id);
	if (!host_id)
		return -ENOMEM;

	addr.s_addr = neigh->ip;
	printf("ip : %s\n", inet_ntoa(addr));
	printf("port : %d\n", neigh->port);
	printf("host_id : %s\n", host_id);
	printf("state : %lx\n", neigh->state);
	printf("hbt_delay : %llu\n", (unsigned long long)neigh->hbt_delay);
	printf("hbt_time : %llu\n", (unsigned long long)neigh->hbt_time);

	crt_free(host_id);
	return 0;
}


static int do_cmd(char *prog, char *cmd, char *bind_ip_s, char *ext_ip_s, int port,
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
		struct in_addr bind_addr, ext_addr;	
		u32 bind_ip, ext_ip;

		if (port <= 0 || port > 64000) {
			printf("port %d invalid\n", port);
			usage(prog);
			return -EINVAL;
		}

		if (!bind_ip_s || !ext_ip_s) {
			printf("bind and ext ip's not specified\n");
			usage(prog);
			return -EINVAL;
		}

		if (!inet_aton(bind_ip_s, &bind_addr)) {
			printf("bind ip %s is not valid\n", bind_ip_s);
			usage(prog);
			return -EINVAL;
		}
	
		if (!inet_aton(ext_ip_s, &ext_addr)) {
			printf("ext ip %s is not valid\n", ext_ip_s);
			usage(prog);
			return -EINVAL;
		}

		ext_ip = ntohl(ext_addr.s_addr);
		bind_ip = ntohl(bind_addr.s_addr);
		if (ext_ip == 0) {
			printf("please specify exact(not 0) ext ip, your ip is %s\n",
					ext_ip_s);
			usage(prog);
			return -EINVAL;
		}

		err = nkfs_server_start(bind_ip, ext_ip, port);
		if (err) {
			printf("cant start server at %s-%s:%d err %d\n",
				bind_ip_s, ext_ip_s, port, err);
		}
	} else if (cmd_equal(cmd, "srv_stop")) {
		struct in_addr bind_addr;	
		u32 bind_ip;

		if (port <= 0 || port > 64000) {
			printf("port %d invalid\n", port);
			usage(prog);
			return -EINVAL;
		}

		if (!bind_ip_s) {
			printf("bind and ext ip's not specified\n");
			usage(prog);
			return -EINVAL;
		}

		if (!inet_aton(bind_ip_s, &bind_addr)) {
			printf("bind ip %s is not valid\n", bind_ip_s);
			usage(prog);
			return -EINVAL;
		}
	
		bind_ip = ntohl(bind_addr.s_addr);
		err = nkfs_server_stop(bind_ip, port);
		if (err) {
			printf("cant stop server %s:%d err %d\n",
				bind_ip_s, port, err);
		}
	} else if (cmd_equal(cmd, "neigh_add")) {
		struct in_addr addr;	
		u32 ip;

		if (port <= 0 || port > 64000) {
			printf("port %d invalid\n", port);
			usage(prog);
			return -EINVAL;
		}

		if (!ext_ip_s) {
			printf("ip not specified\n");
			usage(prog);
			return -EINVAL;
		}

		if (!inet_aton(ext_ip_s, &addr)) {
			printf("ip %s is not valid\n", ext_ip_s);
			usage(prog);
			return -EINVAL;
		}

		ip = ntohl(addr.s_addr);
		err = nkfs_neigh_add(ip, port);
		if (err) {
			printf("cant add neighbour ->%s:%d err %d\n",
				ext_ip_s, port, err);
		}
	} else if (cmd_equal(cmd, "neigh_remove")) {
		struct in_addr addr;	
		u32 ip;

		if (port <= 0 || port > 64000) {
			printf("port %d invalid\n", port);
			usage(prog);
			return -EINVAL;
		}

		if (!ext_ip_s) {
			printf("ip not specified\n");
			usage(prog);
			return -EINVAL;
		}

		if (!inet_aton(ext_ip_s, &addr)) {
			printf("ip %s is not valid\n", ext_ip_s);
			usage(prog);
			return -EINVAL;
		}

		ip = ntohl(addr.s_addr);
		err = nkfs_neigh_remove(ip, port);
		if (err) {
			printf("cant remove neighbour %s:%d err %d\n",
				ext_ip_s, port, err);
		}
	} else if (cmd_equal(cmd, "klog_sync")) {
		err = nkfs_klog_ctl(0, 1);
		if (err)
			printf("can't sync klog err %d\n", err);		
	} else if (cmd_equal(cmd, "neigh_info")) {
		struct nkfs_neigh_info neighs[NKFS_ROUTE_MAX_NEIGHS];
		int nr_neighs, i;

		err = nkfs_neigh_info(neighs, ARRAY_SIZE(neighs), &nr_neighs);
		if (err) {
			printf("cant get neigh info err %d\n", err);
			goto out;
		}

		for (i = 0; i < nr_neighs; i++) {
			output_neigh_info(&neighs[i]);
			printf("\n");
		}
	} else {
		printf("unknown cmd\n");
		usage(prog);
		return -EINVAL;
	}
out:
	return err;	
}

int main(int argc, char *argv[])
{
    	int err = -EINVAL;
 	int format = 0;
	int opt;
	char *dev_name = NULL;
	char *cmd = NULL;
	char *bind_ip_s = NULL;
	char *ext_ip_s = NULL;
	char *prog = argv[0];
	int port = -1;

	prepare_logging();

	while ((opt = getopt(argc, argv, "b:e:p:fd:")) != -1) {
		switch (opt) {
			case 'f':
				format = 1;
				break;
			case 'd':
				dev_name = optarg;
				break;
			case 'b':
				bind_ip_s = optarg;
				break;
			case 'e':
				ext_ip_s = optarg;
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
	err = do_cmd(prog, cmd, bind_ip_s, ext_ip_s, port,
		dev_name, format);
	return err;
}
