#!/bin/bash
echo "remove loop dev /dev/loop0"
sync
losetup -d /dev/loop0
rm -rf lfile
