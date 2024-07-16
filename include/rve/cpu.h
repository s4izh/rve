#ifndef RVE_CPU_H
#define RVE_CPU_H

#include "rve/bus.h"
#include "rve/types.h"

typedef struct cpu_t cpu_t;

struct cpu_t {
	bus_t *bus;
	u64 registers[32]; // 32 integer registers
	u64 pc;
};

void cpu_init(cpu_t *cpu, bus_t *bus, u64 pc);
void cpu_execute(cpu_t *cpu, u32 instruction);
u32 cpu_fetch(cpu_t *cpu);

#endif // RVE_CPU_H
