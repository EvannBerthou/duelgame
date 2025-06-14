#!/bin/sh

set -xe

gcc -Wall -Wextra main.c -o main -L ./lib -lraylib -lm -ggdb
