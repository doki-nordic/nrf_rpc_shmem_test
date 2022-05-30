
#include <stdio.h>

#include "sm_ipt_log.h"
#include "sm_ipt_os.h"
#include "sm_ipt.h"

struct sm_ipt_ctx ctx;
struct sm_ipt_linux_param param;

void handler(struct sm_ipt_ctx *ctx, const uint8_t *packet, size_t len) {
	printf("Handler\n");
	sm_ipt_free_rx_buf(ctx, packet);
}

int main() {
	char* env;
	int err;

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
	param.host_input_size = 65536;
	param.host_output_size = 65536 / 2;

	err = sm_ipt_init(&ctx, handler, &param);
	SM_IPT_DBG("sm_ipt_init: %d", err);

	uint8_t* buf;
	sm_ipt_alloc_tx_buf(&ctx, &buf, 2048);
	buf[0] = 0x11;
	buf[1] = 0x22;
	buf[2] = 0x33;
	buf[3] = 0x44;
	sm_ipt_send(&ctx, buf, 4);

	sleep(5);

	return 0;
}
