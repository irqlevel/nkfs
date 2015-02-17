#!/bin/bash
. scripts/common.sh
log "load modules"
exec insmod bin/nkfs_crt.ko
exec insmod bin/nkfs.ko
