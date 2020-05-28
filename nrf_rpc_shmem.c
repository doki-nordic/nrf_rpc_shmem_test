#include <stdint.h>
#include <stdbool.h>

#include "conf.h"
#include "nrf_rpc_os.h"
#include "nrf_rpc_shmem.h"

#if 0

#define WORD_SIZE sizeof(uint32_t)

#if 0
typedef uint32_t mask_t;
#define NUM_BLOCKS 32
#else
typedef uint64_t mask_t;
#define NUM_BLOCKS 64
#endif

#define SLOT_STATE_MAX         0xFFFFFFF0uL
#define SLOT_HANDSHAKE_INIT    0xFFFFFFF1uL
#define SLOT_HANDSHAKE_CONFIRM 0xFFFFFFF2uL
#define SLOT_EMPTY             0xFFFFFFFFuL
#define SLOT_PENDING           0xFFFFFFFEuL

#define ALLOCABLE_MULTIPLY (WORD_SIZE * NUM_BLOCKS)

#define OUT_SIZE_TOTAL NRF_RPC_SHMEM_OUT_SIZE
#define OUT_SIZE_SLOTS (WORD_SIZE * NRF_RPC_OUT_ENDPOINTS)
#define OUT_MIN_SIZE_QUEUE (2 * WORD_SIZE + (NRF_RPC_OUT_ENDPOINTS + NRF_RPC_IN_ENDPOINTS + 1))
#define OUT_SIZE_ALLOCABLE (((OUT_SIZE_TOTAL - OUT_SIZE_SLOTS - OUT_MIN_SIZE_QUEUE) / ALLOCABLE_MULTIPLY) * ALLOCABLE_MULTIPLY)
#define OUT_SIZE_QUEUE (OUT_SIZE_TOTAL - OUT_SIZE_ALLOCABLE - OUT_SIZE_SLOTS)
#define OUT_QUEUE_ITEMS (OUT_SIZE_QUEUE - 2 * WORD_SIZE)

#define OUT_BLOCK_SIZE (OUT_SIZE_ALLOCABLE / NUM_BLOCKS)

#define IN_SIZE_TOTAL NRF_RPC_SHMEM_IN_SIZE
#define IN_SIZE_SLOTS (WORD_SIZE * NRF_RPC_IN_ENDPOINTS)
#define IN_MIN_SIZE_QUEUE (2 * WORD_SIZE + (NRF_RPC_IN_ENDPOINTS + NRF_RPC_IN_ENDPOINTS + 1))
#define IN_SIZE_ALLOCABLE (((IN_SIZE_TOTAL - IN_SIZE_SLOTS - IN_MIN_SIZE_QUEUE) / ALLOCABLE_MULTIPLY) * ALLOCABLE_MULTIPLY)
#define IN_SIZE_QUEUE (IN_SIZE_TOTAL - IN_SIZE_ALLOCABLE - IN_SIZE_SLOTS)
#define IN_QUEUE_ITEMS (IN_SIZE_QUEUE - 2 * WORD_SIZE)

#define IN_BLOCK_SIZE (IN_SIZE_ALLOCABLE / NUM_BLOCKS)

#if NRF_RPC_OS_SHMEM_PTR_CONST

static uint8_t *const out_allocable = (uint8_t *)nrf_rpc_os_out_shmem_ptr;
static uint32_t *const out_slots = (uint32_t *)&out_allocable[OUT_SIZE_ALLOCABLE];
static uint32_t *const out_queue_tx = (uint32_t *)&out_allocable[OUT_SIZE_ALLOCABLE + OUT_SIZE_SLOTS];
static uint32_t *const out_queue_rx = (uint32_t *)&out_allocable[OUT_SIZE_ALLOCABLE + OUT_SIZE_SLOTS + WORD_SIZE];
static uint8_t *const out_queue = (uint8_t *)&out_allocable[OUT_SIZE_ALLOCABLE + OUT_SIZE_SLOTS + 2 * WORD_SIZE];

