#!/bin/sh
# bitsize and offset differ between rv32 and rv64
exec sed -E 's/bitsize:(32|64)/bitsize:N/; s/offset:[0-9]+/offset:N/'
