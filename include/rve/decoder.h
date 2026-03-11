/**
 * @file decoder.h
 * @brief RISC-V instruction decoder and disassembler.
 *
 * Decodes a 32-bit instruction word into a @ref decoded_instruction_t that
 * carries all fields (opcode, format, immediate, register indices).
 * A lightweight disassembler formats the decoded result into a human-readable
 * assembly-style string.
 */

#ifndef RVE_DECODER_H
#define RVE_DECODER_H

#include "rve/types.h"

#include <stdbool.h>
#include <stdio.h>

/** @brief RISC-V instruction encoding formats. */
typedef enum {
    INSTRUCTION_FORMAT_UNKNOWN = 0, /**< Unknown or unrecognised format. */
    INSTRUCTION_FORMAT_R,           /**< R-type: register-register ops.  */
    INSTRUCTION_FORMAT_I,           /**< I-type: immediate / loads.      */
    INSTRUCTION_FORMAT_S,           /**< S-type: stores.                 */
    INSTRUCTION_FORMAT_B,           /**< B-type: conditional branches.   */
    INSTRUCTION_FORMAT_U,           /**< U-type: upper-immediate ops.    */
    INSTRUCTION_FORMAT_J,           /**< J-type: unconditional jumps.    */
    INSTRUCTION_FORMAT_NUM,         /**< Sentinel: number of formats.    */
} instruction_format_t;

/** @brief Decoded opcode enumeration covering RV32IMAZicsr. */
typedef enum {
    INSTRUCTION_OP_UNKNOWN = 0, /**< Unknown or illegal instruction. */

    /* U-type */
    INSTRUCTION_OP_LUI,    /**< Load Upper Immediate.           */
    INSTRUCTION_OP_AUIPC,  /**< Add Upper Immediate to PC.      */

    /* J-type */
    INSTRUCTION_OP_JAL,    /**< Jump and Link.                  */

    /* I-type (JALR) */
    INSTRUCTION_OP_JALR,   /**< Jump and Link Register.         */

    /* B-type (branches) */
    INSTRUCTION_OP_BEQ,    /**< Branch if Equal.                */
    INSTRUCTION_OP_BNE,    /**< Branch if Not Equal.            */
    INSTRUCTION_OP_BLT,    /**< Branch if Less Than (signed).   */
    INSTRUCTION_OP_BGE,    /**< Branch if >= (signed).          */
    INSTRUCTION_OP_BLTU,   /**< Branch if Less Than (unsigned). */
    INSTRUCTION_OP_BGEU,   /**< Branch if >= (unsigned).        */

    /* I-type (loads) */
    INSTRUCTION_OP_LB,     /**< Load Byte (sign-extended).      */
    INSTRUCTION_OP_LH,     /**< Load Halfword (sign-extended).  */
    INSTRUCTION_OP_LW,     /**< Load Word.                      */
    INSTRUCTION_OP_LBU,    /**< Load Byte (zero-extended).      */
    INSTRUCTION_OP_LHU,    /**< Load Halfword (zero-extended).  */

    /* S-type (stores) */
    INSTRUCTION_OP_SB,     /**< Store Byte.                     */
    INSTRUCTION_OP_SH,     /**< Store Halfword.                 */
    INSTRUCTION_OP_SW,     /**< Store Word.                     */

    /* I-type (op-immediate arithmetic/logic) */
    INSTRUCTION_OP_ADDI,   /**< Add Immediate.                  */
    INSTRUCTION_OP_SLTI,   /**< Set Less Than Immediate.        */
    INSTRUCTION_OP_SLTIU,  /**< Set Less Than Immediate Unsigned.*/
    INSTRUCTION_OP_XORI,   /**< XOR Immediate.                  */
    INSTRUCTION_OP_ORI,    /**< OR Immediate.                   */
    INSTRUCTION_OP_ANDI,   /**< AND Immediate.                  */

    /* I-type (shifts) */
    INSTRUCTION_OP_SLLI,   /**< Shift Left Logical Immediate.   */
    INSTRUCTION_OP_SRLI,   /**< Shift Right Logical Immediate.  */
    INSTRUCTION_OP_SRAI,   /**< Shift Right Arithmetic Immediate.*/

    /* R-type (arithmetic/logic) */
    INSTRUCTION_OP_ADD,    /**< Add.                            */
    INSTRUCTION_OP_SUB,    /**< Subtract.                       */
    INSTRUCTION_OP_SLL,    /**< Shift Left Logical.             */
    INSTRUCTION_OP_SLT,    /**< Set Less Than (signed).         */
    INSTRUCTION_OP_SLTU,   /**< Set Less Than (unsigned).       */
    INSTRUCTION_OP_XOR,    /**< XOR.                            */
    INSTRUCTION_OP_SRL,    /**< Shift Right Logical.            */
    INSTRUCTION_OP_SRA,    /**< Shift Right Arithmetic.         */
    INSTRUCTION_OP_OR,     /**< OR.                             */
    INSTRUCTION_OP_AND,    /**< AND.                            */

    /* I-type (memory ordering) */
    INSTRUCTION_OP_FENCE,   /**< Memory fence.                  */
    INSTRUCTION_OP_FENCE_I, /**< Instruction fence.             */

    /* System / Zicsr */
    INSTRUCTION_OP_ECALL,  /**< Environment call.               */
    INSTRUCTION_OP_EBREAK, /**< Environment breakpoint.         */
    INSTRUCTION_OP_MRET,   /**< Machine-mode return.            */

    /* R-type (M extension -- multiply/divide) */
    INSTRUCTION_OP_MUL,    /**< Multiply (lower 32 bits).       */
    INSTRUCTION_OP_MULH,   /**< Multiply high (signed*signed).  */
    INSTRUCTION_OP_MULHSU, /**< Multiply high (signed*unsigned).*/
    INSTRUCTION_OP_MULHU,  /**< Multiply high (unsigned*unsigned).*/
    INSTRUCTION_OP_DIV,    /**< Divide (signed).                */
    INSTRUCTION_OP_DIVU,   /**< Divide (unsigned).              */
    INSTRUCTION_OP_REM,    /**< Remainder (signed).             */
    INSTRUCTION_OP_REMU,   /**< Remainder (unsigned).           */

    /* Zicsr -- CSR instructions */
    INSTRUCTION_OP_CSRRW,  /**< Atomic Read/Write CSR.              */
    INSTRUCTION_OP_CSRRS,  /**< Atomic Read and Set Bits in CSR.    */
    INSTRUCTION_OP_CSRRC,  /**< Atomic Read and Clear Bits in CSR.  */
    INSTRUCTION_OP_CSRRWI, /**< Atomic Read/Write CSR Immediate.    */
    INSTRUCTION_OP_CSRRSI, /**< Atomic Read and Set Bits (imm).     */
    INSTRUCTION_OP_CSRRCI, /**< Atomic Read and Clear Bits (imm).   */

    INSTRUCTION_OP_NUM  /**< Sentinel: total number of opcodes. */
} instruction_op_t;

