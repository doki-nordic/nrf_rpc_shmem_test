#!/bin/bash
set -e

rm -f shmem_test shmem_test_slave

function bld {
  gcc \
    -O0 -g \
    ./main.c ./nrf_rpc_os.c nrfxlib/nrf_rpc/nrf_rpc.c nrfxlib/nrf_rpc/nrf_rpc_shmem.c nrfxlib/nrf_rpc/nrf_rpc_common.c \
    -I. --include=conf.h\
    -Inrfxlib/nrf_rpc/include \
    -lrt -pthread \
    -Wall \
    $@
}

bld -DMASTER=1 -o shmem_test
bld -DSLAVE=1 -o shmem_test_slave
