/**
 * @file soc.c
 * @brief System-on-Chip implementation.
 */

#include "rve/soc.h"
#include "rve/bus.h"
#include "rve/hart.h"
#include "rve/peripherals/memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void soc_init(soc_t *soc, word reset_pc)
{
    memset(soc, 0, sizeof(*soc));
    bus_init(&soc->bus);
    hart_init(&soc->hart, &soc->bus, reset_pc);
}

bool soc_add_peripheral(soc_t *soc,
                        word addr_start, word addr_end,
                        void *ctx, peripheral_ops_t *ops,
                        const char *name)
{
    return bus_add_peripheral(&soc->bus, addr_start, addr_end, ctx, ops, name);
}

bool soc_add_memory(soc_t *soc, word addr_start, size_t size)
{
    memory_t *mem = memory_create_ctx(addr_start, size);
    if (!mem) {
        fprintf(stderr, "SOC: Failed to allocate memory context\n");
        return false;
    }
    if (!memory_init(mem)) {
        fprintf(stderr, "SOC: Failed to initialise memory\n");
        free(mem);
        return false;
    }
    if (!bus_add_peripheral(&soc->bus, addr_start, addr_start + (word)size,
                            mem, &memory_ops, "RAM")) {
        fprintf(stderr, "SOC: Failed to register memory on bus\n");
        memory_deinit(mem);
        free(mem);
        return false;
    }
    return true;
}

int soc_load_binary(soc_t *soc, const char *data, size_t size, word load_addr)
{
    peripheral_t *p = bus_get_peripheral(&soc->bus, load_addr);
    if (!p) {
        fprintf(stderr, "SOC: No peripheral at load address 0x%08X\n", load_addr);
        return -1;
    }

    memory_t *mem = (memory_t *)p->ctx;
    return memory_load_file(mem, load_addr, (char *)data, size);
}

hart_retire_t soc_step(soc_t *soc)
{
    return hart_step(&soc->hart);
}

void soc_raise_interrupt(soc_t *soc, trap_t interrupt)
{
    if (!TRAP_IS_INTERRUPT(interrupt)) {
        fprintf(stderr, "SOC: soc_raise_interrupt called with non-interrupt cause %d\n",
                interrupt);
        return;
    }
    hart_take_trap(&soc->hart, interrupt, 0);
}
