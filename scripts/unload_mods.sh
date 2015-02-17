#!/bin/bash
. scripts/common.sh
log "unload modules"
exec rmmod nkfs
exec rmmod nkfs_crt
