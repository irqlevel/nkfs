#!/bin/bash
. scripts/common.sh
log "create loop devs"
exec rm -f lfile0
exec rm -f lfile1
exec rm -f lfile2
exec dd if=/dev/zero of=lfile0 bs=1M count=100
exec dd if=/dev/zero of=lfile1 bs=1M count=100
exec dd if=/dev/zero of=lfile2 bs=1M count=100
exec losetup -d /dev/loop0
exec losetup -d /dev/loop1
exec losetup -d /dev/loop2
exec losetup /dev/loop0 lfile0
exec losetup /dev/loop1 lfile1
exec losetup /dev/loop2 lfile2
