#!/bin/bash
echo "loop dev del"
sync
losetup -d /dev/loop0
rm -rf lfile
