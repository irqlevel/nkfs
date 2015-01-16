#!/bin/bash
echo "NET TEST"
sync
./load_mods.sh
./loop_dev_create.sh
bin/ds_ctl dev_add -d /dev/loop0 -f
bin/ds_ctl srv_start -s 127.0.0.1 -p 8000
bin/ds_client obj_test
bin/ds_ctl dev_rem -d /dev/loop0
bin/ds_ctl srv_stop -s 127.0.0.1 -p 8000
./unload_mods.sh
./loop_dev_del.sh
