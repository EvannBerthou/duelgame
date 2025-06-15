#!/bin/sh

set -xe

killall server || true
killall main_game || true

./build.sh
./build_serv.sh

./server &
sleep 1
./main_game &
sleep 1
./main_game &
