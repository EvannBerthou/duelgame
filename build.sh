#!/bin/sh

set -xe

gcc -Wall -Wextra main.c common.c ui.c -o main_game -I ./include -L ./lib -lraylib -lm -ggdb
