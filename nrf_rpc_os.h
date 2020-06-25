/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef NRF_RPC_OS_H_
#define NRF_RPC_OS_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include <nrf_rpc_log.h>

/*
 * THIS IS A TEMPLATE FILE.
 * It should be copied to a suitable location within the host environment into
 * which Remote Procedure serialization is integrated, and the following macros
 * should be provided with appropriate implementations.
 */


/**
 * @defgroup nrf_rpc_os OS-dependent functionality for nRF PRC
 * @{
 * @ingroup nrf_rpc
 *
 * @brief OS-dependent functionality for nRF PRC
 */


#ifdef __cplusplus
extern "C" {
#endif

/** @brief Structure to pass events between threads. */
struct nrf_rpc_os_event
{
	bool set;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
};

/** @brief Structure to pass messages between threads. */
struct nrf_rpc_os_msg
{
	struct nrf_rpc_os_event event;
	const uint8_t *data;
	size_t len;
};


/** @brief Work callback that will be called from thread pool.
 *
 * @param data Data passed from @ref nrf_rpc_os_thread_pool_send.
 * @param len  Data length.
 */
typedef void (*nrf_rpc_os_work_t)(const uint8_t *data, size_t len);

/** @brief nRF RPC OS-dependent initialization.
 *
 * @param callback Work callback that will be called when something was send
 *                 to a thread pool.
 *
 * @return         0 on success or negative error code.
 */
int nrf_rpc_os_init(nrf_rpc_os_work_t callback);

/** @brief Send work to a thread pool.
 *
 * Function reserves a thread from a thread pool and executes callback provided
 * in @ref nrf_rpc_os_init with `data` and `len` paramteres. If there is no
 * thread available in the thread pool then this function waits.
 *
 * @param data Data pointer to pass. Data is passed as a pointer, no copying is
 *             done.
 * @param len  Length of the `data`.
 */
void nrf_rpc_os_thread_pool_send(const uint8_t *data, size_t len);

/** @brief Initialize event passing structure.
 *
 * @param event Event structure to initialize.
 *
 * @return      0 on success or negative error code.
 */
static inline int nrf_rpc_os_event_init(struct nrf_rpc_os_event *event)
{
	event->set = false;
	if (pthread_mutex_init(&event->mutex, NULL) != 0)
	{
		NRF_RPC_ERR("pthread_mutex_init failed!");
		return -EFAULT;
	}
	if (pthread_cond_init(&event->cond, NULL) != 0)
	{
		NRF_RPC_ERR("pthread_cond_init failed!");
		return -EFAULT;
	}
	return 0;
}

/** @brief Set an event.
 *
 * If some thread is waiting of the event then it will be waken up. If there is
 * no thread waiting next call to @ref nrf_rpc_os_event_wait will return
 * immediately.
 *
 * Event behavior is the same as a binary semaphore.
 *
 * @param event Event to set.
 */
static inline void nrf_rpc_os_event_set(struct nrf_rpc_os_event *event)
{
	pthread_mutex_lock(&event->mutex);
	event->set = true;
	pthread_cond_signal(&event->cond);
	pthread_mutex_unlock(&event->mutex);
}

/** @brief Wait for an event.
 *
 * @param event Event to wait for.
 */
static inline void nrf_rpc_os_event_wait(struct nrf_rpc_os_event *event)
{
	pthread_mutex_lock(&event->mutex);
	while (!event->set) {
		pthread_cond_wait(&event->cond, &event->mutex);
	}
	event->set = false;
	pthread_mutex_unlock(&event->mutex);
}

/** @brief Initialize message passing structure.
 *
 * @param msg Structure to initialize.
 *
 * @return    0 on success or negative error code.
 */
static inline int nrf_rpc_os_msg_init(struct nrf_rpc_os_msg *msg)
{
	return nrf_rpc_os_event_init(&msg->event);
}

/** @brief Pass a message to a different therad.
 *
 * nRF RPC is passing one message at a time, so there is no need to do
 * FIFO here.
 *
 * @param msg  Message passing structure.
 * @param data Data pointer to pass. Data is passed as a pointer, so no copying
 *             is done.
 * @param len  Length of the `data`.
 */
static inline void nrf_rpc_os_msg_set(struct nrf_rpc_os_msg *msg, const uint8_t *data,
			size_t len)
{
	pthread_mutex_lock(&msg->event.mutex);
	msg->event.set = true;
	msg->data = data;
	msg->len = len;
	pthread_cond_signal(&msg->event.cond);
	pthread_mutex_unlock(&msg->event.mutex);
}

/** @brief Get a message.
 *
 * If message was not set yet then this function waits.
 *
 * @param[in]  msg  Message passing structure.
 * @param[out] data Received data pointer. Data is passed as a pointer, so no
 *                  copying is done.
 * @param[out] len  Length of the `data`.
 */
static inline void nrf_rpc_os_msg_get(struct nrf_rpc_os_msg *msg, const uint8_t **data,
			size_t *len)
{
	pthread_mutex_lock(&msg->event.mutex);
	while (!msg->event.set) {
		pthread_cond_wait(&msg->event.cond, &msg->event.mutex);
	}
	*data = msg->data;
	*len = msg->len;
	pthread_mutex_unlock(&msg->event.mutex);
}

/** @brief Get TLS (Thread Local Storage) for nRF RPC.
 *
 * nRF PRC need one pointer to associate with a thread.
 *
 * @return Pointer stored on TLS or NULL if pointer was not set for this thread.
 */
static inline void* nrf_rpc_os_tls_get(void)
{
	extern __thread void *_nrf_rpc_tls;
	return _nrf_rpc_tls;
}

/** @brief Set TLS (Thread Local Storage) for nRF RPC.
 *
 * @param data Pointer to store on TLS.
 */
static inline void nrf_rpc_os_tls_set(void *data)
{
	extern __thread void *_nrf_rpc_tls;
	_nrf_rpc_tls = data;
}

/** @brief Reserve one context from command context pool.
 *
 * If there is no available context then this function waits for it.
 *
 * @return Context index between 0 and CONFIG_NRF_RPC_CMD_CTX_POLL_SIZE - 1.
 */
uint32_t nrf_rpc_os_ctx_pool_reserve();

/** @brief Release context from context pool.
 *
 * @param index Context index that was previously reserved.
 */
void nrf_rpc_os_ctx_pool_release(uint32_t index);

/** @brief Set number of remote threads.
 *
 * Number of remote threads that can be reserved by
 * @ref nrf_rpc_os_remote_reserve is limited by `count` parameter.
 * After initialization `count` is assumed to be zero.
 *
 * @param count Number of remote threads.
 */
void nrf_rpc_os_remote_count(int count);

/** @brief Reserve one thread from a remote thread pool.
 *
 * If there are no more threads available or @ref nrf_rpc_os_remote_count was
 * not called yet then this function waits.
 *
 * Remote thread reserving and releasing can be implemented using a semaphore.
 */
void nrf_rpc_os_remote_reserve();

/** @brief Release one thread from a remote thread pool.
 */
void nrf_rpc_os_remote_release();


/*==========================================================================*/

#define NRF_RPC_OS_SHMEM_PTR_CONST 0
#define NRF_RPC_OS_MEMORY_BARIER __sync_synchronize

extern void *nrf_rpc_os_out_shmem_ptr;
extern void *nrf_rpc_os_in_shmem_ptr;

//---------------------------------

void nrf_rpc_os_signal(void);

//---------------------------------

typedef uint32_t nrf_rpc_os_atomic_t;

static inline uint32_t nrf_rpc_os_atomic_or(nrf_rpc_os_atomic_t *atomic, uint32_t value)
{
	return __atomic_fetch_or(atomic, value, __ATOMIC_SEQ_CST);
}

static inline uint32_t nrf_rpc_os_atomic_and(nrf_rpc_os_atomic_t *atomic, uint32_t value)
{
	return __atomic_fetch_and(atomic, value, __ATOMIC_SEQ_CST);
}

static inline uint32_t nrf_rpc_os_atomic_get(nrf_rpc_os_atomic_t *atomic)
{
	return __atomic_load_n(atomic, __ATOMIC_SEQ_CST);
}

//----------------------------------

typedef pthread_mutex_t nrf_rpc_os_mutex_t;

typedef struct nrf_rpc_os_sem {
	uint32_t counter;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
} nrf_rpc_os_sem_t;

static inline void nrf_rpc_os_sem_init(nrf_rpc_os_sem_t *sem)
{
	pthread_mutex_init(&sem->mutex, NULL);
	pthread_cond_init(&sem->cond, NULL);
	sem->counter = 0;
}

static inline void nrf_rpc_os_mutex_init(nrf_rpc_os_mutex_t *mutex)
{
	pthread_mutex_init(mutex, NULL);
}

static inline void nrf_rpc_os_lock(pthread_mutex_t *mutex) {
	pthread_mutex_lock(mutex);
}

static inline void nrf_rpc_os_unlock(pthread_mutex_t *mutex) {
	pthread_mutex_unlock(mutex);
}

static inline void nrf_rpc_os_take(nrf_rpc_os_sem_t *sem) {
	pthread_mutex_lock(&sem->mutex);
	while (sem->counter == 0) {
		pthread_cond_wait(&sem->cond, &sem->mutex);
	}
	sem->counter--;
	pthread_mutex_unlock(&sem->mutex);
}

static inline void nrf_rpc_os_yield() {
	usleep(5000);
}

static inline void nrf_rpc_os_give(nrf_rpc_os_sem_t *sem) {
	pthread_mutex_lock(&sem->mutex);
	if (sem->counter < 1) {
		sem->counter++;
		pthread_cond_signal(&sem->cond);
	}
	pthread_mutex_unlock(&sem->mutex);
}

static inline int nrf_rpc_os_clz64(uint64_t value)
{
	return __builtin_clzll(value);
}

static inline int nrf_rpc_os_clz32(uint32_t value)
{
	if (sizeof(int) != 4) {
		return __builtin_clzl(value);
	}
	return __builtin_clz(value);
}

//------------------------------------

static inline void nrf_rpc_os_fatal(void)
{
	exit(32);
}

//------------------------------------

void nrf_rpc_os_signal_handler(void (*handler)(void));

#ifdef __cplusplus
}
#endif

/**
 *@}
 */

#endif /* NRF_RPC_OS_H_ */
