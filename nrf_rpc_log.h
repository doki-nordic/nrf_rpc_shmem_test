/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NRF_RPC_LOG_H_
#define NRF_RPC_LOG_H_


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

#include "nrf_rpc_os.h"

#if defined(MASTER)
#define _NRF_RPC_LOG_SIDE "MASTER"
#elif defined(SLAVE)
#define _NRF_RPC_LOG_SIDE "SLAVE "
#else
#define _NRF_RPC_LOG_SIDE "      "
#endif

/**
 * @brief Macro for logging a message with the severity level ERR.
 *
 * @param ... printf-style format string, optionally followed by arguments
 *            to be formatted and inserted in the resulting string.
 */
#define NRF_RPC_ERR(text, ...) do { if (_nrf_rpc_log_level >= 1) _nrf_rpc_log(1, stderr, _NRF_RPC_LOG_SIDE " ERR: " text, ##__VA_ARGS__); } while (0)
// TODO: _nrf_rpc_log() will first check if log was initialized if not initializes it and print message only if correct log level is enabled

/**
 * @brief Macro for logging a message with the severity level WRN.
 *
 * @param ... printf-style format string, optionally followed by arguments
 *            to be formatted and inserted in the resulting string.
 */
#define NRF_RPC_WRN(text, ...) do { if (_nrf_rpc_log_level >= 2) _nrf_rpc_log(2, stderr, _NRF_RPC_LOG_SIDE " WRN: " text, ##__VA_ARGS__); } while (0)

/**
 * @brief Macro for logging a message with the severity level INF.
 *
 * @param ... printf-style format string, optionally followed by arguments
 *            to be formatted and inserted in the resulting string.
 */
#define NRF_RPC_INF(text, ...) do { if (_nrf_rpc_log_level >= 3) fprintf(stderr, _NRF_RPC_LOG_SIDE " INF: " text, ##__VA_ARGS__); } while (0)

/**
 * @brief Macro for logging a message with the severity level DBG.
 *
 * @param ... printf-style format string, optionally followed by arguments
 *            to be formatted and inserted in the resulting string.
 */
#define NRF_RPC_DBG(text, ...) do { if (_nrf_rpc_log_level >= 4) fprintf(stderr, _NRF_RPC_LOG_SIDE " DBG: " text, ##__VA_ARGS__); } while (0)

/**
 * @brief Macro for logging a memory dump with the severity level ERR.
 *
 * @param[in] memory Pointer to the memory region to be dumped.
 * @param[in] length Length of the memory region in bytes.
 * @param[in] text   Text describing the dump.
 */
#define NRF_RPC_DUMP_ERR(memory, length, text) do { if (_nrf_rpc_log_level >= 1) _nrf_rpc_log_dump(memory, length, _NRF_RPC_LOG_SIDE "ERR: " text); } while (0)

/**
 * @brief Macro for logging a memory dump with the severity level WRN.
 *
 * @param[in] memory Pointer to the memory region to be dumped.
 * @param[in] length Length of the memory region in bytes.
 * @param[in] text   Text describing the dump.
 */
#define NRF_RPC_DUMP_WRN(memory, length, text) do { if (_nrf_rpc_log_level >= 2) _nrf_rpc_log_dump(memory, length, _NRF_RPC_LOG_SIDE "WRN: " text); } while (0)

/**
 * @brief Macro for logging a memory dump with the severity level INF.
 *
 * @param[in] memory Pointer to the memory region to be dumped.
 * @param[in] length Length of the memory region in bytes.
 * @param[in] text   Text describing the dump.
 */
#define NRF_RPC_DUMP_INF(memory, length, text) do { if (_nrf_rpc_log_level >= 3) _nrf_rpc_log_dump(memory, length, _NRF_RPC_LOG_SIDE "INF: " text); } while (0)

/**
 * @brief Macro for logging a memory dump with the severity level DBG.
 *
 * @param[in] memory Pointer to the memory region to be dumped.
 * @param[in] length Length of the memory region in bytes.
 * @param[in] text   Text describing the dump.
 */
#define NRF_RPC_DUMP_DBG(memory, length, text) do { if (_nrf_rpc_log_level >= 4) _nrf_rpc_log_dump(memory, length, _NRF_RPC_LOG_SIDE "DBG: " text); } while (0)

/**
 *@}
 */

#ifdef __cplusplus
}
#endif

#endif /* NRF_RPC_LOG_H_ */
