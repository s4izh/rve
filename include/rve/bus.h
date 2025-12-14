#ifndef RVE_BUS_H
#define RVE_BUS_H

#include "rve/peripheral.h"
#include "rve/types.h"

#include <stdlib.h>

#define BUS_MAX_PERIPHERALS 10

struct bus_t {
	peripheral_t peripherals[BUS_MAX_PERIPHERALS];
	size_t count;
};

typedef struct bus_t bus_t;

void bus_init(bus_t* bus);
// void bus_add_peripheral(bus_t *bus, peripheral_t *peripheral);
bool bus_add_peripheral(bus_t *bus, u64 addr_start, u64 addr_end, void* ctx, peripheral_ops_t* ops, const char* name);

peripheral_t* bus_get_peripheral(bus_t *bus, u64 addr);

bool bus_write(bus_t *bus, word addr, u8 size, word data);
bool bus_read(bus_t *bus, word addr, u8 size, word* data);

#endif // RVE_BUS_H
