/**
 * @file soc.h
 * @brief System-on-Chip: one hart, one bus, and attached peripherals.
 *
 * The SoC owns all hardware state.  It knows nothing about how it is
 * driven -- that is the emulator's (or DPI bridge's) concern.
 *
 * Typical usage:
 * @code
 *   soc_t soc;
 *   soc_init(&soc, 0x00000000);
 *   soc_add_memory(&soc, 0x00000000, 4 * 1024 * 1024);
 *   soc_load_binary(&soc, data, size, 0x00000000);
 *   hart_retire_t r = soc_step(&soc);
 * @endcode
 */

#ifndef RVE_SOC_H
#define RVE_SOC_H

#include "rve/hart.h"
#include "rve/bus.h"
#include "rve/peripherals/peripheral.h"
#include "rve/peripherals/memory.h"

/**
 * @brief Top-level SoC state container.
 */
typedef struct {
    hart_t hart; /**< The single hardware execution thread.           */
    bus_t  bus;  /**< Address-mapped bus connecting all peripherals.  */
} soc_t;

/**
 * @brief Initialise the SoC.
 *
 * Zeroes all state, initialises the bus and hart.  No peripherals are
 * attached; call @ref soc_add_memory or @ref soc_add_peripheral next.
 *
 * @param soc       SoC to initialise.
 * @param reset_pc  Program counter value at reset.
 */
void soc_init(soc_t *soc, word reset_pc);

/**
 * @brief Register a peripheral on the bus.
 *
 * Thin wrapper around @ref bus_add_peripheral; saves callers from
 * including bus.h directly.
 *
 * @param soc        Target SoC.
 * @param addr_start First address in the peripheral's range (inclusive).
 * @param addr_end   First address beyond the range (exclusive).
 * @param ctx        Peripheral context pointer.
 * @param ops        Peripheral operations table.
 * @param name       Human-readable name for debug messages.
 * @return           true on success; false on slot exhaustion or overlap.
 */
bool soc_add_peripheral(soc_t *soc,
                        word addr_start, word addr_end,
                        void *ctx, peripheral_ops_t *ops,
                        const char *name);

/**
 * @brief Allocate and attach a memory peripheral.
 *
 * Creates a @ref memory_t context, calls @ref memory_init, and registers
 * it on the bus.  Most callers will invoke this once after @ref soc_init.
 *
 * @param soc        Target SoC.
 * @param addr_start Base address for the memory region.
 * @param size       Size in bytes.
 * @return           true on success; false on allocation or bus error.
 */
bool soc_add_memory(soc_t *soc, word addr_start, size_t size);

/**
 * @brief Copy a flat binary image into memory.
 *
 * Looks up the peripheral that owns @p load_addr and calls
 * @ref memory_load_file on its context.  The target range must have a
 * memory peripheral already registered.
 *
 * @param soc        Target SoC.
 * @param data       Binary data to load.
 * @param size       Number of bytes to copy.
 * @param load_addr  Destination address in the SoC's address space.
 * @return           0 on success, -1 on error.
 */
int soc_load_binary(soc_t *soc, const char *data, size_t size, word load_addr);

/**
 * @brief Execute one instruction.
 *
 * Delegates to @ref hart_step.  This is the only way to advance
 * simulation time.
 *
 * @param soc  SoC to step.
 * @return     Retirement record describing the completed instruction.
 */
hart_retire_t soc_step(soc_t *soc);

/**
 * @brief Inject an asynchronous interrupt into the hart.
 *
 * Takes effect at the start of the next @ref soc_step call.
 * Calling with a non-interrupt trap cause logs a warning and returns.
 *
 * @param soc       Target SoC.
 * @param interrupt Interrupt cause (must satisfy TRAP_IS_INTERRUPT).
 */
void soc_raise_interrupt(soc_t *soc, trap_t interrupt);

#endif /* RVE_SOC_H */
