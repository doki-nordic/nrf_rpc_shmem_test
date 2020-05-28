
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

#include "conf.h"
#include "shmem_linux.h"

#define SHARED_MEMORY_SIZE (NRF_RPC_SHMEM_OUT_SIZE + NRF_RPC_SHMEM_IN_SIZE + sizeof(struct ipc_events))

struct ipc_events {
	union {
		sem_t master_in;
		sem_t slave_out;
	};
	union {
		sem_t master_out;
		sem_t slave_in;
	};
};

void* nrf_rpc_os_out_shmem_ptr;
void* nrf_rpc_os_in_shmem_ptr;

static sem_t *ipc_in;
static sem_t *ipc_out;
static uint8_t *shared_memory;
static pthread_t events_reading_thread;


static int translate_error(int err)
{
	return NRF_RPC_ERR_INTERNAL;
}


void *events_reading_thread_main(void* param)
{
	do {
		if (sem_wait(ipc_in) < 0) {
			LOG("Waiting for signal error. Exiting...");
			exit(1);
			break;
		}
		nrf_rpc_os_signaled();
	} while (true);
}


int nrf_rpc_os_signal()
{
	if (sem_post(ipc_out) < 0) {
		return translate_error(errno);
	}
	return NRF_RPC_SUCCESS;
}


static void set_and_wait(uint8_t this_value)
{
	uint8_t value;
	shared_memory[0] = this_value;
	do {
		NRF_RPC_OS_MEMORY_BARIER();
		value = shared_memory[1];
		if (value == this_value) break;
		usleep(1000);
	} while (true);
}


static void listen_and_wait(uint8_t this_value)
{
	uint8_t value;
	do {
		NRF_RPC_OS_MEMORY_BARIER();
		value = shared_memory[0];
		shared_memory[1] = value;
		if (value == this_value) break;
		usleep(1000);
	} while (true);
}

static const char *shmem_name = "/shmem_test_shmdesc";

int nrf_rpc_os_init()
{
	struct ipc_events *ipc_events;
	int fd;
	char* env;

	env = getenv("NRF_RPC_SHMEM_NAME");
	if (env != NULL) {
		char* str = malloc(strlen(env) + 2);
		strcpy(str, "/");
		strcat(str, env);
		shmem_name = str;
	}
	LOG("Shared memory name: %s", shmem_name);

	if (IS_MASTER) {
		fd = shm_open(shmem_name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
		LOG("fd=%d", fd);
		ftruncate(fd, SHARED_MEMORY_SIZE);
		env = getenv("NRF_RPC_RUN_SLAVE");
		if (env != NULL && fork() == 0) {
			LOG("[child] Starting slave");
			system(env);
			LOG("[child] Slave done");
			LOG("[child] Exit");
			exit(0);
		}
		shared_memory = (uint8_t*)mmap(NULL, SHARED_MEMORY_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		close(fd);
		nrf_rpc_os_out_shmem_ptr = &shared_memory[0];
		nrf_rpc_os_in_shmem_ptr = &shared_memory[NRF_RPC_SHMEM_OUT_SIZE];
		ipc_events = (struct ipc_events*)&shared_memory[NRF_RPC_SHMEM_IN_SIZE + NRF_RPC_SHMEM_OUT_SIZE];
		ipc_in = &ipc_events->master_in;
		ipc_out = &ipc_events->master_out;
		LOG("synchronizing 1 of 3");
		set_and_wait(0xDE);
		set_and_wait(0xAD);
		set_and_wait(0xBE);
		LOG("sem_init");
		sem_init(ipc_in, 1, 0);
		sem_init(ipc_out, 1, 0);
		LOG("synchronizing 2 of 3");
		set_and_wait(0xAF);
		set_and_wait(0xCA);
		set_and_wait(0xFE);
	} else {
		fd = shm_open(shmem_name, O_RDWR, S_IRUSR | S_IWUSR);
		LOG("fd=%d", fd);
		shared_memory = (uint8_t*)mmap(NULL, SHARED_MEMORY_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		close(fd);
		nrf_rpc_os_in_shmem_ptr = &shared_memory[0];
		nrf_rpc_os_out_shmem_ptr = &shared_memory[NRF_RPC_SHMEM_IN_SIZE];
		ipc_events = (struct ipc_events*)&shared_memory[NRF_RPC_SHMEM_IN_SIZE + NRF_RPC_SHMEM_OUT_SIZE];
		ipc_in = &ipc_events->slave_in;
		ipc_out = &ipc_events->slave_out;
		LOG("synchronizing 1 of 3");
		listen_and_wait(0xBE);
		LOG("synchronizing 2 of 3");
		listen_and_wait(0xCA);
		listen_and_wait(0xFE);
	}

	LOG("synchronizing 3 of 3");
	sem_post(ipc_out);
	sem_wait(ipc_in);

	LOG("Shared memory and signals synchronized");

	pthread_create(&events_reading_thread, NULL, events_reading_thread_main, NULL);

	return NRF_RPC_SUCCESS;
}
