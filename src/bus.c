#include "rve/bus.h"
#include "rve/peripheral.h"
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

static int bus_find_slot_and_check_overlap(bus_t *bus, u64 addr_start, u64 addr_end, const char* name)
{
    int first_inactive_slot = -1;

    if (addr_end <= addr_start) {
        fprintf(stderr, "BUS ERROR: Cannot register '%s', invalid address range 0x%lx - 0x%lx.\n", name, addr_start, addr_end);
        return -1;
    }

    for (int i = 0; i < BUS_MAX_PERIPHERALS; ++i) {
        peripheral_t *existing = &bus->peripherals[i];
        if (existing->active) {
            // Check for overlap: (StartA < EndB) and (EndA > StartB)
            if (addr_start < existing->addr_end && addr_end > existing->addr_start) {
                fprintf(stderr, "BUS ERROR: Address range overlap for '%s' (0x%lx-0x%lx)!\n", name, addr_start, addr_end);
                fprintf(stderr, "  Conflicts with existing: '%s' (0x%lx-0x%lx)\n", existing->name, existing->addr_start, existing->addr_end);
                return -1; // Overlap detected
            }
        } else {
            // If this slot is inactive and we haven't found one yet, mark it.
            if (first_inactive_slot == -1) {
                first_inactive_slot = i;
            }
        }
    }

    // After checking all slots for overlaps...
    if (first_inactive_slot != -1) {
        // Found an inactive slot, return it.
        return first_inactive_slot;
    } else {
        // No inactive slots found. Check if the bus is actually full.
        // This condition should ideally be equivalent to bus->count >= BUS_MAX_PERIPHERALS
        // if bus->count is maintained correctly.
        fprintf(stderr, "BUS ERROR: Cannot register '%s', maximum number of peripherals (%d) reached.\n", name, BUS_MAX_PERIPHERALS);
        return -1; // Bus is full
    }
}

bool bus_add_peripheral(bus_t *bus, u64 addr_start, u64 addr_end, void* ctx, peripheral_ops_t* ops, const char* name)
{
	int slot = bus_find_slot_and_check_overlap(bus, addr_start, addr_end, name);

	if (slot < 0) {
		// Error message already printed by bus_find_slot_and_check_overlap
		return false;
	}

	// We have a valid inactive slot
	peripheral_t *peripheral = &bus->peripherals[slot];

	peripheral->active = true;
	peripheral->addr_start = addr_start;
	peripheral->addr_end = addr_end;
	strncpy(peripheral->name, name, PERIPHERAL_NAME_MAX - 1);
	peripheral->name[PERIPHERAL_NAME_MAX - 1] = '\0'; // Ensure null termination

	peripheral->ctx = ctx;
	peripheral->ops = ops;

	bus->count++; // Increment count of active peripherals
	return true;
}

peripheral_t *bus_get_peripheral(bus_t *bus, u64 addr)
{
	// Iterate through all possible slots, checking active ones
	for (size_t i = 0; i < BUS_MAX_PERIPHERALS; ++i) {
		peripheral_t *peripheral = &bus->peripherals[i];
		// Check if the peripheral is active and the address falls within its range
		if (peripheral->active && addr >= peripheral->addr_start && addr < peripheral->addr_end) {
			return peripheral;
		}
	}
	// No peripheral found for this address
	return NULL;
}

bool bus_read(bus_t *bus, u64 addr, u8 size_bits, u64* result) // Use size_bits
{
	peripheral_t *peripheral = bus_get_peripheral(bus, addr);

	if (peripheral == NULL) {
		fprintf(stderr, "BUS ERROR: Cannot read from address 0x%lx, no peripheral found.\n", addr);
		return false;
	}

	if (!peripheral->ops->read) {
		fprintf(stderr, "BUS ERROR: Peripheral '%s' does not support read operation.\n", peripheral->name);
		return false;
	}

	if (!peripheral->ops->read(peripheral->ctx, addr, size_bits, result)) { // Pass size_bits
		fprintf(stderr, "BUS ERROR: Failed to read from peripheral '%s' at address 0x%lx.\n", peripheral->name, addr);
		return false;
	}
	return true;
}

bool bus_write(bus_t *bus, u64 addr, u64 data, u8 size_bits) // Use size_bits
{
	peripheral_t *peripheral = bus_get_peripheral(bus, addr);

	if (peripheral == NULL) {
		fprintf(stderr, "BUS ERROR: Cannot write to address 0x%lx, no peripheral found.\n", addr);
		return false;
	}

	if (!peripheral->ops->write) {
		fprintf(stderr, "BUS ERROR: Peripheral '%s' does not support write operation.\n", peripheral->name);
		return false;
	}

	if (!peripheral->ops->write(peripheral->ctx, addr, data, size_bits)) { // Pass size_bits
		fprintf(stderr, "BUS ERROR: Failed to write to peripheral '%s' at address 0x%lx.\n", peripheral->name, addr);
		return false;
	}
	return true;
}
