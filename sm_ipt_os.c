

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

#include "sm_ipt.h"
#include "sm_ipt_log.h"
#include "sm_ipt_os.h"

#define WORD_SIZE 8 // also support 64-bit architecture

#ifdef CONFIG_SM_IPT_CACHE_LINE_BYTES
#if CONFIG_SM_IPT_CACHE_LINE_BYTES > WORD_SIZE
#define ALIGN_SIZE CONFIG_SM_IPT_CACHE_LINE_BYTES
#else
#define ALIGN_SIZE WORD_SIZE
#endif
#else
#define ALIGN_SIZE WORD_SIZE
#endif


struct ipc_events {
	sem_t sem_out;
	sem_t sem_in;
};


#define LOG_LEVEL_UNINITIALIZED 1000


static __thread char log_thread_name[5];

static pthread_t events_reading_thread;

int _nrf_rpc_log_level = LOG_LEVEL_UNINITIALIZED;


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
		SM_IPT_ERR("Invalid log level");
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

char *_nrf_rpc_log_thread()
{
	static int user_thread_counter = 0;
	if (!log_thread_name[0]) {
		sprintf(log_thread_name, "ut%02d", user_thread_counter);
		user_thread_counter++;
	}
	return log_thread_name;
}

void nrf_rpc_log_set_thread_name(const char* str) {
	strcpy(log_thread_name, str);
}

static void *events_reading_thread_main(void* param)
{
	struct sm_ipt_os_ctx *os_ctx = (struct sm_ipt_os_ctx *)param;
	nrf_rpc_log_set_thread_name("sign");
	do {
		SM_IPT_DBG("Waiting for signal...");
		if (sem_wait(os_ctx->ipc_in) < 0) {
			SM_IPT_ERR("Waiting for signal error. Exiting...");
			exit(1);
			break;
		}
		if (os_ctx->signal_handler != NULL) {
			SM_IPT_DBG("Signal received");
			os_ctx->signal_handler(os_ctx);
		} else {
			SM_IPT_WRN("Signal received and dropped");
		}
	} while (true);
}

static void set_and_wait(volatile uint8_t *in, volatile uint8_t *out, uint8_t this_value, uint8_t next_value)
{
	*out = this_value;
	__sync_synchronize();
	while (*in != this_value && *in != next_value) {
		usleep(1000);
		__sync_synchronize();
	}
}

void *ptr_align_up(void* ptr) {
	uintptr_t p = (uintptr_t)ptr;
	return (void*)((p + ALIGN_SIZE - 1) & ~(ALIGN_SIZE - 1));
}

void *ptr_align_down(void* ptr) {
	uintptr_t p = (uintptr_t)ptr;
	return (void*)(p & ~(ALIGN_SIZE - 1));
}

int sm_ipt_os_init_shmem(struct sm_ipt_os_ctx *os_ctx,
				   void *os_specific_param,
				   struct sm_ipt_os_shmem_conf *conf)
{
	int fd;
	struct ipc_events* ipc_events;
	struct sm_ipt_linux_param* param = (struct sm_ipt_linux_param*)os_specific_param;
	memset(os_ctx, 0, sizeof(struct sm_ipt_os_ctx));

	SM_IPT_INF("Shared memory name: %s", param->shmem_name);
	SM_IPT_INF("Host input size: %d", param->host_input_size);
	SM_IPT_INF("Host output size: %d", param->host_output_size);

	os_ctx->signal_handler = NULL;
	os_ctx->is_host = param->is_host;
	uint32_t total_size = param->host_input_size + param->host_input_size + sizeof(struct ipc_events) + 3 * ALIGN_SIZE;

