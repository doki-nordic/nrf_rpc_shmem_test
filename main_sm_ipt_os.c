
#include <stdio.h>

#include "sm_ipt_log.h"
#include "sm_ipt_os.h"

struct sm_ipt_os_ctx os_ctx;
struct sm_ipt_linux_param param;
struct sm_ipt_os_shmem_conf conf;

void handler(struct sm_ipt_os_ctx *ctx) {
	printf("Handler\n");
}

int main() {
	char* env;

	nrf_rpc_log_set_thread_name("main");

	env = getenv("NRF_RPC_SHMEM_NAME");
	if (env != NULL) {
		char* str = malloc(strlen(env) + 2);
		strcpy(str, "/");
		strcat(str, env);
		param.shmem_name = str;
	} else {
		param.shmem_name = "SM_IPT_SHMEM2";
	}
	SM_IPT_INF("Shared memory name: %s", param.shmem_name);

	if (IS_MASTER) {
		env = getenv("NRF_RPC_RUN_SLAVE");
		if (env != NULL && fork() == 0) {
			SM_IPT_INF("[child] Starting slave");
			system(env);
			SM_IPT_INF("[child] Slave done");
			SM_IPT_INF("[child] Exit");
			exit(0);
		}
	}

	param.is_host = IS_MASTER;
	param.input_size = 65536;
	param.output_size = 65536 / 2;

	if (!IS_MASTER) {
		uint32_t t = param.input_size;
		param.input_size = param.output_size;
		param.output_size = t;
	}

	sm_ipt_os_init_shmem(&os_ctx, &param, &conf);
	sm_ipt_os_init_signals(&os_ctx, &param, handler);

	sleep(1);
	sm_ipt_os_signal(&os_ctx);

	sleep(10);

	return 0;
}
