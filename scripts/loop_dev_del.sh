#!/bin/bash
. scripts/common.sh
log "remove loop devs"
exec losetup -d /dev/loop10
exec losetup -d /dev/loop11
exec losetup -d /dev/loop12
exec rm -f lfile10
exec rm -f lfile11
exec rm -f lfile12
