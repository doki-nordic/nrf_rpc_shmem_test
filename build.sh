#!/bin/bash
set -e

function bld {
  gcc \
    -O0 -g \
    main.c shmem_linux.c nrf_rpc_shmem.c \
    -lrt -pthread \
    $@
}

bld -DMASTER=1 -o shmem_test
bld -DSLAVE=1 -o shmem_test_slave
