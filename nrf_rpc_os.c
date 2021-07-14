#define NRF_RPC_LOG_MODULE NRF_RPC_OS
#include <nrf_rpc_log.h>

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

#include "nrf_rpc_os.h"

#define LOG_LEVEL_UNINITIALIZED 1000

#define SHARED_MEMORY_SIZE \
	(CONFIG_NRF_RPC_SHMEM_IN_SIZE + CONFIG_NRF_RPC_SHMEM_OUT_SIZE + sizeof(struct ipc_events))

#define EMPTY_PTR ((void *)1)

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

int _nrf_rpc_log_level = LOG_LEVEL_UNINITIALIZED;

 __thread void *_nrf_rpc_tls = NULL;

void *nrf_rpc_os_out_shmem_ptr;
void *nrf_rpc_os_in_shmem_ptr;

static __thread char log_thread_name[5];

static const char *shmem_name = "/nrf_rpc_shmem";
static sem_t *ipc_in;
static sem_t *ipc_out;
static uint8_t *shared_memory;
static pthread_t events_reading_thread;
static pthread_t thread_pool[CONFIG_NRF_RPC_THREAD_POOL_SIZE];

static uint64_t ctx_pool_free = (((uint64_t)1 << CONFIG_NRF_RPC_CMD_CTX_POOL_SIZE) - 1);
static struct nrf_rpc_os_event ctx_pool_event;

static uint32_t remote_thread_count = 0;
static uint32_t remote_thread_reserved = 0;
static struct nrf_rpc_os_event remote_thread_event;

static struct nrf_rpc_os_msg thread_pool_msg;
static nrf_rpc_os_work_t thread_pool_callback;



uint32_t nrf_rpc_os_ctx_pool_reserve()
{
	uint32_t index = 0;
	
	pthread_mutex_lock(&ctx_pool_event.mutex);
	while (ctx_pool_free == 0) {
		pthread_cond_wait(&ctx_pool_event.cond, &ctx_pool_event.mutex);
	}
	while (!(ctx_pool_free & ((uint64_t)1 << index))) {
		index++;
	}
	ctx_pool_free &= ~((uint64_t)1 << index);
	pthread_mutex_unlock(&ctx_pool_event.mutex);

	return index;
}

void nrf_rpc_os_ctx_pool_release(uint32_t index)
{
	pthread_mutex_lock(&ctx_pool_event.mutex);
	ctx_pool_free |= (uint64_t)1 << index;
	pthread_cond_signal(&ctx_pool_event.cond);
	pthread_mutex_unlock(&ctx_pool_event.mutex);
}

void nrf_rpc_os_remote_count(int count)
{
	pthread_mutex_lock(&remote_thread_event.mutex);
	remote_thread_count = count;
	pthread_cond_signal(&remote_thread_event.cond);
	pthread_mutex_unlock(&remote_thread_event.mutex);
}

void nrf_rpc_os_remote_reserve()
{
	pthread_mutex_lock(&remote_thread_event.mutex);
	while (remote_thread_reserved >= remote_thread_count) {
		pthread_cond_wait(&remote_thread_event.cond, &remote_thread_event.mutex);
	}
	remote_thread_reserved++;
	pthread_mutex_unlock(&remote_thread_event.mutex);
}

void nrf_rpc_os_remote_release()
{
	pthread_mutex_lock(&remote_thread_event.mutex);
	remote_thread_reserved--;
	pthread_cond_signal(&remote_thread_event.cond);
	pthread_mutex_unlock(&remote_thread_event.mutex);
}

void nrf_rpc_os_thread_pool_send(const uint8_t *data, size_t len)
{
	pthread_mutex_lock(&thread_pool_msg.event.mutex);
	while (thread_pool_msg.data != EMPTY_PTR) {
		pthread_cond_wait(&thread_pool_msg.event.cond, &thread_pool_msg.event.mutex);
	}
	thread_pool_msg.data = data;
	thread_pool_msg.len = len;
	pthread_cond_broadcast(&thread_pool_msg.event.cond);
	pthread_mutex_unlock(&thread_pool_msg.event.mutex);
}

