#!/bin/bash
echo "create loop devs"
sync
rm -f lfile0
rm -f lfile1
rm -f lfile2
dd if=/dev/zero of=lfile0 bs=1M count=100
dd if=/dev/zero of=lfile1 bs=1M count=100
dd if=/dev/zero of=lfile2 bs=1M count=100
losetup -d /dev/loop0
losetup -d /dev/loop1
losetup -d /dev/loop2
losetup /dev/loop0 lfile0
losetup /dev/loop1 lfile1
losetup /dev/loop2 lfile2
