
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

#include "nrf_rpc.h"
#include "nrf_rpc_log.h"

#define ID_TEST 1

NRF_RPC_GROUP_DEFINE(test_group, "test_group", NULL, NULL, NULL);
NRF_RPC_GROUP_DEFINE(test_group21, "xyz", NULL, NULL, NULL);
NRF_RPC_GROUP_DEFINE(test_group3, "abc", NULL, NULL, NULL);


void test_rsp_handler(const uint8_t *packet, size_t len, void *handler_data)
{
	NRF_RPC_WRN("HANDLER %d", *(const uint32_t*)packet);
}

int test__;

void test_rpc(uint32_t x)
{
	uint8_t *packet;

	NRF_RPC_ALLOC(packet, 4);

	*(uint32_t*)packet = x;

	nrf_rpc_cmd_no_err(&test_group, ID_TEST, packet, 4, test_rsp_handler, &test__);
}

void test_rpc_handler(const uint8_t *packet, size_t len, void *handler_data)
{
	uint8_t *rsp;

	NRF_RPC_WRN("TEST RPC %d", *(const uint32_t*)packet);

	NRF_RPC_ALLOC(rsp, 4);

	*(uint32_t*)rsp = *(const uint32_t*)packet + 1;

	nrf_rpc_rsp_no_err(rsp, 4);
}


NRF_RPC_CMD_DECODER(test_group, test_rpc_dec, ID_TEST, test_rpc_handler, NULL);

static struct _nrf_rpc_auto_arr_item *first_item = NULL;
static void** auto_arr;

void _nrf_rpc_auto_arr_item_init(struct _nrf_rpc_auto_arr_item *item, const void *data, const char *key, bool is_array)
{
	item->data = data;
	item->key = key;
	item->is_array = is_array;
	item->next = first_item;
	first_item = item;
}

int auto_arr_cmp(const void *a, const void *b)
{
	struct _nrf_rpc_auto_arr_item **l = (struct _nrf_rpc_auto_arr_item **)a;
	struct _nrf_rpc_auto_arr_item **r = (struct _nrf_rpc_auto_arr_item **)b;

	return strcmp((*l)->key, (*r)->key);
}

static int auto_arr_init(void)
{
	size_t i;
	size_t j;
	size_t count = 0;
	struct _nrf_rpc_auto_arr_item **items;
	struct _nrf_rpc_auto_arr_item *item;

	item = first_item;
	while (item != NULL) {
		count++;
		item = item->next;
	}

	items = malloc(sizeof(void *) * (count + 1));
	if (items == NULL) {
		return -ENOMEM;
	}

	auto_arr = (void **)items;
	
	i = 0;
	item = first_item;
	while (item != NULL) {
		items[i] = item;
		i++;
		item = item->next;
	}
	items[i] = NULL;

	qsort(items, count, sizeof(struct _nrf_rpc_auto_arr_item *), auto_arr_cmp);

	NRF_RPC_DBG("AUTO_ARR items:");

	j = 0;
	for (i = 0; i < count; i++) {
		item = items[i];
		if (item->is_array) {
			auto_arr[i] = NULL;
			*(void ***)item->data = &auto_arr[i + 1];
			NRF_RPC_DBG("%03d array: %s", (int)i, item->key);
			j = 0;
		} else {
			auto_arr[i] = (void *)item->data;
			NRF_RPC_DBG("%03d        [%d] %s", (int)i, (int)j, item->key);
			j++;
		}
	}

	return 0;
}

int main()
{
	auto_arr_init();
	nrf_rpc_init(NULL);
	test_rpc(IS_MASTER ? 10 : 20);
	NRF_RPC_DBG("TEST DONE");
	sleep(1);
	return 0;
}

#if 0
int main()
{
	//nrf_rpc_tr_init();

	nrf_rpc_os_init();

	LOG("Start");

	test();
	sleep(200);
#if 0
		char x = '-';

	if (IS_MASTER) {
		sleep(1);
		//strcpy(shared_memory, "Test123");
		nrf_rpc_os_signal();
		sleep(1);
	} else {
		int i;
		//strcpy(shared_memory, "Old");
		for (i = 0; i < 30; i++) {
			usleep(1000000 / 10);
			//LOG("%s", shared_memory);
		}
	}

#endif
	if (IS_MASTER) {
		LOG("Waiting for child process");
		wait(NULL);
	}
	LOG("Exit");
	return 0;
}


#if 0

#define NRF_RPC_SHMEM_IN_SIZE 1024
#define NRF_RPC_SHMEM_OUT_SIZE 2048


int pid;
pid_t child = 0;
bool master;
const char* side;
volatile bool exit_process = false;
uint8_t *shared_memory;
uint8_t *memory_in;
uint8_t *memory_out;

uint8_t* create_shared_memory(size_t size) {
  return (uint8_t*)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
}

void sig_term(int signum)
{
	LOG("SIGTERM");
	exit_process = true;
}

void sig_alarm(int signum)
{
	LOG("Killing subprocess");
	kill(child, SIGKILL);
}

int main(int argc, char* argv[])
{
	int i;

	pid = (int)getpid();
	signal(SIGTERM,(void (*)(int))sig_term);
	signal(SIGALRM,(void (*)(int))sig_alarm);

	shared_memory = create_shared_memory(NRF_RPC_SHMEM_IN_SIZE + NRF_RPC_SHMEM_OUT_SIZE);

	child = fork();
	if (child == 0) {
		side = "SLAVE ";
		master = false;
		memory_in = &shared_memory[0];
		memory_out = &shared_memory[0];
	} else {
		side = "MASTER";
		master = true;
	}

	LOG("Running");

	if (master) {
		LOG("Terminating subprocess");
		alarm(1);
		kill(child, SIGTERM);
		wait(NULL);
	}

	LOG("Exit")

	return 0;
}
#endif
#endif
