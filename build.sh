#!/bin/sh

set -xe

echo "#define GIT_VERSION \"$(git rev-parse HEAD)\"" > version.h
gcc -Wall -Wextra main.c common.c ui.c -o main_game -I ./include -L ./lib -lraylib -lm -ggdb
