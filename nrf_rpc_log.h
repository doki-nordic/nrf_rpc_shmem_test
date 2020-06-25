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
#define _NRF_RPC_LOG_SIDE "\033[32mMASTER\033[39m"
#elif defined(SLAVE)
#define _NRF_RPC_LOG_SIDE "\033[34mSLAVE\033[39m "
#else
#define _NRF_RPC_LOG_SIDE "      "
#endif

extern int _nrf_rpc_log_level;
void _nrf_rpc_log(int level, const char* format, ...);
void _nrf_rpc_log_dump(int level, const uint8_t *memory, size_t len, const char *text);

extern __thread const char *_nrf_rpc_name;

/**
 * @brief Macro for logging a message with the severity level ERR.
 *
 * @param ... printf-style format string, optionally followed by arguments
 *            to be formatted and inserted in the resulting string.
 */
#define NRF_RPC_ERR(text, ...) do { if (_nrf_rpc_log_level >= 1) _nrf_rpc_log(1, _NRF_RPC_LOG_SIDE "(%s) \033[31mERR: " text "\033[39m\n", (_nrf_rpc_name ? _nrf_rpc_name : "???"), ##__VA_ARGS__); } while (0)

/**
 * @brief Macro for logging a message with the severity level WRN.
 *
 * @param ... printf-style format string, optionally followed by arguments
 *            to be formatted and inserted in the resulting string.
 */
#define NRF_RPC_WRN(text, ...) do { if (_nrf_rpc_log_level >= 2) _nrf_rpc_log(2, _NRF_RPC_LOG_SIDE "(%s) \033[33mWRN: " text "\033[39m\n", (_nrf_rpc_name ? _nrf_rpc_name : "???"), ##__VA_ARGS__); } while (0)

/**
 * @brief Macro for logging a message with the severity level INF.
 *
 * @param ... printf-style format string, optionally followed by arguments
 *            to be formatted and inserted in the resulting string.
 */
#define NRF_RPC_INF(text, ...) do { if (_nrf_rpc_log_level >= 3) _nrf_rpc_log(3, _NRF_RPC_LOG_SIDE "(%s) \033[32mINF: " text "\033[39m\n", (_nrf_rpc_name ? _nrf_rpc_name : "???"), ##__VA_ARGS__); } while (0)

/**
 * @brief Macro for logging a message with the severity level DBG.
 *
 * @param ... printf-style format string, optionally followed by arguments
 *            to be formatted and inserted in the resulting string.
 */
#define NRF_RPC_DBG(text, ...) do { if (_nrf_rpc_log_level >= 4) _nrf_rpc_log(4, _NRF_RPC_LOG_SIDE "(%s) DBG: " text "\n", (_nrf_rpc_name ? _nrf_rpc_name : "???"), ##__VA_ARGS__); } while (0)

/**
 * @brief Macro for logging a memory dump with the severity level ERR.
 *
 * @param[in] memory Pointer to the memory region to be dumped.
 * @param[in] length Length of the memory region in bytes.
 * @param[in] text   Text describing the dump.
 */
#define NRF_RPC_DUMP_ERR(memory, length, text) do { if (_nrf_rpc_log_level >= 1) _nrf_rpc_log_dump(1, memory, length, _NRF_RPC_LOG_SIDE "ERR: " text); } while (0)

/**
 * @brief Macro for logging a memory dump with the severity level WRN.
 *
 * @param[in] memory Pointer to the memory region to be dumped.
 * @param[in] length Length of the memory region in bytes.
 * @param[in] text   Text describing the dump.
 */
#define NRF_RPC_DUMP_WRN(memory, length, text) do { if (_nrf_rpc_log_level >= 2) _nrf_rpc_log_dump(2, memory, length, _NRF_RPC_LOG_SIDE "WRN: " text); } while (0)

/**
 * @brief Macro for logging a memory dump with the severity level INF.
 *
 * @param[in] memory Pointer to the memory region to be dumped.
 * @param[in] length Length of the memory region in bytes.
 * @param[in] text   Text describing the dump.
 */
#define NRF_RPC_DUMP_INF(memory, length, text) do { if (_nrf_rpc_log_level >= 3) _nrf_rpc_log_dump(3, memory, length, _NRF_RPC_LOG_SIDE "INF: " text); } while (0)

/**
 * @brief Macro for logging a memory dump with the severity level DBG.
 *
 * @param[in] memory Pointer to the memory region to be dumped.
 * @param[in] length Length of the memory region in bytes.
 * @param[in] text   Text describing the dump.
 */
#define NRF_RPC_DUMP_DBG(memory, length, text) do { if (_nrf_rpc_log_level >= 4) _nrf_rpc_log_dump(4, memory, length, _NRF_RPC_LOG_SIDE "DBG: " text); } while (0)

/**
 *@}
 */

#ifdef __cplusplus
}
#endif

#endif /* NRF_RPC_LOG_H_ */