static uint8_t *const in_allocable = (uint8_t *)nrf_rpc_os_in_shmem_ptr;
static uint32_t *const in_slots = (uint32_t *)&in_allocable[IN_SIZE_ALLOCABLE];
static uint32_t *const in_queue_tx = (uint32_t *)&in_allocable[IN_SIZE_ALLOCABLE + IN_SIZE_SLOTS];
static uint32_t *const in_queue_rx = (uint32_t *)&in_allocable[IN_SIZE_ALLOCABLE + IN_SIZE_SLOTS + WORD_SIZE];
static uint8_t *const in_queue = (uint8_t *)&in_allocable[IN_SIZE_ALLOCABLE + IN_SIZE_SLOTS + 2 * WORD_SIZE];

#else

static uint8_t *out_allocable;
static uint32_t *out_slots;
static uint32_t *out_queue_tx;
static uint32_t *out_queue_rx;
static uint8_t *out_queue;

static uint8_t *in_allocable;
static uint32_t *in_slots;
static uint32_t *in_queue_tx;
static uint32_t *in_queue_rx;
static uint8_t *in_queue;

#endif

static nrf_rpc_os_mutex_t out_mutex;
static nrf_rpc_os_sem_t out_sem;
static mask_t free_mask = ~(mask_t)0;
static mask_t endpoint_mask[NRF_RPC_OUT_ENDPOINTS];


int signal_with_data(uint8_t data)
{
	uint32_t tx = *out_queue_tx;
	uint32_t rx = *out_queue_rx;
	uint32_t dst = tx;

	tx++;
	if (tx >= OUT_QUEUE_ITEMS) {
		tx = 0;
	}

	if (tx == rx || dst >= OUT_QUEUE_ITEMS) {
		return NRF_RPC_ERR_NO_MEM;
	}

	out_queue[dst] = data;
	NRF_RPC_OS_MEMORY_BARIER();
	*out_queue_tx = tx;

	return nrf_rpc_os_signal();
}


int signal_data_get()
{
	uint32_t tx = *in_queue_tx;
	uint32_t rx = *in_queue_rx;
	uint8_t data;

	tx = *in_queue_tx;
	rx = *in_queue_rx;

	if (tx == rx || rx >= IN_QUEUE_ITEMS) {
		return NRF_RPC_ERR_EMPTY;
	}

	NRF_RPC_OS_MEMORY_BARIER();

	data = in_queue[rx];

	rx++;
	if (rx >= IN_QUEUE_ITEMS) {
		rx = 0;
	}

	*in_queue_rx = rx;

	return data;
}

