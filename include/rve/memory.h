#ifndef RVE_MEMORY_H
#define RVE_MEMORY_H

#include "rve/types.h"
#include "rve/peripheral.h"

#include <stdlib.h>

#define RVE_MEMORY_ADDR_START 0x00000000
#define RVE_MEMORY_ADDR_END 0x00000400
#define RVE_MEMORY_SIZE (RVE_MEMORY_ADDR_END - RVE_MEMORY_ADDR_START)

typedef struct memory_t memory_t;

struct memory_t {
	u8 *raw_memory;
	size_t size;
};

bool memory_init(peripheral_t *p);
bool memory_read(memory_t *m, u64 addr, u8 size, u64* result);
bool memory_write(memory_t *m, u64 addr, u8 size, u64 value);

void memory_load_instruction(memory_t *m, word addr, word instruction);

memory_t *memory_create_ctx(size_t size);

void memory_flush(memory_t *m);

int memory_load_file(memory_t *m, u64 addr, char *program_data, size_t program_size);

#endif // RVE_MEMORY_H
