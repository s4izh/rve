/**
 * @file hart.c
 * @brief RISC-V hart execution engine.
 *
 * Implements single-step execution (@ref hart_step), trap handling,
 * and the machine-mode CSR file.  The execute stage is organised as a
 * flat switch over decoded opcodes; memory accesses and branch logic are
 * factored into small static helper functions to avoid macro abuse.
 */

#include "rve/hart.h"
#include "rve/bus.h"
#include "rve/decoder.h"
#include "rve/types.h"

#include <stdio.h>
#include <string.h>

/**
 * @brief Sign-extend @p value from @p bits up to 32 bits.
 *
 * @param value  Unsigned value to extend.
 * @param bits   Current width of @p value (e.g., 8, 12, 13, 16, 21).
 * @return       @p value sign-extended to 32 bits.
 */
static inline word sext(word value, u32 bits)
{
    return (word)((s_word)(value << (32u - bits)) >> (32u - bits));
}

/**
 * @brief Select the appropriate mtval for a given trap cause.
 *
 * Per the privileged spec, different trap causes populate mtval differently:
 * load/store faults use the faulting address, illegal-instruction traps use
 * the instruction word, misaligned jump/branch traps use the target address.
 *
 * @param cause        Trap cause code.
 * @param pc           Current PC (unused; kept for call-site symmetry).
 * @param instruction  Raw instruction word.
 * @param mem_addr     Effective load/store address.
 * @param bad_pc       Misaligned branch/jump target address.
 * @return             Value to write into mtval.
 */
static word trap_tval(trap_t cause, word pc, word instruction, word mem_addr, word bad_pc)
{
    (void)pc;  /* unused: pc is reserved for future use (e.g. security extensions) */
    switch (cause) {
        case TRAP_INSTR_MISALIGNED:
        case TRAP_INSTR_ACCESS_FAULT:  return bad_pc;
        case TRAP_LOAD_MISALIGNED:
        case TRAP_LOAD_ACCESS_FAULT:
        case TRAP_STORE_MISALIGNED:
        case TRAP_STORE_ACCESS_FAULT:  return mem_addr;
        case TRAP_ILLEGAL_INSTR:       return instruction;
        default:                       return 0;
    }
}

/**
 * @brief Commit a trap: update all CSRs and return the new PC.
 *
 * Saves the current PC to mepc, writes tval and mcause, preserves MIE into
 * MPIE, clears MIE, sets MPP=3 (M-mode), then resolves the trap vector:
 *  - Direct mode (mtvec[1:0] == 0): always jumps to mtvec base.
 *  - Vectored mode (mtvec[1:0] == 1): base + 4*cause for interrupts,
 *    base for exceptions.
 *
 * @param hart   Hart whose CSRs are updated.
 * @param cause  Trap cause.
 * @param tval   Value to write to mtval.
 * @return       New PC to load (mtvec target).
 */
static word commit_trap(hart_t *hart, trap_t cause, word tval)
{
    bool is_int = TRAP_IS_INTERRUPT(cause);

    hart->csr.mepc   = hart->pc;
    hart->csr.mtval  = tval;
    hart->csr.mcause = is_int ? (0x80000000u | (word)cause) : (word)cause;

    /* mstatus: save MIE into MPIE, clear MIE, set MPP=3 (M-mode) */
    word mie = (hart->csr.mstatus >> 3) & 1u;
    hart->csr.mstatus = (hart->csr.mstatus & ~0x88u) | (mie << 7);
    hart->csr.mstatus |= (3u << 11);  /* MPP = Machine mode */

    /* Resolve trap vector (mtvec modes: 0=direct, 1=vectored) */
    word base = hart->csr.mtvec & ~3u;
    word mode = hart->csr.mtvec &  3u;
    return (mode == 1u && is_int)
           ? base + ((word)cause & 0x7FFFFFFFu) * 4u
           : base;
}

