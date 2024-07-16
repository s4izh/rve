#ifndef RVE_EMULATOR_H
#define RVE_EMULATOR_H

#include "rve/cpu.h"
typedef struct emulator_t emulator_t;

struct emulator_t {
	cpu_t cpu;
	int kernel_mode;
};

int rve_run_binary(emulator_t* e, const char* program_filename);

#endif // !RVE_EMULATOR_H
