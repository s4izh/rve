#include "rve/emulator.h"
#include "rve/cpu.h"
#include "rve/memory.h"
#include "rve/utils.h"
#include <stdio.h>

int rve_run_binary(emulator_t *e, const char *program_filename)
{
	int res;

	char *program_data;
	size_t program_size;

	res = rve_file_to_string(program_filename, &program_data,
				 &program_size);
	if (res < 0) {
		fprintf(stderr, "Failed to read program: %s\n",
			program_filename);
		return -1;
	}

	bus_t *bus = bus_init();
	peripheral_t *memory =
		memory_init(RVE_MEMORY_ADDR_START, RVE_MEMORY_ADDR_END);

	res = memory_load_file(memory->ctx, RVE_MEMORY_ADDR_START, program_data,
			       program_size);
	if (res < 0) {
		fprintf(stderr, "Failed to load program: %s\n",
			program_filename);
		return -1;
	}

	free(program_data);

	bus_add_peripheral(e->cpu.bus, memory);

	u32 pc = 0;
	cpu_init(&e->cpu, bus, pc);

	u32 ins = cpu_fetch(&e->cpu);

	while (ins) {
		cpu_execute(&e->cpu, ins);
		ins = cpu_fetch(&e->cpu);
	}

	return 0;
}