// Public entry point for external interrupt injection (used by soc_raise_interrupt).
// Commits the trap and redirects PC immediately, takes effect before the next fetch.
void hart_take_trap(hart_t *hart, trap_t cause, word tval)
{
    hart->pc = commit_trap(hart, cause, tval);
}

// ---------------------------------------------------------------------------
// CSR operations
// ---------------------------------------------------------------------------

typedef enum { CSR_RW,  /**< Atomic read/write. */
               CSR_RS,  /**< Atomic read and set bits. */
               CSR_RC   /**< Atomic read and clear bits. */
} csr_op_t;

/**
 * @brief Perform one CSR read-modify-write operation on a single register.
 *
 * Implements the atomic semantics shared by CSRRW/CSRRS/CSRRC and their
 * immediate variants.
 *
 * @param hart      Hart state.
 * @param di        Decoded instruction (rd and rs1 indices).
 * @param reg       Pointer to the target CSR storage word.
 * @param op        Operation: write, set bits, or clear bits.
 * @param use_uimm  If true, treat di->rs1 as a 5-bit unsigned immediate.
 * @return          TRAP_NONE, or TRAP_ILLEGAL_INSTR on unknown op.
 */
static trap_t csr_access(hart_t *hart, const decoded_instruction_t *di,
                         uint32_t *reg, csr_op_t op, bool use_uimm)
{
    uint32_t operand = use_uimm ? (uint32_t)di->rs1 : hart->gpr[di->rs1];
    uint32_t old_val = *reg;
    uint32_t new_val;
    switch (op) {
        case CSR_RW: new_val = operand;             break;
        case CSR_RS: new_val = old_val |  operand;  break;
        case CSR_RC: new_val = old_val & ~operand;  break;
        default:     return TRAP_ILLEGAL_INSTR;
    }
    if (di->rd  != 0) hart->gpr[di->rd] = old_val;
    if (di->rs1 != 0) *reg = new_val;
    return TRAP_NONE;
}

/**
 * @brief Decode a CSR address and dispatch to the correct register.
 *
 * Handles read-only identity registers, stub registers (silently
 * ignored), and the live CSRs in @ref csr_file_t.  Unimplemented
 * addresses generate an illegal-instruction trap.
 *
 * @param hart      Hart state.
 * @param di        Decoded instruction.
 * @param op        CSR operation type.
 * @param use_uimm  True for *I variants (rs1 field is a 5-bit immediate).
 * @return          TRAP_NONE, or TRAP_ILLEGAL_INSTR on unknown address.
 */
static trap_t csr_dispatch(hart_t *hart, const decoded_instruction_t *di,
                            csr_op_t op, bool use_uimm)
{
    uint32_t addr = di->original_instruction >> 20;
    switch (addr) {
        // Read-only identity registers
        case 0xF11:case 0xF12: case 0xF13: case 0xF14:
            if (di->rd != 0) hart->gpr[di->rd] = 0;
            return TRAP_NONE;

        case 0x301: // misa - RV32IM
            if (di->rd != 0) hart->gpr[di->rd] = 0x40001100u;
            return TRAP_NONE;

        // Stubs (reads return 0, writes ignored)
        case 0x302: case 0x303: case 0x304: // medeleg, mideleg, mie
        case 0x344:                          // mip
        case 0x180:                          // satp
        case 0x744: case 0x747:              // menvcfg, mseccfg
        case 0x3A0: case 0x3A1: case 0x3A2: case 0x3A3: // pmpcfg
        case 0x3B0: case 0x3B1: case 0x3B2: case 0x3B3: // pmpaddr
            if (di->rd != 0) hart->gpr[di->rd] = 0;
            return TRAP_NONE;

        case 0x300: return csr_access(hart, di, &hart->csr.mstatus,  op, use_uimm);
        case 0x305: return csr_access(hart, di, &hart->csr.mtvec,    op, use_uimm);
        case 0x340: return csr_access(hart, di, &hart->csr.mscratch, op, use_uimm);
        case 0x341: return csr_access(hart, di, &hart->csr.mepc,     op, use_uimm);
        case 0x342: return csr_access(hart, di, &hart->csr.mcause,   op, use_uimm);
        case 0x343: return csr_access(hart, di, &hart->csr.mtval,    op, use_uimm);

        default:
            fprintf(stderr, "HART: unimplemented CSR 0x%03X at PC 0x%08X\n",
                    addr, hart->pc);
            return TRAP_ILLEGAL_INSTR;
    }
}

