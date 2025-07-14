#!/bin/sh

set -xe

killall server || true
killall main_game || true

./build.sh
./build_serv.sh

./server &
sleep 1
./main_game --ip 127.0.0.1 --port 3000 > /dev/null &
sleep 1
./main_game
