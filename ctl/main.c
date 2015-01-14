#include "ctl.h"
#include "test.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERVER_START_OPT "--server_start"
#define SERVER_STOP_OPT "--server_stop"
#define DEV_ADD_OPT "--dev_add"
#define DEV_REM_OPT "--dev_rem"
#define DEV_QUERY_OPT "--dev_query"
#define DEV_FORMAT_OPT "--format"

#define OBJ_TEST_OPT "--obj_test"

static void usage(void)
{
	printf("Usage:\n"\
		"ds_ctl " SERVER_START_OPT " IP PORT\n"\
		"ds_ctl " SERVER_STOP_OPT " IP PORT\n"\
		"ds_ctl " DEV_ADD_OPT " DEV_NAME [--format]\n"\
		"ds_ctl " DEV_REM_OPT " DEV_NAME\n"\
		"ds_ctl " DEV_QUERY_OPT " DEV_NAME\n");
}

static void prepare_logging()
{
	crt_log_set_path(NULL, "ds_client.log");
	crt_log_set_level(CL_DBG);
	crt_log_enable_printf(1);
}

int main(int argc, char *argv[])
{
    	int err = -EINVAL;
 
	prepare_logging(); 
    	if (argc < 2) {
    		usage();
    	    	err = -EINVAL;
		goto out;
    	}
    
    	if (strncmp(argv[1], SERVER_START_OPT, strlen(SERVER_START_OPT) + 1) == 0) {
		int port = -1;
		struct in_addr addr;	
		u32 ip;
	
		if (argc != 4) {
			usage();
			err = -EINVAL;
			goto out;
		}
		if (!inet_aton(argv[2], &addr)) {
			printf("ip %s is not valid\n", argv[2]);
			err = -EINVAL;
			goto out;
		}
		ip = ntohl(addr.s_addr);
		port = strtol(argv[3], NULL, 10);
		if (port <= 0 || port > 64000) {
			printf("port %d invalid\n", port);
			err = -EINVAL;
			goto out;
		}
		
		printf("starting server ip %x port %d\n", ip, port);
		err = ds_server_start(ip, port);
		if (!err)
			printf("started server with port=%d\n", port);
		goto out;
    	} else if (strncmp(argv[1], SERVER_STOP_OPT, strlen(SERVER_STOP_OPT) + 1) == 0) {
		int port = -1;
		struct in_addr addr;	
		u32 ip;

		if (argc != 4) {
			usage();
			err = -EINVAL;
			goto out;
		}
		if (!inet_aton(argv[2], &addr)) {
			printf("ip %s is not valid\n", argv[2]);
			err = -EINVAL;
			goto out;
		}
		ip = ntohl(addr.s_addr);
	
		port = strtol(argv[3], NULL, 10);
		if (port <= 0 || port > 64000) {
			printf("port %d invalid\n", port);
			err = -EINVAL;
			goto out;
		}

		printf("stopping server ip %x port %d\n", ip, port);
		err = ds_server_stop(ip, port);
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
			ssb_id = ds_obj_id_str(&sb_id);
			if (!ssb_id) {
				err = -ENOMEM;
				goto out;		
			}
			printf("queried dev=%s sb_id=%s\n", dev_name, ssb_id);
			if (ssb_id)
				crt_free(ssb_id);
	
		}
		goto out;
	} else if (strncmp(argv[1], OBJ_TEST_OPT, strlen(OBJ_TEST_OPT) + 1) == 0) {
		if (argc != 2) {
			usage();
			err = -EINVAL;
			goto out;
		}
		err = ds_obj_test(10, 1, 17000);
		printf("obj test err %d\n", err);
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