/* ---------------------------------------------------------------------------
 * Execute-stage helpers
 *
 * Replacing statement macros with typed inline functions eliminates the
 * implicit variable captures and makes tool-chain analysis (sanitisers,
 * static analysers) far more effective.
 * ------------------------------------------------------------------------ */

/**
 * @brief Execute a B-type (conditional branch) instruction.
 *
 * Evaluates @p cond and, when true, decodes the branch offset from the
 * instruction immediate.  Checks for 4-byte alignment on the resolved
 * target address.
 *
 * @param hart     Hart state (read-only: pc).
 * @param di       Decoded instruction supplying the branch immediate.
 * @param cond     True when the branch should be taken.
 * @param next_pc  Updated to the resolved next PC on success.
 * @param bad_pc   Set to the misaligned target when a trap is raised.
 * @return         TRAP_NONE on success, TRAP_INSTR_MISALIGNED on bad target.
 */
static inline trap_t exec_branch(const hart_t *hart, const decoded_instruction_t *di,
                                  bool cond, word *next_pc, word *bad_pc)
{
    word target = hart->pc + sext(di->imm, 13);
    if (cond && (target & 0x3u)) {
        *bad_pc = target;
        return TRAP_INSTR_MISALIGNED;
    }
    *next_pc = cond ? target : hart->pc + 4;
    return TRAP_NONE;
}

/**
 * @brief Execute an I-type load instruction.
 *
 * Computes the effective address (rs1 + sext(imm, 12)), reads @p size_bits
 * from the bus, optionally sign-extends the result, and fills in the
 * memory-read fields of the retirement record.
 *
 * Misaligned accesses are not faulted here; the execution environment is
 * expected to support transparent misaligned loads (Vol I, Sec.2.6).
 *
 * @param hart          Hart state.
 * @param di            Decoded instruction.
 * @param size_bits     Transfer width: 8, 16, or 32.
 * @param sign_extend   True for LB/LH (signed); false for LBU/LHU.
 * @param mem_addr_out  Receives the effective address (used for trap tval).
 * @param r             Retirement record updated with load metadata.
 * @return              TRAP_NONE, or TRAP_LOAD_ACCESS_FAULT on bus error.
 */
static inline trap_t exec_load(hart_t *hart, const decoded_instruction_t *di,
                                u8 size_bits, bool sign_extend,
                                word *mem_addr_out, hart_retire_t *r)
{
    word addr = hart->gpr[di->rs1] + sext(di->imm, 12);
    word val  = 0;
    if (!bus_read(hart->bus, addr, size_bits, &val))
        return TRAP_LOAD_ACCESS_FAULT;
    hart->gpr[di->rd] = sign_extend ? sext(val, size_bits) : val;
    r->mem_read        = true;
    r->mem_read_addr   = addr;
    r->mem_read_value  = hart->gpr[di->rd];
    r->mem_read_size   = size_bits;
    *mem_addr_out      = addr;
    return TRAP_NONE;
}

