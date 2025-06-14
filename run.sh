#!/bin/sh

set -xe

killall server || true
killall main || true

./build.sh
./build_serv.sh

./server &
sleep 1
./main &
sleep 1
./main &
