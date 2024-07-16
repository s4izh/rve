#include "rve/bus.h"
#include "rve/peripheral.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

bus_t* bus_init()
{
	bus_t* bus = malloc(sizeof(bus_t));
	assert(bus != NULL);

	bus->peripherals = malloc(4 * sizeof(peripheral_t *));
	bus->count = 0;
	bus->capacity = 4;

	return bus;
}

void bus_add_peripheral(bus_t *bus, peripheral_t *peripheral)
{
	assert(bus != NULL);
	assert(peripheral != NULL);

	if (bus->count >= bus->capacity) {
		size_t new_capacity = 2 * bus->capacity;
		peripheral_t **new_peripherals = realloc(
			bus->peripherals, new_capacity * sizeof(peripheral_t *));

		assert(new_peripherals != NULL);

		bus->peripherals = new_peripherals;
		bus->capacity = new_capacity;
	}

	bus->peripherals[bus->count] = peripheral;
	bus->count++;
}

peripheral_t *bus_get_peripheral(bus_t *bus, u64 addr)
{
	for (size_t i = 0; i < bus->count; ++i) {
		peripheral_t *peripheral = bus->peripherals[i];
		if (peripheral->addr_start >= addr &&
		    peripheral->addr_end <= addr) {
			return peripheral;
		}
	}
	return NULL;
}

u64 bus_read(bus_t *bus, u64 addr, u8 size)
{
	peripheral_t *peripheral = bus_get_peripheral(bus, addr);

	assert(peripheral != NULL);

	switch (size) {
	case 8:
		return peripheral->ops->read8(peripheral->ctx, addr);
	case 16:
		return peripheral->ops->read16(peripheral->ctx, addr);
	case 32:
		return peripheral->ops->read32(peripheral->ctx, addr);
	case 64:
		return peripheral->ops->read64(peripheral->ctx, addr);
	}

	return -1;
}

void bus_write(bus_t *bus, u64 addr, u64 data, u8 size)
{
	peripheral_t *peripheral = bus_get_peripheral(bus, addr);

	assert(peripheral != NULL);

	switch (size) {
	case 8:
		peripheral->ops->write8(peripheral->ctx, addr, data);
		break;
	case 16:
		peripheral->ops->write16(peripheral->ctx, addr, data);
		break;
	case 32:
		peripheral->ops->write32(peripheral->ctx, addr, data);
		break;
	case 64:
		peripheral->ops->write64(peripheral->ctx, addr, data);
		break;
	}
}