/**
 * @brief Execute an S-type store instruction.
 *
 * Computes the effective address (rs1 + sext(imm, 12)), masks the source
 * register to @p size_bits, writes to the bus, and fills in the
 * memory-write fields of the retirement record.
 *
 * Misaligned accesses are not faulted here (see exec_load note above).
 *
 * @param hart          Hart state.
 * @param di            Decoded instruction.
 * @param size_bits     Transfer width: 8, 16, or 32.
 * @param mask          Bitmask applied to rs2 before writing (width guard).
 * @param mem_addr_out  Receives the effective address (used for trap tval).
 * @param r             Retirement record updated with store metadata.
 * @return              TRAP_NONE, or TRAP_STORE_ACCESS_FAULT on bus error.
 */
static inline trap_t exec_store(hart_t *hart, const decoded_instruction_t *di,
                                 u8 size_bits, word mask,
                                 word *mem_addr_out, hart_retire_t *r)
{
    word addr = hart->gpr[di->rs1] + sext(di->imm, 12);
    word val  = hart->gpr[di->rs2] & mask;
    if (!bus_write(hart->bus, addr, val, size_bits))
        return TRAP_STORE_ACCESS_FAULT;
    r->mem_write        = true;
    r->mem_write_addr   = addr;
    r->mem_write_value  = val;
    r->mem_write_size   = size_bits;
    *mem_addr_out       = addr;
    return TRAP_NONE;
}

/* ---------------------------------------------------------------------- */

/**
 * @brief Initialise a hart to a known-good state.
 * @see hart.h for full documentation.
 */
void hart_init(hart_t *hart, bus_t *bus, word pc)
{
    memset(hart, 0, sizeof(*hart));
    hart->bus = bus;
    hart->pc  = pc;
}

/**
 * @brief Execute one instruction.
 *
 * Linear execution flow:
 *  1. Fetch  -- may raise TRAP_INSTR_MISALIGNED / TRAP_INSTR_ACCESS_FAULT
 *  2. Decode -- may raise TRAP_ILLEGAL_INSTR
 *  3. Execute -- may raise any trap; always sets next_pc
 *  4. Commit -- apply trap CSR side-effects or write back clean results
 *
 * @see hart.h for the full API contract.
 */
