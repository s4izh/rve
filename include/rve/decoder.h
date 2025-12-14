#ifndef RVE_DECODER_H
#define RVE_DECODER_H

#include "rve/types.h"

#include <stdbool.h>
#include <stdio.h>

typedef enum {
	INSTRUCTION_FORMAT_UNKNOWN = 0,
	INSTRUCTION_FORMAT_R,
	INSTRUCTION_FORMAT_I,
	INSTRUCTION_FORMAT_S,
	INSTRUCTION_FORMAT_B,
	INSTRUCTION_FORMAT_U,
	INSTRUCTION_FORMAT_J,
	INSTRUCTION_FORMAT_NUM,
} instruction_format_t;

typedef enum {
    // === Valor para instrucción desconocida/ilegal ===
    INSTRUCTION_OP_UNKNOWN = 0,

	// === U-Type ===
	INSTRUCTION_OP_LUI,    // Load Upper Immediate
	INSTRUCTION_OP_AUIPC,  // Add Upper Immediate to PC

	// === J-Type ===
	INSTRUCTION_OP_JAL,    // Jump and Link

	// === I-Type (JALR) ===
	INSTRUCTION_OP_JALR,   // Jump and Link Register

	// === B-Type (Branches) ===
	INSTRUCTION_OP_BEQ,    // Branch if Equal
	INSTRUCTION_OP_BNE,    // Branch if Not Equal
	INSTRUCTION_OP_BLT,    // Branch if Less Than
	INSTRUCTION_OP_BGE,    // Branch if Greater or Equal
	INSTRUCTION_OP_BLTU,   // Branch if Less Than Unsigned
	INSTRUCTION_OP_BGEU,   // Branch if Greater or Equal Unsigned

	// === I-Type (Loads) ===
	INSTRUCTION_OP_LB,     // Load Byte
	INSTRUCTION_OP_LH,     // Load Halfword
	INSTRUCTION_OP_LW,     // Load Word
	INSTRUCTION_OP_LBU,    // Load Byte Unsigned
	INSTRUCTION_OP_LHU,    // Load Halfword Unsigned

	// === S-Type (Stores) ===
	INSTRUCTION_OP_SB,     // Store Byte
	INSTRUCTION_OP_SH,     // Store Halfword
	INSTRUCTION_OP_SW,     // Store Word

	// === I-Type (Op-Immediate Arithmetic/Logic) ===
	INSTRUCTION_OP_ADDI,   // Add Immediate
	INSTRUCTION_OP_SLTI,   // Set Less Than Immediate
	INSTRUCTION_OP_SLTIU,  // Set Less Than Immediate Unsigned
	INSTRUCTION_OP_XORI,   // XOR Immediate
	INSTRUCTION_OP_ORI,    // OR Immediate
	INSTRUCTION_OP_ANDI,   // AND Immediate

	// === I-Type (Op-Immediate Shifts) ===
	INSTRUCTION_OP_SLLI,   // Shift Left Logical Immediate
	INSTRUCTION_OP_SRLI,   // Shift Right Logical Immediate
	INSTRUCTION_OP_SRAI,   // Shift Right Arithmetic Immediate

	// === R-Type (Op Register Arithmetic/Logic) ===
	INSTRUCTION_OP_ADD,    // Add
	INSTRUCTION_OP_SUB,    // Subtract
	INSTRUCTION_OP_SLL,    // Shift Left Logical
	INSTRUCTION_OP_SLT,    // Set Less Than
	INSTRUCTION_OP_SLTU,   // Set Less Than Unsigned
	INSTRUCTION_OP_XOR,    // XOR
	INSTRUCTION_OP_SRL,    // Shift Right Logical
	INSTRUCTION_OP_SRA,    // Shift Right Arithmetic
	INSTRUCTION_OP_OR,     // OR
	INSTRUCTION_OP_AND,    // AND

	// === I-Type (Memory Synchronization) ===
	INSTRUCTION_OP_FENCE,  // Fence
	INSTRUCTION_OP_FENCE_I,// Fence.i

	// === I-Type (System calls / CSRs - from Zicsr extension) ===
	INSTRUCTION_OP_ECALL,  // Environment Call
	INSTRUCTION_OP_EBREAK, // Environment Breakpoint
	// Instrucciones CSR (Podrían detallarse o agruparse)
	INSTRUCTION_OP_CSRRW,  // Atomic Read/Write CSR
	INSTRUCTION_OP_CSRRS,  // Atomic Read and Set Bits in CSR
	INSTRUCTION_OP_CSRRC,  // Atomic Read and Clear Bits in CSR
	INSTRUCTION_OP_CSRRWI, // Atomic Read/Write CSR Immediate
	INSTRUCTION_OP_CSRRSI, // Atomic Read and Set Bits in CSR Immediate
	INSTRUCTION_OP_CSRRCI, // Atomic Read and Clear Bits in CSR Immediate
	INSTRUCTION_OP_NUM
} instruction_op_t;

typedef struct {
	word original_instruction;
	instruction_op_t op;
	instruction_format_t format;
	uint32_t imm;
	reg_t rs1;
	reg_t rs2;
	reg_t rd;
	funct_t funct3;
	funct_t funct7;
	bool valid;
} decoded_instruction_t;

decoded_instruction_t rve_decode_instruction(word instruction);

const char* rve_instruction_op_to_cstr(instruction_op_t op);
const char* rve_instruction_format_to_cstr(instruction_format_t format);
const char* rve_opcode_to_str(uint32_t opcode);

/**
 * @brief Formatea una instrucción decodificada en un buffer proporcionado,
 *        en formato legible tipo ensamblador.
 *
 * @param di Puntero a la estructura de la instrucción decodificada.
 * @param buffer El buffer de caracteres donde se escribirá la salida.
 * @param buffer_size El tamaño total del buffer (incluyendo espacio para el '\0').
 *
 * @return El número de caracteres que se habrían escrito si el buffer
 *         fuera suficientemente grande (excluyendo el '\0'), similar a snprintf.
 *         Retorna un valor negativo si hubo un error (ej. entrada inválida).
 */
int rve_decoded_format_to_buffer(const decoded_instruction_t* di, char* buffer, size_t buffer_size);

int run_decoder_tests(void);

const char* get_abi_name(reg_t reg_index);

#endif // !RVE_DECODER_H
