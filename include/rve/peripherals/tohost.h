/**
 * @file tohost.h
 * @brief TOHOST peripheral -- riscv-tests exit/result interface.
 *
 * The riscv-tests test suite writes to a magic symbol @c tohost to signal
 * pass/fail.  This peripheral maps that write to an emulator halt:
 *  - data == 1  -> test passed (exit_code = 0)
 *  - data  > 1  -> test failed at case (data >> 1)
 */

#ifndef RVE_TOHOST_H
#define RVE_TOHOST_H

#include "rve/types.h"
#include "rve/peripherals/peripheral.h"

/**
 * @brief Context for the TOHOST peripheral.
 *
 * Both pointers must point into the owning emulator's state and must
 * remain valid for the lifetime of the peripheral.
 */
typedef struct {
    bool *halt_flag; /**< Set to true when the test writes to TOHOST. */
    int  *exit_code; /**< Receives 0 (pass) or the failing case number.*/
} peripheral_tohost_t;

/** @brief Peripheral operations table for TOHOST. */
extern peripheral_ops_t peripheral_tohost_ops;

#endif /* RVE_TOHOST_H */
