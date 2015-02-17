#!/bin/bash
function log {
	local date='['$(date +'%a %Y-%m-%d %H:%M:%S %z')']'
	echo "$date $1"
}

function exec {
	local cmd=$@
	log "CMD:$cmd"
	$cmd
	local status=$?
	if [ $status -ne 0 ]; then
        	log "CMD:$cmd rc:$status"
    	else
		log "CMD:$cmd rc:$status"
	fi
	return $status
}
