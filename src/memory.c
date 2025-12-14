#include "rve/memory.h"
#include "rve/peripheral.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static struct peripheral_ops_t memory_ops = {
	.init = (peripheral_init_fn)memory_init,
	.deinit = (peripheral_deinit_fn)memory_deinit,
	.read = (peripheral_read_fn)memory_read,
	.write = (peripheral_write_fn)memory_write
};

bool memory_init(peripheral_t *p)
{
	assert(p != NULL);

	memory_t *m = (memory_t *)p->ctx;

	memory_deinit((memory_t *)p->ctx);

	m->size = p->addr_end - p->addr_start;
	m->raw_memory = malloc(m->size);
	return true;
}

bool memory_deinit(peripheral_t* p)
{
	assert(p != NULL);

	memory_t *m = (memory_t *)p->ctx;

	if (m == NULL)
		return true;

	if (m->raw_memory == NULL)
		return true;

	free(m->raw_memory);
	m->raw_memory = NULL;

	return true;
}

int memory_load_file(memory_t *m, u64 addr, char *program_data, size_t program_size)
{
	// TODO gap para el stack
	if (program_size >= m->size)
		return -1;

	memcpy(&m->raw_memory[addr - RVE_MEMORY_ADDR_START], program_data, program_size);
	return 0;
}

void memory_load_instruction(memory_t *m, word addr, word instruction)
{
	m->raw_memory[addr - RVE_MEMORY_ADDR_START] = instruction;
}

bool memory_read(memory_t *m, u64 addr, u8 size, u64* result)
{
	memcpy(&result, &m->raw_memory[addr], size);
	return true;
}

bool memory_write(memory_t *m, u64 addr, u8 size, u64 data)
{
	memcpy(&m->raw_memory[addr], &data, size);
	return true;
}
