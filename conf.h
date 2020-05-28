#ifndef CONF_H_
#define CONF_H_

#include <stdio.h>

#ifdef MASTER

#define NRF_RPC_SHMEM_IN_SIZE 1024
#define NRF_RPC_SHMEM_OUT_SIZE 2048
#define NRF_RPC_IN_ENDPOINTS 4
#define NRF_RPC_OUT_ENDPOINTS 3

#define SIDE "MASTER"
#define IS_MASTER 1
#define IS_SLAVE 0

#else

#define NRF_RPC_SHMEM_OUT_SIZE 1024
#define NRF_RPC_SHMEM_IN_SIZE 2048
#define NRF_RPC_OUT_ENDPOINTS 4
#define NRF_RPC_IN_ENDPOINTS 3

#define SIDE "SLAVE "
#define IS_MASTER 0
#define IS_SLAVE 1

#endif

#define LOG(text, ...) printf("%s  " text "\n", SIDE, ##__VA_ARGS__);

enum nrf_rpc_error_code {
	NRF_RPC_SUCCESS            = 0,
	NRF_RPC_ERR_NO_MEM         = -1,
	NRF_RPC_ERR_INVALID_PARAM  = -2,
	NRF_RPC_ERR_NULL           = -3,
	NRF_RPC_ERR_NOT_SUPPORTED  = -4,
	NRF_RPC_ERR_INTERNAL       = -5,
	NRF_RPC_ERR_OS_ERROR       = -6,
	NRF_RPC_ERR_INVALID_STATE  = -7,
	NRF_RPC_ERR_REMOTE         = -8,
	NRF_RPC_ERR_EMPTY          = -9,
};

#endif // CONF_H_
