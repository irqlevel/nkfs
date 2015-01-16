#!/bin/bash
echo "remove loop dev /dev/loop0"
sync
losetup -d /dev/loop0
losetup -d /dev/loop1
losetup -d /dev/loop2
rm -f lfile0
rm -f lfile1
rm -f lfile2
