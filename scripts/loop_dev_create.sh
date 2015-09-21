#!/bin/bash
. scripts/common.sh
log "create loop devs"
exec rm -f lfile10
exec rm -f lfile11
exec rm -f lfile12
exec dd if=/dev/zero of=lfile10 bs=1M count=100
exec dd if=/dev/zero of=lfile11 bs=1M count=100
exec dd if=/dev/zero of=lfile12 bs=1M count=100
exec sync
exec sync
exec losetup -d /dev/loop10
exec losetup -d /dev/loop11
exec losetup -d /dev/loop12
exec losetup /dev/loop10 lfile10
exec losetup /dev/loop11 lfile11
exec losetup /dev/loop12 lfile12
