#!/bin/bash
. scripts/common.sh
log "start"
exec scripts/load_mods.sh
exec scripts/loop_dev_create.sh
exec bin/nkfs_ctl dev_add -d /dev/loop10 -f
exec bin/nkfs_ctl dev_add -d /dev/loop11 -f
exec bin/nkfs_ctl dev_add -d /dev/loop12 -f
exec iptables -F
exec bin/nkfs_ctl srv_start -b 127.0.0.1 -e 127.0.0.1 -p 9111
