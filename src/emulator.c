#include "rve/emulator.h"
#include "rve/bus.h"
#include "rve/cpu.h"
#include "rve/memory.h"
#include "rve/utils.h"
#include "rve/decoder.h"
#include <stdio.h>

int rve_run_binary(emulator_t *e, const char *program_filename)
{
	int res;

	char *program_data;
	size_t program_size;

	u32 pc = 0;
	cpu_init(&e->cpu, &e->bus, pc);

	u32 ins = cpu_fetch(&e->cpu);

	while (ins) {
		decoded_instruction_t di = rve_decode_instruction(ins);
		trap_t trap = cpu_execute(&e->cpu, &di);
		if (trap != TRAP_OK) {
			char instr[32];
			rve_decoded_format_to_buffer(&di, instr, sizeof(instr));
			fprintf(stderr, "Error executing: %s\n", instr);
			return -1;
		}
		ins = cpu_fetch(&e->cpu);
	}

	return 0;
}

peripheral_t memory = {
	.addr_start = RVE_MEMORY_ADDR_START,
	.addr_end = RVE_MEMORY_ADDR_END,
	.ctx = NULL,
	.ops = NULL,
	.name = "Memory",
};

// peripheral_t* peripherals[] = {
// 	&memory,
// 	NULL,
// };

trap_t rve_emulator_cycle(emulator_t *e);

void rve_emulator_init(emulator_t *e)
{
	bus_init(&e->bus);
	cpu_init(&e->cpu, &e->bus, 0);

	// Initialize the bus with peripherals
	bus_add_peripheral(&e->bus, &memory);

	// for (int i = 0; peripherals[i] != NULL; i++) {
	// 	bus_add_peripheral(&e->bus, peripherals[i]);
	// }

	memory_t *mem = malloc(sizeof(memory_t));

	bus_add_peripheral(&e->bus, RVE_MEMORY_ADDR_START, RVE_MEMORY_ADDR_END, mem, &memory_ops, "Memory");

	static peripheral_t memory = {
		.addr_start = 0x1000,
		.addr_end = 0x2000,
		.ctx = NULL,
		.ops = NULL,
		.name = "Memory",
	};

	static peripheral_t vga = {
		.addr_start = 0x2000,
		.addr_end = 0x3000,
		.ctx = NULL,
		.ops = NULL,
		.name = "VGA",
	};

	memory_t *mem = malloc(sizeof(memory_t));
	mem->size = RVE_MEMORY_SIZE;

	bus_add_peripheral(&e->bus, &memory);
	bus_add_peripheral(&e->bus, &vga);

	// bus_init_peripherals(&e->bus);

	while (true) {
		rve_emulator_cycle(e);
	}
}

trap_t rve_emulator_cycle(emulator_t *e)
{
	word instruction = cpu_fetch(&e->cpu);
	decoded_instruction_t di = rve_decode_instruction(instruction);

	if (!di.valid) {
		fprintf(stderr, "Invalid instruction: 0x%08X\n", instruction);
		return TRAP_ERR;
	}

	trap_t trap = cpu_execute(&e->cpu, &di);

	if (trap != TRAP_OK) {
		fprintf(stderr, "Error executing instruction: %s\n", rve_instruction_op_to_cstr(di.op));
		return trap;
	}

	return TRAP_OK;
}
