/**
 * @file emulator.h
 * @brief High-level emulator harness.
 *
 * The emulator owns a @ref soc_t and drives it with a fetch-decode-execute
 * loop.  It handles policy decisions that are not part of the hardware spec:
 *  - Linux ABI syscall emulation
 *  - Flat binary loading
 *  - Halt and exit conditions
 *  - Optional per-instruction tracing
 *
 * This is one consumer of @ref soc_step.  A DPI co-simulation bridge is
 * another.  Neither knows about the other.
 */

#ifndef RVE_EMULATOR_H
#define RVE_EMULATOR_H

#include "rve/soc.h"
#include "rve/hart.h"
#include "rve/types.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Syscall handler callback.
 *
 * Called when the hart retires an ECALL instruction.
 *
 * @param soc       The SoC whose hart issued the call.
 * @param retire    Retirement record for the ECALL instruction.
 * @param userdata  Caller-supplied context pointer.
 * @return          true  -> syscall handled, execution continues.@n
 *                  false -> unhandled; emulator halts with an error.
 */
typedef bool (*syscall_handler_t)(soc_t *soc, hart_retire_t *retire, void *userdata);

/**
 * @brief Trace callback.
 *
 * Called after every retired instruction, before the next fetch.
 * Use for disassembly output, coverage instrumentation, or co-simulation.
 *
 * @param retire    Retirement record of the completed instruction.
 * @param userdata  Caller-supplied context pointer.
 */
typedef void (*trace_fn_t)(const hart_retire_t *retire, void *userdata);

/**
 * @brief Emulator state.
 */
typedef struct {
    soc_t soc; /**< The simulated SoC. */

    /* Callbacks -- both optional; NULL disables the feature */
    syscall_handler_t syscall_handler;  /**< Called on ECALL retirement.          */
    void             *syscall_userdata; /**< Context forwarded to the handler.    */
    trace_fn_t        trace_fn;         /**< Called after every instruction.      */
    void             *trace_userdata;   /**< Context forwarded to the trace fn.   */

    /* Runtime state */
    bool     halted;      /**< True once a halt condition has been reached.       */
    int      exit_code;   /**< Program exit code (valid only when halted).        */
    uint64_t cycle_count; /**< Total instructions retired since emulator_init().  */
} emulator_t;

/**
 * @brief Initialise the emulator and its SoC.
 *
 * Attaches the default memory map and peripherals (RAM, TOHOST, UART).
 * The built-in Linux ABI syscall handler is installed automatically.
 *
 * @param e         Emulator to initialise.
 * @param reset_pc  Program counter value at reset.
 */
void emulator_init(emulator_t *e, word reset_pc);

/**
 * @brief Load a flat binary file into the SoC's address space.
 *
 * @param e          Emulator.
 * @param path       Path to the binary file.
 * @param load_addr  Destination address.
 * @return           0 on success, -1 on error.
 */
int emulator_load_binary(emulator_t *e, const char *path, word load_addr);

/**
 * @brief Replace the built-in syscall handler.
 *
 * @param e         Emulator.
 * @param fn        New handler (or NULL to disable syscall handling).
 * @param userdata  Context pointer forwarded to @p fn.
 */
void emulator_set_syscall_handler(emulator_t *e,
                                  syscall_handler_t fn, void *userdata);

/**
 * @brief Install a trace callback.
 *
 * @param e         Emulator.
 * @param fn        Trace function (or NULL to disable tracing).
 * @param userdata  Context pointer forwarded to @p fn.
 */
void emulator_set_trace(emulator_t *e, trace_fn_t fn, void *userdata);

/**
 * @brief Run until a halt condition is reached.
 *
 * Loops calling @ref emulator_step until the emulator is halted by an
 * ECALL exit, an unhandled trap, or @p max_steps being exceeded.
 *
 * @param e          Emulator.
 * @param max_steps  Maximum instruction count; pass 0 for unlimited.
 * @return           Exit code (0 on clean exit; non-zero on error or trap).
 */
int emulator_run(emulator_t *e, uint64_t max_steps);

/**
 * @brief Execute exactly one instruction.
 *
 * Returns immediately without looping.  If the emulator is already halted
 * a dummy retirement record with TRAP_ILLEGAL_INSTR is returned.
 *
 * @param e  Emulator.
 * @return   Retirement record for the completed instruction.
 */
hart_retire_t emulator_step(emulator_t *e);

#endif /* RVE_EMULATOR_H */
