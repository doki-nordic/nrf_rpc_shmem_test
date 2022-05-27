

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
	SM_IPT_OS_MEMORY_BARRIER();
	while (*in != this_value && *in != next_value) {
		usleep(1000);
		SM_IPT_OS_MEMORY_BARRIER();
	}
}

int sm_ipt_os_init_shmem(struct sm_ipt_os_ctx *os_ctx,
				   void *os_specific_param,
				   struct sm_ipt_os_shmem_conf *conf)
{
	int fd;
	struct ipc_events* ipc_events;
	struct sm_ipt_linux_param* param = (struct sm_ipt_linux_param*)os_specific_param;
	memset(os_ctx, 0, sizeof(struct sm_ipt_os_ctx));
	static uint8_t *shared_memory;

	SM_IPT_INF("Shared memory name: %s", param->shmem_name);
	SM_IPT_INF("Input size: %d", param->input_size);
	SM_IPT_INF("Output size: %d", param->output_size);

	os_ctx->signal_handler = NULL;
	conf->input_size = param->input_size;
	conf->output_size = param->output_size;
	uint32_t input_size = (param->input_size + 7) & ~7;
	uint32_t output_size = (param->output_size + 7) & ~7;
	uint32_t total_size = input_size + output_size + sizeof(struct ipc_events);

	if (param->is_host) {
		fd = shm_open(param->shmem_name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	} else {
		fd = shm_open(param->shmem_name, O_RDWR, S_IRUSR | S_IWUSR);
	}
	SM_IPT_DBG("fd=%d", fd);
	ftruncate(fd, total_size);
	shared_memory = (uint8_t*)mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);
	ipc_events = (struct ipc_events*)&shared_memory[input_size + output_size];
	conf->input = &shared_memory[param->is_host ? 0 : output_size];
	conf->output = &shared_memory[param->is_host ? input_size : 0];
	os_ctx->ipc_in = param->is_host ? &ipc_events->sem_in : &ipc_events->sem_out;
	os_ctx->ipc_out = param->is_host ? &ipc_events->sem_out : &ipc_events->sem_in;
	SM_IPT_DBG("shmem=0x%08X %d", (int)(intptr_t)shared_memory, total_size);
	SM_IPT_DBG("input=0x%08X %d", (int)(intptr_t)conf->input, conf->input_size);
	SM_IPT_DBG("output=0x%08X %d", (int)(intptr_t)conf->output, conf->output_size);
	SM_IPT_DBG("ipc=0x%08X 0x%08X", (int)(intptr_t)os_ctx->ipc_in, (int)(intptr_t)os_ctx->ipc_out);
	SM_IPT_DBG("synchronizing 1 of 3");
	set_and_wait(conf->input, conf->output, 0xDE, 0xAD);
	set_and_wait(conf->input, conf->output, 0xAD, 0xBE);
	set_and_wait(conf->input, conf->output, 0xBE, 0xAF);
	if (param->is_host) {
		SM_IPT_DBG("sem_init");
		sem_init(os_ctx->ipc_in, 1, 0);
		sem_init(os_ctx->ipc_out, 1, 0);
	}
	SM_IPT_DBG("synchronizing 2 of 3");
	set_and_wait(conf->input, conf->output, 0xAF, 0xCA);
	set_and_wait(conf->input, conf->output, 0xCA, 0xFE);
	set_and_wait(conf->input, conf->output, 0xFE, 0xFE);

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
	SM_IPT_OS_MEMORY_BARRIER();
	sem_post(os_ctx->ipc_out);
}
