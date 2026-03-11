/**
 * @file memory.c
 * @brief Flat RAM peripheral implementation.
 */

#include "rve/peripherals/memory.h"
#include "rve/peripherals/peripheral.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

// Exported ops (declared in header)
peripheral_ops_t memory_ops = {
	.init = memory_init,
	.deinit = memory_deinit,
	.read = memory_read,
	.write = memory_write,
	.tick = NULL,
};

memory_t *memory_create_ctx(word base_addr, size_t size)
{
	memory_t *m = malloc(sizeof(memory_t));
	if (!m) return NULL;
	m->raw_memory = NULL;
	m->size = size;
	m->base_addr = base_addr;
	return m;
}

bool memory_init(void *ctx)
{
	if (!ctx) return false;
	memory_t *m = (memory_t *)ctx;
	if (m->raw_memory) {
		free(m->raw_memory);
		m->raw_memory = NULL;
	}
	m->raw_memory = calloc(1, m->size);
	return m->raw_memory != NULL;
}

bool memory_deinit(void *ctx)
{
	if (!ctx) return true;
	memory_t *m = (memory_t *)ctx;
	if (m->raw_memory) {
		free(m->raw_memory);
		m->raw_memory = NULL;
	}
	return true;
}

int memory_load_file(memory_t *m, word addr, char *program_data, size_t program_size)
{
	if (!m || !m->raw_memory) return -1;
	if (addr < m->base_addr) return -1;
	size_t offset = (size_t)(addr - m->base_addr);
	if (offset + program_size > m->size) return -1;
	memcpy(&m->raw_memory[offset], program_data, program_size);
	return 0;
}

void memory_load_instruction(memory_t *m, word addr, word instruction)
{
	if (!m || !m->raw_memory) return;
	if (addr < m->base_addr) return;
	size_t offset = (size_t)(addr - m->base_addr);
	if (offset + sizeof(word) > m->size) return;
	memcpy(&m->raw_memory[offset], &instruction, sizeof(word));
}

bool memory_read(void *ctx, word addr, u8 size_bits, word* result)
{
	if (!ctx || !result) return false;
	memory_t *m = (memory_t *)ctx;
	u8 bytes = (u8)(size_bits / 8);
	if (bytes == 0 || bytes > 8) return false;
	if (addr < m->base_addr) return false;
	size_t offset = (size_t)(addr - m->base_addr);
	if (offset + bytes > m->size) return false;

	/* Zero result and copy little-endian bytes into it */
	*result = 0;
	memcpy(result, &m->raw_memory[offset], bytes);
	return true;
}

bool memory_write(void *ctx, word addr, word data, u8 size_bits)
{
	if (!ctx) return false;
	memory_t *m = (memory_t *)ctx;
	u8 bytes = (u8)(size_bits / 8);
	if (bytes == 0 || bytes > 8) return false;
	if (addr < m->base_addr) return false;
	size_t offset = (size_t)(addr - m->base_addr);
	if (offset + bytes > m->size) return false;

	memcpy(&m->raw_memory[offset], &data, bytes);
	return true;
}

void memory_flush(memory_t *m)
{
	if (!m || !m->raw_memory) return;
	memset(m->raw_memory, 0, m->size);
}
