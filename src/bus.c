/**
 * @file bus.c
 * @brief Address-mapped system bus implementation.
 */

#include "rve/bus.h"
#include "rve/peripherals/peripheral.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void bus_init(bus_t *bus)
{
	assert(bus != NULL);
	for (size_t i = 0; i < BUS_MAX_PERIPHERALS; ++i) {
		bus->peripherals[i].active = false;
	}
	bus->count = 0;
}

/**
 * @brief Return the index of the first inactive slot on the bus.
 *
 * @param bus        Bus to search.
 * @param addr_start Unused; kept for a uniform signature with potential
 *                   future overlap-checking variants.
 * @param addr_end   Unused; see @p addr_start.
 * @param name       Unused; see @p addr_start.
 * @return           Slot index on success, -1 if the bus is full.
 */
static int bus_find_slot(bus_t *bus, word addr_start, word addr_end, const char *name)
{
    (void)addr_start; (void)addr_end; (void)name;
    for (int i = 0; i < BUS_MAX_PERIPHERALS; ++i) {
        if (!bus->peripherals[i].active) return i;
    }
    return -1;
}

bool bus_add_peripheral(bus_t *bus, word addr_start, word addr_end, void *ctx, peripheral_ops_t *ops, const char *name)
{
	int slot = bus_find_slot(bus, addr_start, addr_end, name);

	if (slot < 0) {
        fprintf(stderr, "BUS ERROR: Cannot register '%s', maximum number of peripherals (%d) reached.\n", name, BUS_MAX_PERIPHERALS);
		return false;
	}

	peripheral_t *peripheral = &bus->peripherals[slot];

	peripheral->active     = true;
	peripheral->addr_start = addr_start;
	peripheral->addr_end   = addr_end;
	strncpy(peripheral->name, name, PERIPHERAL_NAME_MAX - 1);
	peripheral->name[PERIPHERAL_NAME_MAX - 1] = '\0';

	peripheral->ctx  = ctx;
	peripheral->ops  = ops;

	bus->count++;
	return true;
}

/**
 * @brief Find the peripheral that best covers @p addr.
 *
 * "Best" is defined as the active peripheral whose address range contains
 * @p addr and whose range is smallest (most specific match first).
 */
peripheral_t *bus_get_peripheral(bus_t *bus, word addr)
{
    peripheral_t *best_match   = NULL;
    word          smallest_range = 0xFFFFFFFF;

    for (size_t i = 0; i < BUS_MAX_PERIPHERALS; ++i) {
        peripheral_t *p = &bus->peripherals[i];
        if (p->active && addr >= p->addr_start && addr < p->addr_end) {
            word range = p->addr_end - p->addr_start;
            if (best_match == NULL || range < smallest_range) {
                best_match    = p;
                smallest_range = range;
            }
        }
    }
    return best_match;
}

bool bus_read(bus_t *bus, word addr, u8 size_bits, word *result)
{
	peripheral_t *peripheral = bus_get_peripheral(bus, addr);

	if (peripheral == NULL) {
		fprintf(stderr, "BUS ERROR: Cannot read from address 0x%x, no peripheral found.\n", addr);
		return false;
	}

	if (!peripheral->ops->read) {
		fprintf(stderr, "BUS ERROR: Peripheral '%s' does not support read operation.\n", peripheral->name);
		return false;
	}

	if (!peripheral->ops->read(peripheral->ctx, addr, size_bits, result)) { // Pass size_bits
		fprintf(stderr, "BUS ERROR: Failed to read from peripheral '%s' at address 0x%x.\n", peripheral->name, addr);
		return false;
	}
	return true;
}

bool bus_write(bus_t *bus, word addr, word data, u8 size_bits)
{
	peripheral_t *peripheral = bus_get_peripheral(bus, addr);

	if (peripheral == NULL) {
		fprintf(stderr, "BUS ERROR: Cannot write to address 0x%x, no peripheral found.\n", addr);
		return false;
	}

	if (!peripheral->ops->write) {
		fprintf(stderr, "BUS ERROR: Peripheral '%s' does not support write operation.\n", peripheral->name);
		return false;
	}

	if (!peripheral->ops->write(peripheral->ctx, addr, data, size_bits)) { // Pass size_bits
		fprintf(stderr, "BUS ERROR: Failed to write to peripheral '%s' at address 0x%x.\n", peripheral->name, addr);
		return false;
	}
	return true;
}
