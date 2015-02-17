#!/bin/bash
. scripts/common.sh
log "stop"
exec scripts/unload_mods.sh
exec scripts/loop_dev_del.sh
