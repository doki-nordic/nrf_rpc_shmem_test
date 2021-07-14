#!/bin/bash
set -e

export NRF_RPC_LOG_LEVEL=D
./shmem_test &
./shmem_test_slave
