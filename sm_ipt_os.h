/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef SM_IPT_OS_H_
#define SM_IPT_OS_H_


#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include <sys/types.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <stdarg.h>
#include <assert.h>

/**
 * @defgroup sm_ipt_os_zephyr SM IPT OS abstraction for Zephyr.
 * @{
 * @brief SM IPT OS abstraction for Zephyr.
 *
 * API is compatible with sm_ipt_os API. For API documentation
 * @see sm_ipt_os_tmpl.h
 */

#ifdef __cplusplus
extern "C" {
#endif

struct sm_ipt_os_shmem_conf {
	void* output;
	void* input;
	uint32_t output_size;
	uint32_t input_size;
};

struct sm_ipt_linux_param {
	uint32_t output_size;
	uint32_t input_size;
	const char* shmem_name;
	bool is_host;
};

struct sm_ipt_os_ctx{
	void (*signal_handler)(struct sm_ipt_os_ctx *);
	sem_t *ipc_in;
	sem_t *ipc_out;
};

#define SM_IPT_ASSERT(_expr) assert((_expr))

#define SM_IPT_OS_MEMORY_BARRIER() __sync_synchronize()

#define SM_IPT_OS_CACHE_WB(ptr) __sync_synchronize()
#define SM_IPT_OS_CACHE_INV(ptr) __sync_synchronize()
#define SM_IPT_OS_CACHE_WB_SIZE(ptr, size) __sync_synchronize()
#define SM_IPT_OS_CACHE_INV_SIZE(ptr, size) __sync_synchronize()

#define SM_IPT_OS_GET_CONTAINTER(ptr, type, field) ((type *)(((char *)(ptr)) - offsetof(type, field)))

void sm_ipt_os_signal(struct sm_ipt_os_ctx *os_ctx);
int sm_ipt_os_init_signals(struct sm_ipt_os_ctx *os_ctx,
						   void *os_specific_param,
						   void (*handler)(struct sm_ipt_os_ctx *));
int sm_ipt_os_init_shmem(struct sm_ipt_os_ctx *os_ctx,
						 void *os_specific_param,
						 struct sm_ipt_os_shmem_conf *conf);

typedef uint32_t sm_ipt_os_atomic_t;

static inline uint32_t sm_ipt_os_atomic_or(sm_ipt_os_atomic_t *atomic, uint32_t value)
{
	return __atomic_fetch_or(atomic, value, __ATOMIC_SEQ_CST);
}

static inline uint32_t sm_ipt_os_atomic_and(sm_ipt_os_atomic_t *atomic, uint32_t value)
{
	return __atomic_fetch_and(atomic, value, __ATOMIC_SEQ_CST);
}

static inline uint32_t sm_ipt_os_atomic_get(sm_ipt_os_atomic_t *atomic)
{
	return __atomic_load_n(atomic, __ATOMIC_SEQ_CST);
}


typedef pthread_mutex_t sm_ipt_os_mutex_t;

static inline void sm_ipt_os_mutex_init(sm_ipt_os_mutex_t *mutex)
{
	pthread_mutex_init(mutex, NULL);
}

static inline void sm_ipt_os_lock(pthread_mutex_t *mutex) {
	pthread_mutex_lock(mutex);
}

static inline void sm_ipt_os_unlock(pthread_mutex_t *mutex) {
	pthread_mutex_unlock(mutex);
}

typedef struct sm_ipt_os_sem {
	uint32_t counter;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
} sm_ipt_os_sem_t;

static inline void sm_ipt_os_sem_init(sm_ipt_os_sem_t *sem)
{
	pthread_mutex_init(&sem->mutex, NULL);
	pthread_cond_init(&sem->cond, NULL);
	sem->counter = 0;
}

static inline void sm_ipt_os_take(sm_ipt_os_sem_t *sem) {
	pthread_mutex_lock(&sem->mutex);
	while (sem->counter == 0) {
		pthread_cond_wait(&sem->cond, &sem->mutex);
	}
	sem->counter--;
	pthread_mutex_unlock(&sem->mutex);
}

static inline void sm_ipt_os_give(sm_ipt_os_sem_t *sem) {
	pthread_mutex_lock(&sem->mutex);
	if (sem->counter < 1) {
		sem->counter++;
		pthread_cond_signal(&sem->cond);
	}
	pthread_mutex_unlock(&sem->mutex);
}

static inline void sm_ipt_os_yield() {
	usleep(5000);
}

static inline void sm_ipt_os_fatal(void)
{
	exit(32);
}

static inline int sm_ipt_os_clz64(uint64_t value)
{
	return __builtin_clzll(value);
}

static inline int sm_ipt_os_clz32(uint32_t value)
{
	if (sizeof(int) != 4) {
		return __builtin_clzl(value);
	}
	return __builtin_clz(value);
}


#ifdef __cplusplus
}
#endif

/**
 *@}
 */

#endif /* SM_IPT_OS_H */
