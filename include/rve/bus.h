/**
 * @file bus.h
 * @brief Address-mapped system bus.
 *
 * The bus holds an array of @ref peripheral_t slots and dispatches
 * reads/writes to whichever peripheral owns the accessed address.
 * Overlapping address ranges are not permitted; on a conflict the
 * peripheral with the smallest range wins.
 */

#ifndef RVE_BUS_H
#define RVE_BUS_H

#include "rve/peripherals/peripheral.h"
#include "rve/types.h"

#include <stdlib.h>

/** @brief Maximum number of peripherals that can be attached to the bus. */
#define BUS_MAX_PERIPHERALS 10

/**
 * @brief System bus state.
 *
 * Do not access fields directly; use the bus_* API.
 */
struct bus_t {
    peripheral_t peripherals[BUS_MAX_PERIPHERALS]; /**< Peripheral slot array.       */
    size_t       count;                            /**< Number of active peripherals. */
};

typedef struct bus_t bus_t;

/**
 * @brief Initialise the bus, marking all peripheral slots inactive.
 * @param bus  Bus to initialise.
 */
void bus_init(bus_t *bus);

/**
 * @brief Register a peripheral on the bus.
 *
 * @param bus        Target bus.
 * @param addr_start First address in the peripheral's range (inclusive).
 * @param addr_end   First address beyond the peripheral's range (exclusive).
 * @param ctx        Peripheral-private context pointer.
 * @param ops        Pointer to the peripheral's operation table.
 * @param name       Human-readable name used in debug messages.
 * @return           true on success; false on slot exhaustion or overlap.
 */
bool bus_add_peripheral(bus_t *bus, word addr_start, word addr_end,
                        void *ctx, peripheral_ops_t *ops, const char *name);

/**
 * @brief Find the peripheral that owns @p addr.
 *
 * When multiple peripherals overlap the same address the one with the
 * smallest address range is preferred (most-specific match).
 *
 * @param bus   Bus to search.
 * @param addr  Address to look up.
 * @return      Pointer to the matching peripheral, or NULL.
 */
peripheral_t *bus_get_peripheral(bus_t *bus, word addr);

/**
 * @brief Dispatch a write to the peripheral at @p addr.
 *
 * @param bus       Bus.
 * @param addr      Target address.
 * @param data      Value to write.
 * @param size_bits Transfer width in bits (8, 16, or 32).
 * @return          true on success; false if no peripheral or write failed.
 */
bool bus_write(bus_t *bus, word addr, word data, u8 size_bits);

/**
 * @brief Dispatch a read from the peripheral at @p addr.
 *
 * @param bus       Bus.
 * @param addr      Source address.
 * @param size_bits Transfer width in bits (8, 16, or 32).
 * @param result    Receives the value read.
 * @return          true on success; false if no peripheral or read failed.
 */
bool bus_read(bus_t *bus, word addr, u8 size_bits, word *result);

#endif /* RVE_BUS_H */
