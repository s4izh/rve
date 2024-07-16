#include "rve/cpu.h"
#include "rve/bus.h"

void cpu_init(cpu_t *cpu, bus_t *bus, u64 pc)
{
	cpu->bus = bus;
	cpu->pc = pc;
}

void cpu_execute(cpu_t *cpu, u32 instruction)
{
}

u32 cpu_fetch(cpu_t *cpu)
{
	return bus_read(cpu->bus, cpu->pc, 32);
}
