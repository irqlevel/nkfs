#include "ctl.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static void prepare_logging()
{
	crt_log_set_path(NULL, "ds_ctl.log");
	crt_log_set_level(CL_DBG);
	crt_log_enable_printf(1);
}

static void usage(char *program)
{
	printf("Usage: %s [-d device] [-f format] [-s srv ip] [-p srv port]"
		" command{dev_add, dev_rem, srv_start, srv_stop}\n",
		program);
}

static int cmd_equal(char *cmd, const char *val)
{
	return (strncmp(cmd, val, strlen(val) + 1) == 0) ? 1 : 0;
}


static int do_cmd(char *prog, char *cmd, char *server, int port,
	char *dev_name, int format)
{
	int err;
	if (cmd_equal(cmd, "dev_add")) {
		if (dev_name == NULL) {
			printf("device not specified\n");
			usage(prog);
			return -EINVAL;
		}
		err = ds_dev_add(dev_name, format);
		if (err) {
			printf("cant add device %s err %d\n", dev_name, err);
		}
	} else if (cmd_equal(cmd, "dev_rem")) {
		if (dev_name == NULL) {
			printf("device not specified\n");
			usage(prog);
			return -EINVAL;
		}
		err = ds_dev_rem(dev_name);
		if (err) {
			printf("can rem device %s err %d\n", dev_name, err);
		}
	} else if (cmd_equal(cmd, "srv_start")) {
		struct in_addr addr;	
		u32 ip;

		if (port <= 0 || port > 64000) {
			printf("server port %d invalid\n", port);
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
		err = ds_server_start(ip, port);
		if (err) {
			printf("cant start server %s:%d err %d\n",
				server, port, err);
		}
	} else if (cmd_equal(cmd, "srv_stop")) {
		struct in_addr addr;	
		u32 ip;

		if (port <= 0 || port > 64000) {
			printf("server port %d invalid\n", port);
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
		err = ds_server_stop(ip, port);
		if (err) {
			printf("cant stop server %s:%d err %d\n",
				server, port, err);
		}
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
	int port = -1;

	prepare_logging();

	while ((opt = getopt(argc, argv, "s:p:fd:")) != -1) {
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
	err = do_cmd(prog, cmd, server, port, dev_name, format);
	return err;
}
