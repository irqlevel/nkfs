#!/bin/bash
rm -rf /var/log/ds.log
rm -rf lfile
dd if=/dev/zero of=lfile bs=1M count=20
rmmod ds
insmod bin/ds.ko
losetup -d /dev/loop0
losetup /dev/loop0 lfile
bin/ds_ctl --dev_add /dev/loop0 --format
bin/ds_ctl --dev_obj_test /dev/loop0
bin/ds_ctl --dev_rem /dev/loop0
rmmod ds
