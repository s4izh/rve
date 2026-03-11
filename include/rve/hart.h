/**
 * @file hart.h
 * @brief RVE hardware thread (hart) -- state, retirement record, and public API.
 *
 * A hart (hardware thread) is the fundamental execution unit of a RISC-V core.
 * This module owns:
 *  - The integer register file and machine-mode CSR file (@ref hart_t)
 *  - The per-instruction retirement record (@ref hart_retire_t)
 *  - The single-step execution entry point (@ref hart_step)
 */

#ifndef RVE_HART_H
#define RVE_HART_H

#include "rve/bus.h"
#include "rve/decoder.h"
#include "rve/types.h"

#define XLEN      32           /**< Register width in bits.                   */
#define XLEN_MASK 0xFFFFFFFF   /**< Bit mask for a full XLEN-wide value.      */

/**
 * @brief Machine-mode CSR (Control and Status Register) file.
 *
 * Only CSRs actually exercised by this implementation are stored here.
 * All other CSR addresses are either read-only zero or silently ignored.
 */
typedef struct {
    word mstatus;   /**< Machine status register (mstatus).               */
    word mtvec;     /**< Machine trap-handler base address (mtvec).        */
    word mepc;      /**< Machine exception program counter (mepc).         */
    word mcause;    /**< Machine trap cause (mcause).                      */
    word mtval;     /**< Machine bad address or instruction (mtval).       */
    word mscratch;  /**< Scratch register for trap handlers (mscratch).    */
} csr_file_t;

/**
 * @brief Trap cause codes written to mcause.
 *
 * Synchronous exceptions have bit 31 = 0.  For interrupts, bit 31 is set
 * automatically by @ref hart_take_trap; the enumerators below carry the
 * raw cause index without that bit.
 */
typedef enum {
    TRAP_NONE               = -1,  /**< Sentinel: no trap pending.                     */

    /* Synchronous exceptions */
    TRAP_INSTR_MISALIGNED   =  0,  /**< Instruction address misaligned.                */
    TRAP_INSTR_ACCESS_FAULT =  1,  /**< Instruction access fault.                      */
    TRAP_ILLEGAL_INSTR      =  2,  /**< Illegal instruction.                           */
    TRAP_LOAD_MISALIGNED    =  4,  /**< Load address misaligned.                       */
    TRAP_LOAD_ACCESS_FAULT  =  5,  /**< Load access fault.                             */
    TRAP_STORE_MISALIGNED   =  6,  /**< Store/AMO address misaligned.                  */
    TRAP_STORE_ACCESS_FAULT =  7,  /**< Store/AMO access fault.                        */
    TRAP_ECALL_U            =  8,  /**< Environment call from U-mode.                  */
    TRAP_ECALL_M            = 11,  /**< Environment call from M-mode.                  */

    /* Asynchronous interrupts -- mcause bit 31 is set by hart_take_trap */
    TRAP_INT_SW             =  3,  /**< Machine software interrupt.                    */
    TRAP_INT_TIMER          =  7,  /**< Machine timer interrupt.                       */
    TRAP_INT_EXTERNAL       = 11,  /**< Machine external interrupt.                    */
} trap_t;

/** @brief Evaluates to non-zero when @p c is an asynchronous interrupt cause. */
#define TRAP_IS_INTERRUPT(c) \
    ((c) == TRAP_INT_SW || (c) == TRAP_INT_TIMER || (c) == TRAP_INT_EXTERNAL)

/**
 * @brief Retirement record for a single instruction.
 *
 * Completely describes the observable effects of one instruction execution.
 * This is the primary output of @ref hart_step and the struct consumed by
 * co-simulation, tracing, and any other analysis layer -- not raw CPU state.
 *
 * At most one memory read and one memory write are produced per RV32I
 * instruction.
 */
typedef struct {
    word                  pc;              /**< PC of the retired instruction.               */
    word                  next_pc;         /**< PC of the next instruction to execute.       */
    word                  instruction;     /**< Raw 32-bit instruction word.                 */
    decoded_instruction_t di;              /**< Decoded instruction fields.                  */

    bool                  rd_written;      /**< True when a destination register was written.*/
    reg_t                 rd;              /**< Destination register index (if rd_written).  */
    word                  rd_value;        /**< Value written to rd (if rd_written).          */

    bool                  mem_read;        /**< True when a memory read was performed.       */
    word                  mem_read_addr;   /**< Effective address of the load.               */
    word                  mem_read_value;  /**< Value returned by the load.                  */
    u8                    mem_read_size;   /**< Transfer width in bits: 8, 16, or 32.        */

    bool                  mem_write;       /**< True when a memory write was performed.      */
    word                  mem_write_addr;  /**< Effective address of the store.              */
    word                  mem_write_value; /**< Value written to memory.                     */
    u8                    mem_write_size;  /**< Transfer width in bits: 8, 16, or 32.        */

    trap_t                trap;            /**< TRAP_NONE on clean retirement; cause otherwise.*/
} hart_retire_t;

/**
 * @brief Hardware thread (hart) state.
 */
typedef struct {
    word        gpr[XLEN]; /**< Integer register file x0..x31. x0 is always zero. */
    csr_file_t  csr;       /**< Machine-mode CSR file.                             */
    word        pc;        /**< Program counter.                                   */
    bus_t      *bus;       /**< System bus -- not owned; must outlive the hart.     */
} hart_t;

/* ---------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------ */

/**
 * @brief Initialise a hart to a known-good state.
 *
 * Zeroes the register file and all CSRs, then attaches the bus and sets
 * the reset PC.
 *
 * @param hart  Hart to initialise.
 * @param bus   System bus; must outlive @p hart.
 * @param pc    Reset program counter.
 */
void hart_init(hart_t *hart, bus_t *bus, word pc);

/**
 * @brief Execute one instruction.
 *
 * Fetches, decodes, and executes the instruction at the current PC.
 * The returned @ref hart_retire_t is always fully populated.
 *
 * On a synchronous fault or interrupt the hart's PC is already redirected
 * to the trap vector before this function returns; @c retire.trap carries
 * the cause.  The caller (SoC / emulator / DPI bridge) decides what to
 * do with the trap signal.
 *
 * @param hart  Hart to step.
 * @return      Retirement record describing the completed instruction.
 */
hart_retire_t hart_step(hart_t *hart);

/**
 * @brief Inject a trap (exception or interrupt) directly into the hart.
 *
 * Commits all CSR side-effects (mepc, mcause, mtval, mstatus) and
 * redirects the PC to mtvec.  Designed for the SoC to deliver external
 * interrupts between normal instruction steps.
 *
 * @param hart   Target hart.
 * @param cause  Trap cause code.
 * @param tval   Value for mtval (faulting address, instruction, or 0).
 */
void hart_take_trap(hart_t *hart, trap_t cause, word tval);

/**
 * @brief Dump the register file and CSRs to stdout.
 *
 * @param hart   Hart to print.
 * @param title  Optional heading string; may be NULL.
 */
void hart_print(const hart_t *hart, const char *title);

/**
 * @brief Convert a trap cause to a human-readable C string.
 *
 * @param trap  Trap cause.
 * @return      NUL-terminated string constant; never NULL.
 */
const char *rve_trap_to_cstr(trap_t trap);

#endif /* RVE_HART_H */