/**
 * @brief Fully decoded instruction.
 *
 * Produced by @ref rve_decode_instruction.  When @p valid is false all
 * other fields are undefined.
 */
typedef struct {
    word                 original_instruction; /**< Raw 32-bit instruction word.  */
    instruction_op_t     op;                   /**< Decoded opcode.               */
    instruction_format_t format;               /**< Encoding format (R/I/S/...).  */
    uint32_t             imm;                  /**< Decoded immediate value.      */
    reg_t                rs1;                  /**< Source register 1 index.      */
    reg_t                rs2;                  /**< Source register 2 index.      */
    reg_t                rd;                   /**< Destination register index.   */
    funct_t              funct3;               /**< funct3 field.                 */
    funct_t              funct7;               /**< funct7 field.                 */
    bool                 valid;                /**< False for illegal encodings.  */
} decoded_instruction_t;

/**
 * @brief Decode a 32-bit instruction word.
 *
 * @param instruction  Raw instruction word.
 * @return             Decoded fields; @p valid is false on illegal encodings.
 */
decoded_instruction_t rve_decode_instruction(word instruction);

/** @brief Convert an opcode enum value to a C string (e.g. "addi"). */
const char *rve_instruction_op_to_cstr(instruction_op_t op);

/** @brief Convert a format enum value to a C string (e.g. "I-type"). */
const char *rve_instruction_format_to_cstr(instruction_format_t format);

/** @brief Convert a 7-bit opcode field to a mnemonic string. */
const char *rve_opcode_to_str(uint32_t opcode);

/**
 * @brief Disassemble a decoded instruction into a buffer.
 *
 * Produces an assembly-style string (e.g. @c "addi sp, sp, -16").
 * The output is always NUL-terminated if @p buffer_size > 0.
 *
 * @param di           Decoded instruction to format.
 * @param buffer       Destination character buffer.
 * @param buffer_size  Total buffer size including the NUL terminator.
 * @return             Number of characters that would have been written
 *                     (excluding NUL), as per snprintf.  Negative on error.
 */
int rve_decoded_format_to_buffer(const decoded_instruction_t *di,
                                 char *buffer, size_t buffer_size);

/** @brief Run the built-in decoder unit tests.  Returns 0 on success. */
int run_decoder_tests(void);

/**
 * @brief Return the ABI name for a register index (e.g. 2 -> "sp").
 * @param reg_index  Register index 0-31.
 * @return           NUL-terminated ABI name string.
 */
const char *get_abi_name(reg_t reg_index);

#endif /* RVE_DECODER_H */
