#!/bin/bash
echo "load modules"
sync
insmod bin/ds_crt.ko
insmod bin/ds.ko