static void *thread_pool_main(void* param)
{
	const uint8_t *data;
	size_t len;

	sprintf(log_thread_name, "tp%02d", (int)(uintptr_t)param);

	do {
		pthread_mutex_lock(&thread_pool_msg.event.mutex);
		while (thread_pool_msg.data == EMPTY_PTR) {
			pthread_cond_wait(&thread_pool_msg.event.cond, &thread_pool_msg.event.mutex);
		}
		data = thread_pool_msg.data;
		len = thread_pool_msg.len;
		thread_pool_msg.data = EMPTY_PTR;
		pthread_cond_broadcast(&thread_pool_msg.event.cond);
		pthread_mutex_unlock(&thread_pool_msg.event.mutex);
		thread_pool_callback(data, len);
	} while (true);

	return NULL;
}

static void init_log(void)
{
	const char* env;
	char log_level;

	env = getenv("NRF_RPC_LOG_LEVEL");
	log_level = (env == NULL) ? 'N' : env[0];
	switch (log_level)
	{
	case 'D':
	case 'd':
		_nrf_rpc_log_level = 4;
		break;
	case 'I':
	case 'i':
		_nrf_rpc_log_level = 3;
		break;
	case 'W':
	case 'w':
		_nrf_rpc_log_level = 2;
		break;
	case 'E':
	case 'e':
		_nrf_rpc_log_level = 1;
		break;
	case 'n':
	case 'N':
		_nrf_rpc_log_level = 0;
		break;
	case '4':
	case '3':
	case '2':
	case '1':
	case '0':
		_nrf_rpc_log_level = log_level - '0';
		if (env[1] == 0) {
			break;
		}
		/* Fall to default (error) case */
	default:
		_nrf_rpc_log_level = 1;
		NRF_RPC_ERR("Invalid log level");
		exit(1);
	}
}

void _nrf_rpc_log(int level, const char* format, ...)
{
	va_list args;

	if (_nrf_rpc_log_level == LOG_LEVEL_UNINITIALIZED) {
		init_log();
		if (level < _nrf_rpc_log_level)
			return;
	}
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end (args);
}

void _nrf_rpc_log_dump(int level, const uint8_t *memory, size_t len, const char *text)
{
	if (_nrf_rpc_log_level == LOG_LEVEL_UNINITIALIZED) {
		init_log();
		if (level < _nrf_rpc_log_level)
			return;
	}
	// TODO: _nrf_rpc_log_dump
}

char *_nrf_rpc_log_thread()
{
	static int user_thread_counter = 0;
	if (!log_thread_name[0]) {
		sprintf(log_thread_name, "ut%02d", user_thread_counter);
		user_thread_counter++;
	}
	return log_thread_name;
}


static void (*signal_handler)(void) = NULL;

void nrf_rpc_os_signal_handler(void (*handler)(void))
{
	signal_handler = handler;
}

static void *events_reading_thread_main(void* param)
{
	strcpy(log_thread_name, "sign");
	do {
		if (sem_wait(ipc_in) < 0) {
			NRF_RPC_ERR("Waiting for signal error. Exiting...");
			exit(1);
			break;
		}
		if (signal_handler != NULL) {
			NRF_RPC_DBG("Signal received");
			signal_handler();
		} else {
			NRF_RPC_WRN("Signal received and dropped");
		}
	} while (true);
}

static void set_and_wait(uint8_t this_value, uint8_t next_value)
{
	static const int in = IS_MASTER ? 0 : 1;
	static const int out = IS_MASTER ? 1 : 0;

	shared_memory[out] = this_value;
	NRF_RPC_OS_MEMORY_BARIER();
	while (shared_memory[in] != this_value && shared_memory[in] != next_value) {
		usleep(1000);
		NRF_RPC_OS_MEMORY_BARIER();
	}
}


