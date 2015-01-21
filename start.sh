#!/bin/bash
echo "start"
./load_mods.sh
./loop_dev_create.sh
bin/ds_ctl dev_add -d /dev/loop0 -f
bin/ds_ctl dev_add -d /dev/loop1 -f
bin/ds_ctl dev_add -d /dev/loop2 -f
iptables -F
bin/ds_ctl srv_start -s 0.0.0.0 9111
bin/ds_ctl srv_start -s 0.0.0.0 9112
