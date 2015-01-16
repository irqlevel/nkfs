#!/bin/bash
echo "create loop dev /dev/loop0"
sync
rm -rf lfile
dd if=/dev/zero of=lfile bs=1M count=100
losetup -d /dev/loop0
losetup /dev/loop0 lfile