int nrf_rpc_os_init(nrf_rpc_os_work_t callback)
{
	int i;
	struct ipc_events *ipc_events;
	int fd;
	char* env;

	init_log();

	signal_handler = NULL;
	thread_pool_callback = callback;

	nrf_rpc_os_event_init(&ctx_pool_event);
	nrf_rpc_os_event_init(&remote_thread_event);
	nrf_rpc_os_msg_init(&thread_pool_msg);
	thread_pool_msg.data = EMPTY_PTR;

	env = getenv("NRF_RPC_SHMEM_NAME");
	if (env != NULL) {
		char* str = malloc(strlen(env) + 2);
		strcpy(str, "/");
		strcat(str, env);
		shmem_name = str;
	}
	NRF_RPC_INF("Shared memory name: %s", shmem_name);

	if (IS_MASTER) {
		fd = shm_open(shmem_name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
		NRF_RPC_DBG("fd=%d", fd);
		ftruncate(fd, SHARED_MEMORY_SIZE);
		env = getenv("NRF_RPC_RUN_SLAVE");
		if (env != NULL && fork() == 0) {
			NRF_RPC_INF("[child] Starting slave");
			system(env);
			NRF_RPC_INF("[child] Slave done");
			NRF_RPC_INF("[child] Exit");
			exit(0);
		}
		shared_memory = (uint8_t*)mmap(NULL, SHARED_MEMORY_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		close(fd);
		nrf_rpc_os_out_shmem_ptr = &shared_memory[0];
		nrf_rpc_os_in_shmem_ptr = &shared_memory[CONFIG_NRF_RPC_SHMEM_OUT_SIZE];
		ipc_events = (struct ipc_events*)&shared_memory[CONFIG_NRF_RPC_SHMEM_OUT_SIZE + CONFIG_NRF_RPC_SHMEM_IN_SIZE];
		ipc_in = &ipc_events->master_in;
		ipc_out = &ipc_events->master_out;
		NRF_RPC_DBG("synchronizing 1 of 3");
		set_and_wait(0xDE, 0xAD);
		set_and_wait(0xAD, 0xBE);
		set_and_wait(0xBE, 0xAF);
		NRF_RPC_DBG("sem_init");
		sem_init(ipc_in, 1, 0);
		sem_init(ipc_out, 1, 0);
		NRF_RPC_DBG("synchronizing 2 of 3");
		set_and_wait(0xAF, 0xCA);
		set_and_wait(0xCA, 0xFE);
		set_and_wait(0xFE, 0xFE);
	} else {
		fd = shm_open(shmem_name, O_RDWR, S_IRUSR | S_IWUSR);
		NRF_RPC_DBG("fd=%d", fd);
		shared_memory = (uint8_t*)mmap(NULL, SHARED_MEMORY_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		close(fd);
		nrf_rpc_os_in_shmem_ptr = &shared_memory[0];
		nrf_rpc_os_out_shmem_ptr = &shared_memory[CONFIG_NRF_RPC_SHMEM_IN_SIZE];
		ipc_events = (struct ipc_events*)&shared_memory[CONFIG_NRF_RPC_SHMEM_OUT_SIZE + CONFIG_NRF_RPC_SHMEM_IN_SIZE];
		ipc_in = &ipc_events->slave_in;
		ipc_out = &ipc_events->slave_out;
		NRF_RPC_DBG("synchronizing 1 of 3");
		set_and_wait(0xDE, 0xAD);
		set_and_wait(0xAD, 0xBE);
		set_and_wait(0xBE, 0xAF);
		NRF_RPC_DBG("synchronizing 2 of 3");
		set_and_wait(0xAF, 0xCA);
		set_and_wait(0xCA, 0xFE);
		set_and_wait(0xFE, 0xFE);
	}

	NRF_RPC_DBG("synchronizing 3 of 3");
	sem_post(ipc_out);
	sem_wait(ipc_in);

	NRF_RPC_INF("Shared memory and signals synchronized");

	pthread_create(&events_reading_thread, NULL, events_reading_thread_main, NULL);

	for (i = 0; i < CONFIG_NRF_RPC_THREAD_POOL_SIZE; i++)
	{
		pthread_create(&thread_pool[i], NULL, thread_pool_main, (void*)(uintptr_t)i);
	}

	return 0;
}

void nrf_rpc_os_signal(void)
{
	sem_post(ipc_out);
}
