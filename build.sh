#!/bin/sh

set -xe

gcc -Wall -Wextra main.c -o main_game -L ./lib -lraylib -lm -ggdb
