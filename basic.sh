#!/bin/bash
insmod bin/ds.ko
bin/ds_ctl --server_start 8005
sleep 20
bin/ds_ctl --server_stop 8005
rmmod ds

