#!/bin/bash
echo "stop"
sync
bin/ds_ctl --dev_rem /dev/loop0
bin/ds_ctl --server_stop 127.0.0.1 8000
rmmod ds
rmmod ds_crt
losetup -d /dev/loop0
rm -rf lfile
