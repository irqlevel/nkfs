#!/bin/bash
echo "start"
./load_mods.sh
./loop_dev_create.sh
bin/nkfs_ctl dev_add -d /dev/loop0 -f
bin/nkfs_ctl dev_add -d /dev/loop1 -f
bin/nkfs_ctl dev_add -d /dev/loop2 -f
iptables -F
bin/nkfs_ctl srv_start -s 0.0.0.0 -p 9111
bin/nkfs_ctl srv_start -s 0.0.0.0 -p 9112
