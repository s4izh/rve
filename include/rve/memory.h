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
	char *raw_memory;
	size_t memory_size;
};

u64 memory_read(memory_t *m, u64 addr, u8 size);
void memory_write(memory_t *m, u64 addr, u64 data, u8 size);

peripheral_t *memory_init(u64 addr_start, u64 addr_end);

int memory_load_file(memory_t *m, u64 addr, char *program_data, size_t program_size);

#endif // RVE_MEMORY_H
