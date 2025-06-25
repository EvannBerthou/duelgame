#!/bin/sh

set -xe

gcc -Wall -Wextra server.c common.c -o server -lm
