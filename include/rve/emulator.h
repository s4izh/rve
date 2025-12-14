#ifndef RVE_EMULATOR_H
#define RVE_EMULATOR_H

#include "rve/cpu.h"

#define EMULATOR_MAX_PERIPHERALS 10

struct emulator_t {
	cpu_t cpu;
	bus_t bus;
	int kernel_mode;
};
typedef struct emulator_t emulator_t;

int rve_run_binary(emulator_t* e, const char* program_filename);

#endif // !RVE_EMULATOR_H
