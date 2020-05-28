
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

#include "conf.h"
#include "shmem_linux.h"



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