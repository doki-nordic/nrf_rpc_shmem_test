#!/bin/bash
set -e

rm -f shmem_test shmem_test_slave

function bld {
  gcc \
    -O0 -g \
    ./main.c ./nrf_rpc_os_linux.c ./nrf_rpc_shmem.c nrfxlib/nrf_rpc/nrf_rpc.c \
    -I. \
    -Inrfxlib/nrf_rpc/include \
    -lrt -pthread \
    -DCONFIG_NRF_RPC_TR_SHMEM \
    $@
}

bld -DMASTER=1 -o shmem_test
bld -DSLAVE=1 -o shmem_test_slave
