#!/bin/bash
set -e

rm -f shmem_test shmem_test_slave

function bld {
  gcc \
    -O0 -g \
    ./main.c ./sm_ipt_os.c nrfxlib/sm_ipt/sm_ipt.c \
    -I. --include=conf.h \
    -Inrfxlib/sm_ipt/include \
    -lrt -pthread \
    -Wall \
    -m32 \
    $@
}

bld -DMASTER=1 -o sm_ipt_test
bld -DSLAVE=1 -o sm_ipt_test_slave
