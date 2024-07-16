#include "rve/memory.h"
#include "rve/peripheral.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef u64 (*read_fn)(void *, u64, u8);
typedef u64 (*write_fn)(void *, u64, u64, u8);

static struct peripheral_ops_t memory_ops = {
	.read = (read_fn)memory_read,
	.write = (write_fn)memory_write
};

peripheral_t *memory_init(u64 addr_start, u64 addr_end)
{
	peripheral_ops_t *ops = &memory_ops;

	memory_t *m = malloc(sizeof(memory_t));
	assert(m != NULL);

	m->memory_size = addr_end - addr_start;
	m->raw_memory = malloc(m->memory_size);
	assert(m->raw_memory != NULL);

	peripheral_t *peripheral =
		peripheral_init(addr_start, addr_end, (void *)m, ops, "memory");

	return peripheral;
}

int memory_load_file(memory_t *m, u64 addr, char *program_data,
		     size_t program_size)
{
	// TODO gap para el stack
	if (program_size >= m->memory_size)
		return -1;

	memcpy(&m->raw_memory[addr - RVE_MEMORY_ADDR_START], program_data,
	       program_size);
	return 0;
}

u64 memory_read(memory_t *m, u64 addr, u8 size)
{
	u64 value = 0;
	memcpy(&m->raw_memory[addr], &value, size);
	return value;
}

void memory_write(memory_t *m, u64 addr, u64 data, u8 size)
{
	memcpy(&m->raw_memory[addr], &data, size);
}