hart_retire_t hart_step(hart_t *hart)
{
    hart_retire_t r;
    memset(&r, 0, sizeof(r));
    r.pc   = hart->pc;
    r.trap = TRAP_NONE;

    trap_t pending_trap = TRAP_NONE;
    word   next_pc      = hart->pc + 4;
    word   mem_addr     = 0;   // filled by load/store, used for trap tval
    word   bad_pc       = 0;   // filled by misaligned jumps

    decoded_instruction_t di;
    memset(&di, 0, sizeof(di));

    // ------------------------------------------------------------------
    // 1. Fetch
    // ------------------------------------------------------------------

    if (hart->pc & 0x3u) {
        bad_pc       = hart->pc;
        pending_trap = TRAP_INSTR_MISALIGNED;
        goto commit;
    }

    if (!bus_read(hart->bus, hart->pc, 32, &r.instruction)) {
        bad_pc       = hart->pc;
        pending_trap = TRAP_INSTR_ACCESS_FAULT;
        goto commit;
    }

    // ------------------------------------------------------------------
    // 2. Decode
    // ------------------------------------------------------------------

    di   = rve_decode_instruction(r.instruction);
    r.di = di;

    if (!di.valid) {
        pending_trap = TRAP_ILLEGAL_INSTR;
        goto commit;
    }

    // ------------------------------------------------------------------
    // 3. Execute
    // - Set next_pc for every control-flow change.
    // - Set pending_trap and goto commit on any fault.
    // - Never return early.
    // ------------------------------------------------------------------
    {
        dword wide = 0;

        switch (di.op) {

            // -- U-type --------------------------------------------------

            case INSTRUCTION_OP_LUI:
                hart->gpr[di.rd] = di.imm;
                break;

            case INSTRUCTION_OP_AUIPC:
                hart->gpr[di.rd] = hart->pc + di.imm;
                break;

            // -- J-type --------------------------------------------------

            case INSTRUCTION_OP_JAL: {
                word target = hart->pc + sext(di.imm, 21);
                if (target & 0x3u) { bad_pc = target; pending_trap = TRAP_INSTR_MISALIGNED; goto commit; }
                hart->gpr[di.rd] = hart->pc + 4;
                next_pc = target;
                break;
            }

            case INSTRUCTION_OP_JALR: {
                word target = (hart->gpr[di.rs1] + sext(di.imm, 12)) & ~1u;
                if (target & 0x3u) { bad_pc = target; pending_trap = TRAP_INSTR_MISALIGNED; goto commit; }
                hart->gpr[di.rd] = hart->pc + 4;
                next_pc = target;
                break;
            }

            // -- B-type --------------------------------------------------

            case INSTRUCTION_OP_BEQ:
                pending_trap = exec_branch(hart, &di, hart->gpr[di.rs1] == hart->gpr[di.rs2], &next_pc, &bad_pc);
                if (pending_trap != TRAP_NONE) goto commit;
                break;
            case INSTRUCTION_OP_BNE:
                pending_trap = exec_branch(hart, &di, hart->gpr[di.rs1] != hart->gpr[di.rs2], &next_pc, &bad_pc);
                if (pending_trap != TRAP_NONE) goto commit;
                break;
            case INSTRUCTION_OP_BLT:
                pending_trap = exec_branch(hart, &di, (s_word)hart->gpr[di.rs1] <  (s_word)hart->gpr[di.rs2], &next_pc, &bad_pc);
                if (pending_trap != TRAP_NONE) goto commit;
                break;
            case INSTRUCTION_OP_BGE:
                pending_trap = exec_branch(hart, &di, (s_word)hart->gpr[di.rs1] >= (s_word)hart->gpr[di.rs2], &next_pc, &bad_pc);
                if (pending_trap != TRAP_NONE) goto commit;
                break;
            case INSTRUCTION_OP_BLTU:
                pending_trap = exec_branch(hart, &di, hart->gpr[di.rs1] <  hart->gpr[di.rs2], &next_pc, &bad_pc);
                if (pending_trap != TRAP_NONE) goto commit;
                break;
            case INSTRUCTION_OP_BGEU:
                pending_trap = exec_branch(hart, &di, hart->gpr[di.rs1] >= hart->gpr[di.rs2], &next_pc, &bad_pc);
                if (pending_trap != TRAP_NONE) goto commit;
                break;

            // -- Loads ---------------------------------------------------

            case INSTRUCTION_OP_LB:
                pending_trap = exec_load(hart, &di,  8, true,  &mem_addr, &r);
                if (pending_trap != TRAP_NONE) goto commit;
                break;
            case INSTRUCTION_OP_LH:
                pending_trap = exec_load(hart, &di, 16, true,  &mem_addr, &r);
                if (pending_trap != TRAP_NONE) goto commit;
                break;
            case INSTRUCTION_OP_LW:
                pending_trap = exec_load(hart, &di, 32, true,  &mem_addr, &r);
                if (pending_trap != TRAP_NONE) goto commit;
                break;
            case INSTRUCTION_OP_LBU:
                pending_trap = exec_load(hart, &di,  8, false, &mem_addr, &r);
                if (pending_trap != TRAP_NONE) goto commit;
                break;
            case INSTRUCTION_OP_LHU:
                pending_trap = exec_load(hart, &di, 16, false, &mem_addr, &r);
                if (pending_trap != TRAP_NONE) goto commit;
                break;

            // -- Stores --------------------------------------------------

            case INSTRUCTION_OP_SB:
                pending_trap = exec_store(hart, &di,  8, 0x000000FFu, &mem_addr, &r);
                if (pending_trap != TRAP_NONE) goto commit;
                break;
            case INSTRUCTION_OP_SH:
                pending_trap = exec_store(hart, &di, 16, 0x0000FFFFu, &mem_addr, &r);
                if (pending_trap != TRAP_NONE) goto commit;
                break;
            case INSTRUCTION_OP_SW:
                pending_trap = exec_store(hart, &di, 32, 0xFFFFFFFFu, &mem_addr, &r);
                if (pending_trap != TRAP_NONE) goto commit;
                break;

            // -- Op-Immediate --------------------------------------------

            case INSTRUCTION_OP_ADDI:  hart->gpr[di.rd] = hart->gpr[di.rs1] + sext(di.imm, 12);                          break;
            case INSTRUCTION_OP_SLTI:  hart->gpr[di.rd] = (s_word)hart->gpr[di.rs1] <  (s_word)sext(di.imm, 12);         break;
            case INSTRUCTION_OP_SLTIU: hart->gpr[di.rd] = hart->gpr[di.rs1] <  (word)sext(di.imm, 12);                   break;
            case INSTRUCTION_OP_XORI:  hart->gpr[di.rd] = hart->gpr[di.rs1] ^  sext(di.imm, 12);                         break;
            case INSTRUCTION_OP_ORI:   hart->gpr[di.rd] = hart->gpr[di.rs1] |  sext(di.imm, 12);                         break;
            case INSTRUCTION_OP_ANDI:  hart->gpr[di.rd] = hart->gpr[di.rs1] &  sext(di.imm, 12);                         break;
            case INSTRUCTION_OP_SLLI:  hart->gpr[di.rd] = hart->gpr[di.rs1] << (di.imm & 0x1Fu);                         break;
            case INSTRUCTION_OP_SRLI:  hart->gpr[di.rd] = hart->gpr[di.rs1] >> (di.imm & 0x1Fu);                         break;
            case INSTRUCTION_OP_SRAI:  hart->gpr[di.rd] = (s_word)hart->gpr[di.rs1] >> (di.imm & 0x1Fu);                 break;

            // -- Op-Register ---------------------------------------------

            case INSTRUCTION_OP_ADD: wide = (dword)hart->gpr[di.rs1] + hart->gpr[di.rs2]; hart->gpr[di.rd] = (word)wide; break;
            case INSTRUCTION_OP_SUB: wide = (dword)hart->gpr[di.rs1] - hart->gpr[di.rs2]; hart->gpr[di.rd] = (word)wide; break;
            case INSTRUCTION_OP_SLL:  hart->gpr[di.rd] = hart->gpr[di.rs1] << (hart->gpr[di.rs2] & 0x1Fu);               break;
            case INSTRUCTION_OP_SLT:  hart->gpr[di.rd] = (s_word)hart->gpr[di.rs1] <  (s_word)hart->gpr[di.rs2];         break;
            case INSTRUCTION_OP_SLTU: hart->gpr[di.rd] = hart->gpr[di.rs1] <  hart->gpr[di.rs2];                         break;
            case INSTRUCTION_OP_XOR:  hart->gpr[di.rd] = hart->gpr[di.rs1] ^  hart->gpr[di.rs2];                         break;
            case INSTRUCTION_OP_SRL:  hart->gpr[di.rd] = hart->gpr[di.rs1] >> (hart->gpr[di.rs2] & 0x1Fu);               break;
            case INSTRUCTION_OP_SRA:  hart->gpr[di.rd] = (s_word)hart->gpr[di.rs1] >> (hart->gpr[di.rs2] & 0x1Fu);       break;
            case INSTRUCTION_OP_OR:   hart->gpr[di.rd] = hart->gpr[di.rs1] |  hart->gpr[di.rs2];                         break;
            case INSTRUCTION_OP_AND:  hart->gpr[di.rd] = hart->gpr[di.rs1] &  hart->gpr[di.rs2];                         break;

            // -- M extension ---------------------------------------------

            case INSTRUCTION_OP_MUL:
                hart->gpr[di.rd] = (word)((i64)(s_word)hart->gpr[di.rs1] * (i64)(s_word)hart->gpr[di.rs2]);
                break;
            case INSTRUCTION_OP_MULH: {
                i64 tmp = (i64)(s_word)hart->gpr[di.rs1] * (i64)(s_word)hart->gpr[di.rs2];
                hart->gpr[di.rd] = (word)((u64)tmp >> 32);
                break;
            }
            case INSTRUCTION_OP_MULHSU:
                hart->gpr[di.rd] = (word)(((i64)(s_word)hart->gpr[di.rs1] * (u64)hart->gpr[di.rs2]) >> 32);
                break;
            case INSTRUCTION_OP_MULHU:
                hart->gpr[di.rd] = (word)(((u64)hart->gpr[di.rs1] * (u64)hart->gpr[di.rs2]) >> 32);
                break;

            case INSTRUCTION_OP_DIV: {
                s_word a = (s_word)hart->gpr[di.rs1], b = (s_word)hart->gpr[di.rs2];
                hart->gpr[di.rd] = (b == 0) ? 0xFFFFFFFFu
                                 : (a == (s_word)0x80000000 && b == -1) ? 0x80000000u
                                 : (word)(a / b);
                break;
            }
            case INSTRUCTION_OP_DIVU:
                hart->gpr[di.rd] = (hart->gpr[di.rs2] == 0) ? 0xFFFFFFFFu
                                 : hart->gpr[di.rs1] / hart->gpr[di.rs2];
                break;
            case INSTRUCTION_OP_REM: {
                s_word a = (s_word)hart->gpr[di.rs1], b = (s_word)hart->gpr[di.rs2];
                hart->gpr[di.rd] = (b == 0) ? (word)a
                                 : (a == (s_word)0x80000000 && b == -1) ? 0u
                                 : (word)(a % b);
                break;
            }
            case INSTRUCTION_OP_REMU:
                hart->gpr[di.rd] = (hart->gpr[di.rs2] == 0) ? hart->gpr[di.rs1]
                                 : hart->gpr[di.rs1] % hart->gpr[di.rs2];
                break;

            // -- Fences (no-op) ------------------------------------------

            case INSTRUCTION_OP_FENCE:
            case INSTRUCTION_OP_FENCE_I:
                break;

            // -- CSR -----------------------------------------------------

            case INSTRUCTION_OP_CSRRW:  pending_trap = csr_dispatch(hart, &di, CSR_RW, false); break;
            case INSTRUCTION_OP_CSRRS:  pending_trap = csr_dispatch(hart, &di, CSR_RS, false); break;
            case INSTRUCTION_OP_CSRRC:  pending_trap = csr_dispatch(hart, &di, CSR_RC, false); break;
            case INSTRUCTION_OP_CSRRWI: pending_trap = csr_dispatch(hart, &di, CSR_RW, true);  break;
            case INSTRUCTION_OP_CSRRSI: pending_trap = csr_dispatch(hart, &di, CSR_RS, true);  break;
            case INSTRUCTION_OP_CSRRCI: pending_trap = csr_dispatch(hart, &di, CSR_RC, true);  break;

            // -- System --------------------------------------------------

            case INSTRUCTION_OP_ECALL:
                r.trap    = TRAP_ECALL_M;
                r.next_pc = hart->pc + 4;
                hart->gpr[0] = 0;
                hart->pc     = hart->pc + 4;
                pending_trap = TRAP_ECALL_M;
				goto commit;

            case INSTRUCTION_OP_MRET:
                // Restore PC from mepc; mstatus.MIE = mstatus.MPIE
                next_pc = hart->csr.mepc;
				{
					word mpie = (hart->csr.mstatus >> 7) & 1u;
					hart->csr.mstatus = (hart->csr.mstatus & ~0x8u) | (mpie << 3);
					hart->csr.mstatus |= (1u << 7);
				}
                break;

            case INSTRUCTION_OP_EBREAK:
                pending_trap = TRAP_ILLEGAL_INSTR;
                goto commit;

            default:
                fprintf(stderr, "HART: unknown op %d at PC 0x%08X\n", di.op, hart->pc);
                pending_trap = TRAP_ILLEGAL_INSTR;
                goto commit;
        }

        // CSR ops communicate trap via pending_trap -- check here so they
        // fall through to commit like everything else.
        if (pending_trap != TRAP_NONE)
            goto commit;
    }

    // ------------------------------------------------------------------
    // 4. Commit -- clean retirement
    // ------------------------------------------------------------------

commit:
    if (pending_trap != TRAP_NONE) {

		// Not a hardware trap -- returned to caller as policy signal.
		// Caller (emulator/cosim) decides what to do.
		if (pending_trap == TRAP_ECALL_M)
			return r;

        r.trap  = pending_trap;
        word tv = trap_tval(pending_trap, hart->pc, r.instruction, mem_addr, bad_pc);
        next_pc = commit_trap(hart, pending_trap, tv);
    } else {
        // Record register writeback (not for S/B-type -- they have no rd)
        if (di.rd != 0 &&
            di.format != INSTRUCTION_FORMAT_S &&
            di.format != INSTRUCTION_FORMAT_B) {
            r.rd_written = true;
            r.rd         = di.rd;
            r.rd_value   = hart->gpr[di.rd];
        }
    }

    hart->gpr[0] = 0;
    hart->pc     = next_pc;
    r.next_pc    = next_pc;

    return r;
}

