/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NRF_RPC_LOG_H_
#define NRF_RPC_LOG_H_


#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

/**
 * @defgroup nrf_rpc_log Logging functionality for nRF PRC
 * @{
 * @ingroup nrf_rpc
 *
 * @brief Logging functionality for nRF PRC
 */


#ifdef __cplusplus
extern "C" {
#endif

#if defined(MASTER)
#define _NRF_RPC_LOG_SIDE "\033[32mMASTER"
#elif defined(SLAVE)
#define _NRF_RPC_LOG_SIDE "\033[34mSLAVE "
#else
#define _NRF_RPC_LOG_SIDE "      "
#endif

extern int _nrf_rpc_log_level;
void _nrf_rpc_log(int level, const char* format, ...);
void _nrf_rpc_log_dump(int level, const uint8_t *memory, size_t len, const char *text);
char *_nrf_rpc_log_thread();
void nrf_rpc_log_set_thread_name(const char* str);

/**
 * @brief Macro for logging a message with the severity level ERR.
 *
 * @param ... printf-style format string, optionally followed by arguments
 *            to be formatted and inserted in the resulting string.
 */
#define SM_IPT_ERR(text, ...) do { if (_nrf_rpc_log_level >= 1) _nrf_rpc_log(1, _NRF_RPC_LOG_SIDE " %s\033[39m \033[31mERR: " text "\033[39m\n", _nrf_rpc_log_thread(), ##__VA_ARGS__); } while (0)

/**
 * @brief Macro for logging a message with the severity level WRN.
 *
 * @param ... printf-style format string, optionally followed by arguments
 *            to be formatted and inserted in the resulting string.
 */
#define SM_IPT_WRN(text, ...) do { if (_nrf_rpc_log_level >= 2) _nrf_rpc_log(2, _NRF_RPC_LOG_SIDE " %s\033[39m \033[33mWRN: " text "\033[39m\n", _nrf_rpc_log_thread(), ##__VA_ARGS__); } while (0)

/**
 * @brief Macro for logging a message with the severity level INF.
 *
 * @param ... printf-style format string, optionally followed by arguments
 *            to be formatted and inserted in the resulting string.
 */
#define SM_IPT_INF(text, ...) do { if (_nrf_rpc_log_level >= 3) _nrf_rpc_log(3, _NRF_RPC_LOG_SIDE " %s\033[39m \033[32mINF: " text "\033[39m\n", _nrf_rpc_log_thread(), ##__VA_ARGS__); } while (0)

/**
 * @brief Macro for logging a message with the severity level DBG.
 *
 * @param ... printf-style format string, optionally followed by arguments
 *            to be formatted and inserted in the resulting string.
 */
#define SM_IPT_DBG(text, ...) do { if (_nrf_rpc_log_level >= 4) _nrf_rpc_log(4, _NRF_RPC_LOG_SIDE " %s\033[39m DBG: " text "\n", _nrf_rpc_log_thread(), ##__VA_ARGS__); } while (0)

/**
 *@}
 */

#ifdef __cplusplus
}
#endif

#endif /* NRF_RPC_LOG_H_ */