uint8_t *out_shmem_alloc(uint32_t addr, size_t size)
{
	size_t i;
	size_t blocks = (size + (4 + OUT_BLOCK_SIZE - 1)) / OUT_BLOCK_SIZE;
	bool sem_taken = false;
	mask_t sh_mask;

	if (blocks == 0 || blocks > NUM_BLOCKS) {
		return NULL;
	}

	/* if this slot was not consumed yet wait for it */
	while (out_slots[addr] <= SLOT_STATE_MAX) {
		nrf_rpc_os_sem_take(&out_sem);
		sem_taken = true;
	}

	//k_mutex_lock(&out_mutex, K_FOREVER); // Maybe lock scheduler?

	// if this slot is empty or pending reclaim its memory
	free_mask ^= endpoint_mask[addr];
	endpoint_mask[addr] = 0;

	do {
		// create shifted mask with bits set where `blocks` can be allocated
		sh_mask = free_mask;
		for (i = 1; i < blocks; i++) {
			sh_mask &= (sh_mask >> 1);
		}

		// if no memory
		if (sh_mask == 0) {
			// wait for any slot to be empty
			k_mutex_unlock(&out_mutex);
			nrf_rpc_os_sem_take(&out_sem);
			sem_taken = true;
			//k_mutex_lock(&out_mutex, K_FOREVER);
			// if any slot is empty reclaim its memory
			for (i = 0; i < NRF_RPC_OUT_ENDPOINTS; i++) {
				if (out_slots[i] == SLOT_EMPTY) {
					free_mask ^= endpoint_mask[i];
					endpoint_mask[i] = 0;
				}
			}
		}

	} while (sh_mask == 0);

	// get first available blocks
	size_t free_index = MASK_CTZ(sh_mask);
	// create bit mask with blocks that will be used
	mask_t mask = ((blocks == NUM_BLOCKS) ? ~(mask_t)0 : (((mask_t)1 << blocks) - 1)) << free_index;
	// update masks
	free_mask ^= mask;
	endpoint_mask[addr] = mask;
	// mark this slot as pending: memory cannot be reclaimed and remote side will not consume it
	out_slots[addr] = SLOT_PENDING;

	k_mutex_unlock(&out_mutex);
	
	// Give semaphore back, because there may be some other thread waiting
	if (sem_taken && free_mask != 0) {
		k_sem_give(&out_sem);
	}

	uint32_t *mem_start = (uint32_t *)&out_allocable[OUT_BLOCK_SIZE * free_index];

	mem_start[0] = size;

	return &mem_start[1];
}


// send allocated memory: set slot from pending to specific index
void out_shmem_send(uint32_t addr, uint8_t* buffer)
{
	out_slots[addr] = buffer - 4 - out_allocable;
	signal_with_data(addr);
}



// discard allocated buffer without sending
void out_shmem_discard(uint32_t addr)
{
	out_slots[addr] = SLOT_EMPTY;

	//k_mutex_lock(&out_mutex, K_FOREVER); // Maybe lock scheduler?

	// reclaim its memory
	free_mask ^= endpoint_mask[addr];
	endpoint_mask[addr] = 0;

	k_mutex_unlock(&out_mutex);

	k_sem_give(&out_sem);
}

// receive data from input shared memory
int in_shmem_recv(uint32_t addr, uint8_t **buf)
{
	uint32_t index = in_slots[addr];

	if (index > SLOT_STATE_MAX) {
		return NRF_RPC_ERR_EMPTY;
	} else if (index > OUT_SIZE_ALLOCABLE - 4) {
		return NRF_RPC_ERR_INTERNAL;
	}

	uint32_t *mem_start = (uint32_t *)&in_allocable[index];
	size_t size = mem_start[0];

	if (index + 4 + size > OUT_SIZE_ALLOCABLE) {
		in_shmem_consume(addr);
		return NRF_RPC_ERR_INTERNAL;
	}

	*buf = &mem_start[1];
	
	return size;
}

// consume incoming data i.e. mark slot as empty and signal remote about it
void in_shmem_consume(uint32_t addr)
{
	out_slots[addr] = SLOT_EMPTY;
	signal_with_data(0xFF);
}

#define O(x) ((int)((uint8_t*)(x) - base))

void print_addresses()
{
	uint8_t *base = (uint8_t *)nrf_rpc_os_out_shmem_ptr;
	if ((uint8_t *)nrf_rpc_os_in_shmem_ptr < base) {
		base = (uint8_t *)nrf_rpc_os_in_shmem_ptr;
	}
	usleep(IS_MASTER ? 100000 : 400000);
	LOG("    out_allocable %8d", O(out_allocable));
	LOG("    out_slots     %8d", O(out_slots));
	LOG("    out_queue_tx  %8d", O(out_queue_tx));
	LOG("    out_queue_rx  %8d", O(out_queue_rx));
	LOG("    out_queue     %8d", O(out_queue));
	LOG("    in_allocable  %8d", O(in_allocable));
	LOG("    in_slots      %8d", O(in_slots));
	LOG("    in_queue_tx   %8d", O(in_queue_tx));
	LOG("    in_queue_rx   %8d", O(in_queue_rx));
	LOG("    in_queue      %8d", O(in_queue));
	usleep(IS_SLAVE ? 100000 : 200000);
}

