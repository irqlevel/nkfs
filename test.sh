#!/bin/bash
echo "test"
sync
./start.sh
bin/ds_client obj_test
./stop.sh