// ---------------------------------------------------------------------------
// Debug
// ---------------------------------------------------------------------------

void hart_print(const hart_t *hart, const char *title)
{
    printf("=== %s ===\n", title ? title : "hart");
    printf("PC: 0x%08X\n", hart->pc);
    for (int i = 0; i < XLEN; i += 4) {
        printf("  x%-2d %-5s 0x%08X    x%-2d %-5s 0x%08X    "
               "x%-2d %-5s 0x%08X    x%-2d %-5s 0x%08X\n",
               i,   get_abi_name(i),   hart->gpr[i],
               i+1, get_abi_name(i+1), hart->gpr[i+1],
               i+2, get_abi_name(i+2), hart->gpr[i+2],
               i+3, get_abi_name(i+3), hart->gpr[i+3]);
    }
    printf("  mstatus=0x%08X  mtvec=0x%08X\n",
           hart->csr.mstatus, hart->csr.mtvec);
    printf("  mepc=0x%08X  mcause=0x%08X  mtval=0x%08X  mscratch=0x%08X\n",
           hart->csr.mepc, hart->csr.mcause, hart->csr.mtval, hart->csr.mscratch);
    printf("===\n");
}

const char *rve_trap_to_cstr(trap_t trap)
{
    switch (trap) {
        case TRAP_NONE:                return "none";
        case TRAP_INSTR_MISALIGNED:    return "instruction-address-misaligned";
        case TRAP_INSTR_ACCESS_FAULT:  return "instruction-access-fault";
        case TRAP_ILLEGAL_INSTR:       return "illegal-instruction";
        case TRAP_LOAD_MISALIGNED:     return "load-address-misaligned";
        case TRAP_LOAD_ACCESS_FAULT:   return "load-access-fault";
        case TRAP_STORE_MISALIGNED:    return "store-address-misaligned";
        case TRAP_STORE_ACCESS_FAULT:  return "store-access-fault";
        case TRAP_ECALL_U:             return "ecall-u";
        case TRAP_ECALL_M:             return "ecall-m";
        case TRAP_INT_SW:              return "interrupt-software";
        // case TRAP_INT_TIMER:           return "interrupt-timer";
        // case TRAP_INT_EXTERNAL:        return "interrupt-external";
        default:                       return "unknown";
    }
}
