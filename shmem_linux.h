#ifndef SHMEM_LINUX_H_
#define SHMEM_LINUX_H_

#include <pthread.h>
#include <semaphore.h>

#define NRF_RPC_OS_SHMEM_PTR_CONST 0
#define NRF_RPC_OS_MEMORY_BARIER __sync_synchronize

typedef pthread_mutex_t nrf_rpc_os_mutex_t;
typedef sem_t nrf_rpc_os_sem_t;

extern void* nrf_rpc_os_out_shmem_ptr;
extern void* nrf_rpc_os_in_shmem_ptr;

int nrf_rpc_os_init();

void nrf_rpc_os_signaled();
int nrf_rpc_os_signal();

static int nrf_rpc_os_mutex_init(nrf_rpc_os_mutex_t *mutex){}
static inline void nrf_rpc_os_mutex_lock(nrf_rpc_os_mutex_t *mutex)
{
	pthread_mutex_lock(mutex);
}
static inline void nrf_rpc_os_mutex_unlock(nrf_rpc_os_mutex_t *mutex)
{
	pthread_mutex_unlock(mutex);
}

static int nrf_rpc_os_sem_init(nrf_rpc_os_sem_t *sem, int init_count){}
static void nrf_rpc_os_sem_take(nrf_rpc_os_sem_t *sem){}
static void nrf_rpc_os_sem_give(nrf_rpc_os_sem_t *sem){}


#endif // SHMEM_LINUX_H_