	if (param->is_host) {
		conf->input_size = param->host_input_size;
		conf->output_size = param->host_output_size;
		fd = shm_open(param->shmem_name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	} else {
		conf->input_size = param->host_output_size;
		conf->output_size = param->host_input_size;
		fd = shm_open(param->shmem_name, O_RDWR, S_IRUSR | S_IWUSR);
	}
	SM_IPT_DBG("fd=%d", fd);
	ftruncate(fd, total_size);
	os_ctx->shared_memory = (uint8_t*)mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);
	if (CONFIG_SM_IPT_CACHE_LINE_BYTES > 0) {
		os_ctx->cache_memory = malloc(total_size);
	} else {
		os_ctx->cache_memory = os_ctx->shared_memory;
	}
	ipc_events = (struct ipc_events*)&os_ctx->shared_memory[0];
	uint8_t *hs_in = ptr_align_up((uint8_t *)os_ctx->shared_memory + sizeof(struct ipc_events));
	uint8_t *hs_out = hs_in + 8;
	conf->input = ptr_align_up((uint8_t *)os_ctx->cache_memory + sizeof(struct ipc_events));
	conf->output = ptr_align_up((uint8_t *)conf->input + param->host_input_size);
	os_ctx->ipc_in = param->is_host ? &ipc_events->sem_in : &ipc_events->sem_out;
	os_ctx->ipc_out = param->is_host ? &ipc_events->sem_out : &ipc_events->sem_in;
	if (!param->is_host) {
		void *tmp = conf->input;
		conf->input = conf->output;
		conf->output = tmp;
		tmp = hs_in;
		hs_in = hs_out;
		hs_out = tmp;
	}
	SM_IPT_DBG("shmem=0x%08X %d", (int)(intptr_t)os_ctx->shared_memory, total_size);
	SM_IPT_DBG("cache=0x%08X %d", (int)(intptr_t)os_ctx->cache_memory, total_size);
	SM_IPT_DBG("input=0x%08X %d", (int)(intptr_t)conf->input, conf->input_size);
	SM_IPT_DBG("output=0x%08X %d", (int)(intptr_t)conf->output, conf->output_size);
	SM_IPT_DBG("ipc=0x%08X 0x%08X", (int)(intptr_t)os_ctx->ipc_in, (int)(intptr_t)os_ctx->ipc_out);
	SM_IPT_DBG("synchronizing 1 of 3");
	set_and_wait(hs_in, hs_out, 0xDE, 0xAD);
	set_and_wait(hs_in, hs_out, 0xAD, 0xBE);
	set_and_wait(hs_in, hs_out, 0xBE, 0xAF);
	if (param->is_host) {
		SM_IPT_DBG("sem_init");
		sem_init(os_ctx->ipc_in, 1, 0);
		sem_init(os_ctx->ipc_out, 1, 0);
	}
	SM_IPT_DBG("synchronizing 2 of 3");
	set_and_wait(hs_in, hs_out, 0xAF, 0xCA);
	set_and_wait(hs_in, hs_out, 0xCA, 0xFE);
	set_and_wait(hs_in, hs_out, 0xFE, 0xFE);

	SM_IPT_DBG("synchronizing 3 of 3");
	sem_post(os_ctx->ipc_out);
	sem_wait(os_ctx->ipc_in);

	SM_IPT_INF("Shared memory and signals synchronized");

	return 0;
}

int sm_ipt_os_init_signals(struct sm_ipt_os_ctx *os_ctx,
						   void *os_specific_param,
				void (*handler)(struct sm_ipt_os_ctx *))
{
	SM_IPT_INF("Start receiving signals");
	os_ctx->signal_handler = handler;
	pthread_create(&events_reading_thread, NULL, events_reading_thread_main, os_ctx);
	return 0;
}

void sm_ipt_os_signal(struct sm_ipt_os_ctx *os_ctx)
{
	sem_post(os_ctx->ipc_out);
}


void sm_ipt_os_cache_wb(struct sm_ipt_os_ctx *os_ctx, void *ptr, size_t size)
{
	uint8_t *cache = os_ctx->cache_memory;
	uint8_t *main = os_ctx->shared_memory;
	uint8_t *begin_line = ptr_align_down(ptr);
	uint8_t *end_line = (uint8_t *)ptr_align_down((uint8_t *)ptr + size - 1) + ALIGN_SIZE;
	__sync_synchronize();
	memcpy(main + (begin_line - cache), begin_line, end_line - begin_line);
}


void sm_ipt_os_cache_inv(struct sm_ipt_os_ctx *os_ctx, void *ptr, size_t size)
{
	uint8_t *cache = os_ctx->cache_memory;
	uint8_t *main = os_ctx->shared_memory;
	uint8_t *begin_line = ptr_align_down(ptr);
	uint8_t *end_line = (uint8_t *)ptr_align_down((uint8_t *)ptr + size - 1) + ALIGN_SIZE;
	memcpy(begin_line, main + (begin_line - cache), end_line - begin_line);
	__sync_synchronize();
}
