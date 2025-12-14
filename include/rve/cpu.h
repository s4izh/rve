#ifndef RVE_CPU_H
#define RVE_CPU_H

#include "rve/bus.h"
#include "rve/types.h"
#include "rve/decoder.h"

#define XLEN 32
#define ILEN 32

#define XLEN_MASK 0xFFFFFFFF

typedef struct {
	word regs[XLEN];
	word pc;
	bus_t* bus;
} cpu_t;

typedef enum {
	TRAP_OK,
	TRAP_ERR,
} trap_t;

void cpu_init(cpu_t *cpu, bus_t *bus, u64 pc);
word cpu_fetch(cpu_t *cpu);
trap_t cpu_execute(cpu_t *cpu, decoded_instruction_t* di);

#endif // RVE_CPU_H