static void handshake_set_and_wait(uint32_t state)
{
	out_slots[0] = state;
	while (in_slots[0] != state) {
		///nrf_rpc_os_sem_take_timeout(&out_sem, HANDSHAKE_WAIT_TIMEOUT);
		NRF_RPC_OS_MEMORY_BARIER();
	}
}

int nrf_rpc_tr_init()
{
	int err;
	
	err = nrf_rpc_os_init();
	if (err != NRF_RPC_SUCCESS) {
		return err;
	}

#	if !NRF_RPC_OS_SHMEM_PTR_CONST
	out_allocable = (uint8_t *)nrf_rpc_os_out_shmem_ptr;
	out_slots = (uint32_t *)&out_allocable[OUT_SIZE_ALLOCABLE];
	out_queue_tx = (uint32_t *)&out_allocable[OUT_SIZE_ALLOCABLE + OUT_SIZE_SLOTS];
	out_queue_rx = (uint32_t *)&out_allocable[OUT_SIZE_ALLOCABLE + OUT_SIZE_SLOTS + WORD_SIZE];
	out_queue = (uint8_t *)&out_allocable[OUT_SIZE_ALLOCABLE + OUT_SIZE_SLOTS + 2 * WORD_SIZE];
	in_allocable = (uint8_t *)nrf_rpc_os_in_shmem_ptr;
	in_slots = (uint32_t *)&in_allocable[IN_SIZE_ALLOCABLE];
	in_queue_tx = (uint32_t *)&in_allocable[IN_SIZE_ALLOCABLE + IN_SIZE_SLOTS];
	in_queue_rx = (uint32_t *)&in_allocable[IN_SIZE_ALLOCABLE + IN_SIZE_SLOTS + WORD_SIZE];
	in_queue = (uint8_t *)&in_allocable[IN_SIZE_ALLOCABLE + IN_SIZE_SLOTS + 2 * WORD_SIZE];
#	endif

	print_addresses();

	handshake_set_and_wait(SLOT_HANDSHAKE_INIT);

	memset(&out_slots[1], 0xFF, WORD_SIZE * (NRF_RPC_OUT_ENDPOINTS - 1));
	*out_queue_tx = 0;
	*out_queue_rx = 0;
	memset(&in_slots[1], 0xFF, WORD_SIZE * (NRF_RPC_IN_ENDPOINTS - 1));
	*in_queue_tx = 0;
	*in_queue_rx = 0;

	handshake_set_and_wait(SLOT_HANDSHAKE_CONFIRM);
	handshake_set_and_wait(SLOT_EMPTY);
}

void dump_queue()
{
	int i;
	LOG("dump_queue OUT");
	for (i = 0; i < 10; i++) {
		LOG("   %02X", out_queue_tx[i]);
	}
	LOG("dump_queue IN");
	for (i = 0; i < 10; i++) {
		LOG("   %02X", in_queue_tx[i]);
	}
}

bool handshake_active = true;

void nrf_rpc_os_signaled()
{
	if (handshake_active) {
		nrf_rpc_os_sem_give(&out_sem);
	} else {
		do {
			int addr = signal_data_get();
			if (addr < 0) {
				break;
			} else if (addr < NRF_RPC_IN_ENDPOINTS) {
				//nrf_rpc_os_sem_give(&in_endpoints[addr].in_sem);
			} else {
				nrf_rpc_os_sem_give(&out_sem);
			}
		while (true);
	}
}


void test()
{
	LOG("Signaling");
	int i = signal_with_data(IS_SLAVE ? 12 : 112);
	LOG("Done %d", i);
}
#endif

void nrf_rpc_os_signaled()
{
}

void test()
{
	LOG("DONE");
}
