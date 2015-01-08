#include "ctl.h"
#include "test.h"

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

