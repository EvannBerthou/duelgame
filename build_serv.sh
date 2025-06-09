#!/bin/sh

set -xe

gcc -Wall -Wextra server.c -o server -lm
./server
