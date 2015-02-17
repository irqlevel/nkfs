#!/bin/bash
. scripts/common.sh
log "remove loop devs"
exec losetup -d /dev/loop0
exec losetup -d /dev/loop1
exec losetup -d /dev/loop2
exec rm -f lfile0
exec rm -f lfile1
exec rm -f lfile2
