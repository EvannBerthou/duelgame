#!/bin/sh

set -xe

gcc net_protocol_builder.c -o net_protocol_builder -I.
./net_protocol_builder
