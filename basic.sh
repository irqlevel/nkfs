#!/bin/bash
insmod bin/ds.ko
bin/ds_ctl --server_start 8001
sleep 10
bin/ds_ctl --server_stop 8001
rmmod ds

