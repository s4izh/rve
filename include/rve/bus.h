#ifndef RVE_BUS_H
#define RVE_BUS_H

#include "rve/peripheral.h"
#include "rve/types.h"

#include <stdlib.h>

typedef struct bus_t bus_t;

struct bus_t {
	peripheral_t **peripherals;
	size_t count;
	size_t capacity;
};

bus_t* bus_init();
void bus_add_peripheral(bus_t *bus, peripheral_t *peripheral);
peripheral_t* bus_get_peripheral(bus_t *bus, u64 addr);

void bus_write(bus_t *bus, u64 addr, u64 data, u8 size);
u64 bus_read(bus_t *bus, u64 addr, u8 size);

#endif // RVE_BUS_H
