#include "rve/cpu.h"
#include "rve/types.h"
#include "rve/riscv.h"
#include "rve/decoder.h"

#include <stdio.h>

static inline uint32_t get_opcode(word instruction)
{
	return (instruction & 0x7F);
}

static inline reg_t get_rd(word instruction)
{
	return (instruction >> 7) & 0x1F;
}

static inline reg_t get_funct3(word instruction)
{
	return (instruction >> 12) & 0x7;
}

static inline reg_t get_rs1(word instruction)
{
	return (instruction >> 15) & 0x1F;
}

static inline reg_t get_rs2(word instruction)
{
	return (instruction >> 20) & 0x1F;
}

static inline reg_t get_shamt(word instruction)
{
	return get_rs2(instruction);
}

static inline reg_t get_funct7(word instruction)
{
	return (instruction >> 25) & 0x7F;
}

static inline uint32_t get_imm_i(word instruction)
{
	// signed bit extension
	return (int32_t)instruction >> 20;
}

static inline uint32_t get_imm_s(word instruction)
{
	uint32_t imm_11_5 = get_funct7(instruction);
	uint32_t imm_4_0 = get_rd(instruction);
	uint32_t imm_unsigned = (imm_11_5 << 5) | imm_4_0;
	return (int32_t)(imm_unsigned << 20) >> 20;
}

static inline uint32_t get_imm_u(word instruction)
{
    return (uint32_t)(instruction & 0xFFFFF000);
}

static inline int32_t get_imm_j(word instruction)
{
    uint32_t imm_20    = (instruction >> 31) & 0x1;   // bit 31 -> imm[20]
    uint32_t imm_10_1  = (instruction >> 21) & 0x3FF; // bits 30:21 -> imm[10:1]
    uint32_t imm_11    = (instruction >> 20) & 0x1;   // bit 20 -> imm[11]
    uint32_t imm_19_12 = (instruction >> 12) & 0xFF;  // bits 19:12 -> imm[19:12]
    uint32_t imm_unsigned = (imm_20 << 20) | (imm_19_12 << 12) | (imm_11 << 11) | (imm_10_1 << 1);
    // sign extend desde 21 bits (bit 0 implícito es 0)
    return (int32_t)(imm_unsigned << 11) >> 11;
}

static inline uint32_t get_imm_b(word instruction)
{
	uint32_t imm_12 = (instruction >> 31) & 0x1; // bit 31 -> imm[12]
	uint32_t imm_10_5 = (instruction >> 25) & 0x3F; // bits 30:25 -> imm[10:5]
	uint32_t imm_4_1 = (instruction >> 8) & 0xF; // bits 11:8 -> imm[10:5]
	uint32_t imm_11 = (instruction >> 7) & 0x1; // bits 7 -> imm[11]
	uint32_t imm_unsigned = (imm_12 << 12) | (imm_11 << 11) | (imm_10_5 << 5) | (imm_4_1 << 1);
    return (int32_t)(imm_unsigned << 19) >> 19;
}

static inline void decode_instruction_u(decoded_instruction_t* di, u32 instruction, u32 opcode)
{
	di->format = INSTRUCTION_FORMAT_U;

	switch (opcode) {
		case RISCV_OPCODE_LUI:
			di->op = INSTRUCTION_OP_LUI;
			break;
		case RISCV_OPCODE_AUIPC:
			di->op = INSTRUCTION_OP_AUIPC;
			break;
		default:
			fprintf(stderr, "ERROR: Unknown opcode for U-type instruction: %x\n", opcode);
			di->valid = false;
			return;
	}

	di->imm = get_imm_u(instruction);
	di->rd = get_rd(instruction);
	di->valid = true;
}

static inline void decode_instruction_j(decoded_instruction_t* di, u32 instruction, u32 opcode)
{
	di->format = INSTRUCTION_FORMAT_J;

	switch (opcode) {
		case RISCV_OPCODE_JAL:
			di->op = INSTRUCTION_OP_JAL;
			break;
		default:
			fprintf(stderr, "ERROR: Unknown opcode for J-type instruction: %x\n", opcode);
			return;
	}

	di->imm = get_imm_j(instruction);
	di->rd = get_rd(instruction);
	di->valid = true;
}

static inline void decode_instruction_i(decoded_instruction_t* di, u32 instruction, u32 opcode)
{
	di->format = INSTRUCTION_FORMAT_I;

	di->funct3 = get_funct3(instruction);
	di->rs1 = get_rs1(instruction);
	di->imm = get_imm_i(instruction);
	di->rd = get_rd(instruction);

	switch (opcode)	{
		case RISCV_OPCODE_JALR:
			switch (di->funct3) {
				case RISCV_FUNCT3_JALR:
					di->op = INSTRUCTION_OP_JALR;
					break;
				default:
					fprintf(stderr, "ERROR: Unknown funct3 for JALR: %x\n", di->funct3);
					di->valid = false;
					return;
			}
			break;
		case RISCV_OPCODE_LOAD:
			switch (di->funct3) {
				case RISCV_FUNCT3_LB: di->op = INSTRUCTION_OP_LB; break;
				case RISCV_FUNCT3_LH: di->op = INSTRUCTION_OP_LH; break;
				case RISCV_FUNCT3_LW: di->op = INSTRUCTION_OP_LW; break;
				case RISCV_FUNCT3_LBU: di->op = INSTRUCTION_OP_LBU; break;
				case RISCV_FUNCT3_LHU: di->op = INSTRUCTION_OP_LHU; break;
				default:
					fprintf(stderr, "ERROR: Unknown funct3 for LOAD: %x\n", di->funct3);
					di->valid = false;
					return;
			}
			break;
		case RISCV_OPCODE_OP_IMM:
			switch (di->funct3) {
				case RISCV_FUNCT3_ADDI:	di->op = INSTRUCTION_OP_ADDI; break;
				case RISCV_FUNCT3_SLTI:	di->op = INSTRUCTION_OP_SLTI; break;
				case RISCV_FUNCT3_SLTIU:di->op = INSTRUCTION_OP_SLTIU; break;
				case RISCV_FUNCT3_XORI:	di->op = INSTRUCTION_OP_XORI; break;
				case RISCV_FUNCT3_ORI:	di->op = INSTRUCTION_OP_ORI; break;
				case RISCV_FUNCT3_ANDI:	di->op = INSTRUCTION_OP_ANDI; break;
				case RISCV_FUNCT3_SLLI:	{
					di->funct7 = get_funct7(instruction);
					if (di->funct7 == RISCV_FUNCT7_SLLI) {
						di->op = INSTRUCTION_OP_SLLI;
					}
					else {
						fprintf(stderr, "ERROR: Unknown funct7 for SLLI: %x\n", di->funct7);
						di->valid = false;
						return;
					}
				}
				break;
				case RISCV_FUNCT3_SRLI_SRAI: {
					di->funct7 = get_funct7(instruction);
					if (di->funct7 == RISCV_FUNCT7_SRLI) {
						di->op = INSTRUCTION_OP_SRLI;
						di->imm = get_shamt(instruction);
					}
					else if (di->funct7 == RISCV_FUNCT7_SRAI) {
						di->op = INSTRUCTION_OP_SRAI;
						di->imm = get_shamt(instruction);
					}
					else {
						fprintf(stderr, "ERROR: Unknown funct7 for SRLI/SRAI: %x\n", di->funct7);
						di->valid = false;
						return;
					}
				}
			}
			break;

		case RISCV_OPCODE_MISC_MEM:
			di->imm = 0;
			di->rs1 = get_rs1(instruction);
			di->rd = get_rd(instruction);
			di->funct3 = get_funct3(instruction);

			switch (di->funct3) {
				case RISCV_FUNCT3_FENCE:
					di->op = INSTRUCTION_OP_FENCE;
					break;
				case RISCV_FUNCT3_FENCE_I:
					di->op = INSTRUCTION_OP_FENCE_I;
					break;
				default:
					fprintf(stderr, "ERROR: Unknown funct3 for MISC_MEM: %x\n", di->funct3);
					di->valid = false;
					return;
			}
			break;

		case RISCV_OPCODE_SYSTEM:
			// Extraer campos relevantes para SYSTEM
			// di->imm contendrá el índice CSR o el código ECALL/EBREAK/etc.
			di->imm = get_imm_i(instruction); // Bits 31:20
			di->funct3 = get_funct3(instruction);
			// rd y rs1 ya fueron extraídos. rs1 contiene uimm[4:0] para CSRR*I

			switch (di->funct3) {
				case RISCV_FUNCT3_PRIV: { // Scope para rs2_val
						reg_t rs2_val = get_rs2(instruction); // Bits 24:20
						// Chequear ECALL (imm=0, rd=0, rs1=0, rs2=0)
						if (di->imm == RISCV_FUNCT12_ECALL && di->rs1 == 0 && di->rd == 0 && rs2_val == 0) {
							di->op = INSTRUCTION_OP_ECALL;
							di->imm = 0; // Guardar el código como inmediato
						// Chequear EBREAK (imm=1, rd=0, rs1=0, rs2=1)
						} else if (di->imm == RISCV_FUNCT12_EBREAK && di->rs1 == 0 && di->rd == 0 && rs2_val == 1) {
							di->op = INSTRUCTION_OP_EBREAK;
							di->imm = 1; // Guardar el código como inmediato
						} else {
							// Aquí se manejarían WFI, MRET, SRET, SFENCE.VMA si se implementan
							// diferenciando por los valores en di->imm y rs2_val/funct7
							fprintf(stderr, "TODO/ERROR: Unsupported PRIV instruction (funct3=0, imm=0x%x, rs2=0x%x) in Inst: 0x%08X\n", di->imm, rs2_val, instruction);
						}
						// Forzar rd/rs1 a 0 en la estructura decodificada para ECALL/EBREAK por claridad
						if(di->op == INSTRUCTION_OP_ECALL || di->op == INSTRUCTION_OP_EBREAK) {
							 di->rd = 0;
							 di->rs1 = 0;
						}
					}
					break; // Salir del case PRIV

				// --- Instrucciones CSR ---
				// Nota: di->imm ya contiene el índice CSR (bits 31:20)
				// Para CSRR*I, di->rs1 contiene el uimm[4:0] (bits 19:15)
				case RISCV_FUNCT3_CSRRW:  // 0b001
					di->op = INSTRUCTION_OP_CSRRW;
					break;
				case RISCV_FUNCT3_CSRRS:  // 0b010
					di->op = INSTRUCTION_OP_CSRRS;
					break;
				case RISCV_FUNCT3_CSRRC:  // 0b011
					di->op = INSTRUCTION_OP_CSRRC;
					break;
				case RISCV_FUNCT3_CSRRWI: // 0b101
					di->op = INSTRUCTION_OP_CSRRWI;
					// La ejecución usará di->rs1 como uimm y di->imm como CSR index
					break;
				case RISCV_FUNCT3_CSRRSI: // 0b110
					di->op = INSTRUCTION_OP_CSRRSI;
					break;
				case RISCV_FUNCT3_CSRRCI: // 0b111
					di->op = INSTRUCTION_OP_CSRRCI;
					// La ejecución usará di->rs1 como uimm y di->imm como CSR index
					break;

				default:
					fprintf(stderr, "ERROR: Unknown funct3 0x%x for SYSTEM (Inst: 0x%08X)\n", di->funct3, instruction);
					di->valid = false;
					break;
			}
			break;

		default:
			fprintf(stderr, "ERROR: Unknown opcode for I-type instruction: %x\n", opcode);
			di->valid = false;
			return;
	}

	di->valid = true;
}

static inline void decode_instruction_b(decoded_instruction_t* di, u32 instruction, u32 opcode)
{
	UNUSED(opcode);
	di->format = INSTRUCTION_FORMAT_B;
	di->imm = get_imm_b(instruction);
	di->rs1 = get_rs1(instruction);
	di->rs2 = get_rs2(instruction);
	di->funct3 = get_funct3(instruction);

	// clang-format off
	switch (di->funct3) {
		case RISCV_FUNCT3_BEQ: di->op = INSTRUCTION_OP_BEQ; break;
		case RISCV_FUNCT3_BNE: di->op = INSTRUCTION_OP_BNE; break;
		case RISCV_FUNCT3_BLT: di->op = INSTRUCTION_OP_BLT; break;
		case RISCV_FUNCT3_BGE: di->op = INSTRUCTION_OP_BGE; break;
		case RISCV_FUNCT3_BLTU:	di->op = INSTRUCTION_OP_BLTU; break;
		case RISCV_FUNCT3_BGEU: di->op = INSTRUCTION_OP_BGEU; break;
		default:
			fprintf(stderr, "ERROR: Unknown funct3 for BRANCH instruction: 0x%x (Instruction: 0x%08X)\n",
					di->funct3, instruction);
			di->valid = false;
			break;
	}
	// clang-format on

	di->valid = true;
}

static inline void decode_instruction_s(decoded_instruction_t* di, u32 instruction, u32 opcode)
{
	UNUSED(opcode);
	di->format = INSTRUCTION_FORMAT_S;
	di->imm = get_imm_s(instruction);
	di->rs2 = get_rs2(instruction);
	di->rs1 = get_rs1(instruction);
	di->funct3 = get_funct3(instruction);
	di->rd = get_rd(instruction);

	// clang-format off
	switch (di->funct3) {
		case RISCV_FUNCT3_SB: di->op = INSTRUCTION_OP_SB; break;
		case RISCV_FUNCT3_SH: di->op = INSTRUCTION_OP_SH; break;
		case RISCV_FUNCT3_SW: di->op = INSTRUCTION_OP_SW; break;
		// RV64/RV128 usan funct3=011 para SD
		default:
			// Funct3 inválido para STORE en RV32I base
			fprintf(stderr, "ERROR: Unknown funct3 for STORE instruction: 0x%x (Instruction: 0x%08X)\n",
					di->funct3, instruction);
			di->valid = false;
			break;
	}
	// clang-format on

	di->valid = true;
}

static inline void decode_instruction_r(decoded_instruction_t* di, u32 instruction, u32 opcode)
{
	UNUSED(opcode);

	di->format = INSTRUCTION_FORMAT_R;
	di->imm = 0;

	di->funct7 = get_funct7(instruction);
	di->rs2 = get_rs2(instruction);
	di->rs1 = get_rs1(instruction);
	di->funct3 = get_funct3(instruction);
	di->rd = get_rd(instruction);

	// clang-format off
	switch (di->funct7) {
	case RISCV_FUNCT7_ADD:
		switch (di->funct3) {
			case RISCV_FUNCT3_ADD_SUB:	di->op = INSTRUCTION_OP_ADD; break;
			case RISCV_FUNCT3_SLL:		di->op = INSTRUCTION_OP_SLL; break;
			case RISCV_FUNCT3_SLT:		di->op = INSTRUCTION_OP_SLT; break;
			case RISCV_FUNCT3_SLTU:		di->op = INSTRUCTION_OP_SLTU; break;
			case RISCV_FUNCT3_XOR:		di->op = INSTRUCTION_OP_XOR; break;
			case RISCV_FUNCT3_SRL_SRA:	di->op = INSTRUCTION_OP_SRL; break;
			case RISCV_FUNCT3_OR:		di->op = INSTRUCTION_OP_OR;	 break;
			case RISCV_FUNCT3_AND:		di->op = INSTRUCTION_OP_AND; break;
			default:
				fprintf(stderr, "ERROR: Unknown funct3 for ADD: %x\n", di->funct3);
				di->valid = false;
				return;
		}
		break;
	case RISCV_FUNCT7_SUB:
		switch (di->funct3) {
			case RISCV_FUNCT3_ADD_SUB:	di->op = INSTRUCTION_OP_SUB; break;
			case RISCV_FUNCT3_SRL_SRA:	di->op = INSTRUCTION_OP_SRA; break;
			default:
				fprintf(stderr, "ERROR: Unknown funct3 for SUB: %x\n", di->funct3);
				di->valid = false;
				return;
		}
		break;
	default:
		fprintf(stderr, "ERROR: Unknown funct7 for R-type instruction: %x\n", di->funct7);
		di->valid = false;
		return;
	}
	// clang-format on

	di->valid = true;
}

decoded_instruction_t rve_decode_instruction(word instruction)
{
	u32 opcode = get_opcode(instruction);

	// printf("decode_instruction: %x\n", instruction);
	// printf("opcode: %s (%x)\n", rve_opcode_to_str(opcode), opcode);

	decoded_instruction_t di = {0};

	di.original_instruction = instruction;

	switch (opcode) {
		case RISCV_OPCODE_LUI:
		case RISCV_OPCODE_AUIPC:
			decode_instruction_u(&di, instruction, opcode);
			break;
		case RISCV_OPCODE_JAL:
			decode_instruction_j(&di, instruction, opcode);
			break;
		case RISCV_OPCODE_JALR:
		case RISCV_OPCODE_LOAD:
		case RISCV_OPCODE_OP_IMM:
		case RISCV_OPCODE_MISC_MEM:
		case RISCV_OPCODE_SYSTEM:
			decode_instruction_i(&di, instruction, opcode);
			break;
		case RISCV_OPCODE_BRANCH:
			decode_instruction_b(&di, instruction, opcode);
			break;
		case RISCV_OPCODE_STORE:
			decode_instruction_s(&di, instruction, opcode);
			break;
		case RISCV_OPCODE_OP:
			decode_instruction_r(&di, instruction, opcode);
			break;
		default:
			fprintf(stderr, "ERROR: Unknown opcode: %x\n", opcode);
			return di;
	}
	return di;
}

const char* rve_instruction_format_to_cstr(instruction_format_t format)
{
	switch (format) {
		case INSTRUCTION_FORMAT_UNKNOWN: return "UNKNOWN";
		case INSTRUCTION_FORMAT_R: return "R";
		case INSTRUCTION_FORMAT_I: return "I";
		case INSTRUCTION_FORMAT_S: return "S";
		case INSTRUCTION_FORMAT_B: return "B";
		case INSTRUCTION_FORMAT_U: return "U";
		case INSTRUCTION_FORMAT_J: return "J";
		default: return "UNKNOWN_OTHER";
	}
}

const char* rve_instruction_op_to_cstr(instruction_op_t op)
{
    switch (op) {
		case INSTRUCTION_OP_UNKNOWN: return "INSTRUCTION_OP_UNKNOWN";
        case INSTRUCTION_OP_LUI:     return "INSTRUCTION_OP_LUI";
        case INSTRUCTION_OP_AUIPC:   return "INSTRUCTION_OP_AUIPC";
        case INSTRUCTION_OP_JAL:     return "INSTRUCTION_OP_JAL";
        case INSTRUCTION_OP_JALR:    return "INSTRUCTION_OP_JALR";
        case INSTRUCTION_OP_BEQ:     return "INSTRUCTION_OP_BEQ";
        case INSTRUCTION_OP_BNE:     return "INSTRUCTION_OP_BNE";
        case INSTRUCTION_OP_BLT:     return "INSTRUCTION_OP_BLT";
        case INSTRUCTION_OP_BGE:     return "INSTRUCTION_OP_BGE";
        case INSTRUCTION_OP_BLTU:    return "INSTRUCTION_OP_BLTU";
        case INSTRUCTION_OP_BGEU:    return "INSTRUCTION_OP_BGEU";
        case INSTRUCTION_OP_LB:      return "INSTRUCTION_OP_LB";
        case INSTRUCTION_OP_LH:      return "INSTRUCTION_OP_LH";
        case INSTRUCTION_OP_LW:      return "INSTRUCTION_OP_LW";
        case INSTRUCTION_OP_LBU:     return "INSTRUCTION_OP_LBU";
        case INSTRUCTION_OP_LHU:     return "INSTRUCTION_OP_LHU";
        case INSTRUCTION_OP_SB:      return "INSTRUCTION_OP_SB";
        case INSTRUCTION_OP_SH:      return "INSTRUCTION_OP_SH";
        case INSTRUCTION_OP_SW:      return "INSTRUCTION_OP_SW";
        case INSTRUCTION_OP_ADDI:    return "INSTRUCTION_OP_ADDI";
        case INSTRUCTION_OP_SLTI:    return "INSTRUCTION_OP_SLTI";
        case INSTRUCTION_OP_SLTIU:   return "INSTRUCTION_OP_SLTIU";
        case INSTRUCTION_OP_XORI:    return "INSTRUCTION_OP_XORI";
        case INSTRUCTION_OP_ORI:     return "INSTRUCTION_OP_ORI";
        case INSTRUCTION_OP_ANDI:    return "INSTRUCTION_OP_ANDI";
        case INSTRUCTION_OP_SLLI:    return "INSTRUCTION_OP_SLLI";
        case INSTRUCTION_OP_SRLI:    return "INSTRUCTION_OP_SRLI";
        case INSTRUCTION_OP_SRAI:    return "INSTRUCTION_OP_SRAI";
        case INSTRUCTION_OP_ADD:     return "INSTRUCTION_OP_ADD";
        case INSTRUCTION_OP_SUB:     return "INSTRUCTION_OP_SUB";
        case INSTRUCTION_OP_SLL:     return "INSTRUCTION_OP_SLL";
        case INSTRUCTION_OP_SLT:     return "INSTRUCTION_OP_SLT";
        case INSTRUCTION_OP_SLTU:    return "INSTRUCTION_OP_SLTU";
        case INSTRUCTION_OP_XOR:     return "INSTRUCTION_OP_XOR";
        case INSTRUCTION_OP_SRL:     return "INSTRUCTION_OP_SRL";
        case INSTRUCTION_OP_SRA:     return "INSTRUCTION_OP_SRA";
        case INSTRUCTION_OP_OR:      return "INSTRUCTION_OP_OR";
        case INSTRUCTION_OP_AND:     return "INSTRUCTION_OP_AND";
        case INSTRUCTION_OP_FENCE:   return "INSTRUCTION_OP_FENCE";
        case INSTRUCTION_OP_FENCE_I: return "INSTRUCTION_OP_FENCE_I";
        case INSTRUCTION_OP_ECALL:   return "INSTRUCTION_OP_ECALL";
        case INSTRUCTION_OP_EBREAK:  return "INSTRUCTION_OP_EBREAK";
        case INSTRUCTION_OP_CSRRW:   return "INSTRUCTION_OP_CSRRW";
        case INSTRUCTION_OP_CSRRS:   return "INSTRUCTION_OP_CSRRS";
        case INSTRUCTION_OP_CSRRC:   return "INSTRUCTION_OP_CSRRC";
        case INSTRUCTION_OP_CSRRWI:  return "INSTRUCTION_OP_CSRRWI";
        case INSTRUCTION_OP_CSRRSI:  return "INSTRUCTION_OP_CSRRSI";
        case INSTRUCTION_OP_CSRRCI:  return "INSTRUCTION_OP_CSRRCI";
		default: return "INSTRUCTION_OP_UNKNOWN_OTHER";
    }
}

const char* rve_opcode_to_str(uint32_t opcode)
{
    switch (opcode & 0x7F) {
        case RISCV_OPCODE_LUI:        return "RISCV_OPCODE_LUI";
        case RISCV_OPCODE_AUIPC:      return "RISCV_OPCODE_AUIPC";
        case RISCV_OPCODE_JAL:        return "RISCV_OPCODE_JAL";
        case RISCV_OPCODE_JALR:       return "RISCV_OPCODE_JALR";
        case RISCV_OPCODE_BRANCH:     return "RISCV_OPCODE_BRANCH";
        case RISCV_OPCODE_LOAD:       return "RISCV_OPCODE_LOAD";
        case RISCV_OPCODE_STORE:      return "RISCV_OPCODE_STORE";
        case RISCV_OPCODE_OP_IMM:     return "RISCV_OPCODE_OP_IMM";
        case RISCV_OPCODE_OP:         return "RISCV_OPCODE_OP";
        case RISCV_OPCODE_MISC_MEM:   return "RISCV_OPCODE_MISC_MEM";
        case RISCV_OPCODE_SYSTEM:     return "RISCV_OPCODE_SYSTEM";
        default:                      return "RISCV_OPCODE_UNKNOWN";
    }
}

// nombres ABI estándar para los registros x0-x31
// clang-format off
static const char* abi_names[32] = {
    "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
    "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
    "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
    "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"
};
// clang-format on

const char *get_abi_name(reg_t reg_index)
{
	if (reg_index < XLEN) {
		return abi_names[reg_index];
	}
	return "x??";
}

int rve_decoded_format_to_buffer(const decoded_instruction_t *di, char *buffer, size_t buffer_size)
{
	if (!di || !buffer || buffer_size == 0) {
		if (buffer && buffer_size > 0)
			buffer[0] = '\0';
		return -1;
	}

	buffer[0] = '\0';

	if (!di->valid) {
		return snprintf(buffer, buffer_size, "<invalid instruction: 0x%08X>",
				di->original_instruction);
	}

	// clang-format off
    switch (di->op) {
        // --- U-Type ---
        case INSTRUCTION_OP_LUI:
            return snprintf(buffer, buffer_size, "%-8s %s, 0x%x", "lui", get_abi_name(di->rd), (uint32_t)di->imm >> 12);
        case INSTRUCTION_OP_AUIPC:
            return snprintf(buffer, buffer_size, "%-8s %s, 0x%x", "auipc", get_abi_name(di->rd), (uint32_t)di->imm >> 12);

        // --- J-Type ---
        case INSTRUCTION_OP_JAL:
            return snprintf(buffer, buffer_size, "%-8s %s, %d", "jal", get_abi_name(di->rd), di->imm);

        // --- I-Type (JALR) ---
        case INSTRUCTION_OP_JALR:
            return snprintf(buffer, buffer_size, "%-8s %s, %d(%s)", "jalr", get_abi_name(di->rd), di->imm, get_abi_name(di->rs1));

        // --- B-Type (Branches) ---
        case INSTRUCTION_OP_BEQ:
            return snprintf(buffer, buffer_size, "%-8s %s, %s, %d", "beq", get_abi_name(di->rs1), get_abi_name(di->rs2), di->imm);
        case INSTRUCTION_OP_BNE:
            return snprintf(buffer, buffer_size, "%-8s %s, %s, %d", "bne", get_abi_name(di->rs1), get_abi_name(di->rs2), di->imm);
        case INSTRUCTION_OP_BLT:
            return snprintf(buffer, buffer_size, "%-8s %s, %s, %d", "blt", get_abi_name(di->rs1), get_abi_name(di->rs2), di->imm);
        case INSTRUCTION_OP_BGE:
            return snprintf(buffer, buffer_size, "%-8s %s, %s, %d", "bge", get_abi_name(di->rs1), get_abi_name(di->rs2), di->imm);
        case INSTRUCTION_OP_BLTU:
            return snprintf(buffer, buffer_size, "%-8s %s, %s, %d", "bltu", get_abi_name(di->rs1), get_abi_name(di->rs2), di->imm);
        case INSTRUCTION_OP_BGEU:
            return snprintf(buffer, buffer_size, "%-8s %s, %s, %d", "bgeu", get_abi_name(di->rs1), get_abi_name(di->rs2), di->imm);

        // --- I-Type (Loads) ---
        case INSTRUCTION_OP_LB:
            return snprintf(buffer, buffer_size, "%-8s %s, %d(%s)", "lb", get_abi_name(di->rd), di->imm, get_abi_name(di->rs1));
        case INSTRUCTION_OP_LH:
            return snprintf(buffer, buffer_size, "%-8s %s, %d(%s)", "lh", get_abi_name(di->rd), di->imm, get_abi_name(di->rs1));
        case INSTRUCTION_OP_LW:
            return snprintf(buffer, buffer_size, "%-8s %s, %d(%s)", "lw", get_abi_name(di->rd), di->imm, get_abi_name(di->rs1));
        case INSTRUCTION_OP_LBU:
            return snprintf(buffer, buffer_size, "%-8s %s, %d(%s)", "lbu", get_abi_name(di->rd), di->imm, get_abi_name(di->rs1));
        case INSTRUCTION_OP_LHU:
            return snprintf(buffer, buffer_size, "%-8s %s, %d(%s)", "lhu", get_abi_name(di->rd), di->imm, get_abi_name(di->rs1));

        // --- S-Type (Stores) ---
        case INSTRUCTION_OP_SB:
            return snprintf(buffer, buffer_size, "%-8s %s, %d(%s)", "sb", get_abi_name(di->rs2), di->imm, get_abi_name(di->rs1));
        case INSTRUCTION_OP_SH:
            return snprintf(buffer, buffer_size, "%-8s %s, %d(%s)", "sh", get_abi_name(di->rs2), di->imm, get_abi_name(di->rs1));
        case INSTRUCTION_OP_SW:
            return snprintf(buffer, buffer_size, "%-8s %s, %d(%s)", "sw", get_abi_name(di->rs2), di->imm, get_abi_name(di->rs1));

        // --- I-Type (Op-Immediate Arithmetic/Logic) ---
        case INSTRUCTION_OP_ADDI:
            return snprintf(buffer, buffer_size, "%-8s %s, %s, %d", "addi", get_abi_name(di->rd), get_abi_name(di->rs1), di->imm);
        case INSTRUCTION_OP_SLTI:
            return snprintf(buffer, buffer_size, "%-8s %s, %s, %d", "slti", get_abi_name(di->rd), get_abi_name(di->rs1), di->imm);
        case INSTRUCTION_OP_SLTIU:
            return snprintf(buffer, buffer_size, "%-8s %s, %s, %u", "sltiu", get_abi_name(di->rd), get_abi_name(di->rs1), di->imm);
        case INSTRUCTION_OP_XORI:
            return snprintf(buffer, buffer_size, "%-8s %s, %s, %d", "xori", get_abi_name(di->rd), get_abi_name(di->rs1), di->imm);
        case INSTRUCTION_OP_ORI:
            return snprintf(buffer, buffer_size, "%-8s %s, %s, %d", "ori", get_abi_name(di->rd), get_abi_name(di->rs1), di->imm);
        case INSTRUCTION_OP_ANDI:
            return snprintf(buffer, buffer_size, "%-8s %s, %s, %d", "andi", get_abi_name(di->rd), get_abi_name(di->rs1), di->imm);

        // --- I-Type (Op-Immediate Shifts) --- imm contiene shamt
        case INSTRUCTION_OP_SLLI:
            return snprintf(buffer, buffer_size, "%-8s %s, %s, %u", "slli", get_abi_name(di->rd), get_abi_name(di->rs1), (uint32_t)di->imm & 0x1F);
        case INSTRUCTION_OP_SRLI:
            return snprintf(buffer, buffer_size, "%-8s %s, %s, %u", "srli", get_abi_name(di->rd), get_abi_name(di->rs1), (uint32_t)di->imm & 0x1F);
        case INSTRUCTION_OP_SRAI:
            return snprintf(buffer, buffer_size, "%-8s %s, %s, %u", "srai", get_abi_name(di->rd), get_abi_name(di->rs1), (uint32_t)di->imm & 0x1F);

        // --- R-Type (Op Register Arithmetic/Logic) ---
        case INSTRUCTION_OP_ADD:
            return snprintf(buffer, buffer_size, "%-8s %s, %s, %s", "add", get_abi_name(di->rd), get_abi_name(di->rs1), get_abi_name(di->rs2));
        case INSTRUCTION_OP_SUB:
            return snprintf(buffer, buffer_size, "%-8s %s, %s, %s", "sub", get_abi_name(di->rd), get_abi_name(di->rs1), get_abi_name(di->rs2));
        case INSTRUCTION_OP_SLL:
            return snprintf(buffer, buffer_size, "%-8s %s, %s, %s", "sll", get_abi_name(di->rd), get_abi_name(di->rs1), get_abi_name(di->rs2));
        case INSTRUCTION_OP_SLT:
            return snprintf(buffer, buffer_size, "%-8s %s, %s, %s", "slt", get_abi_name(di->rd), get_abi_name(di->rs1), get_abi_name(di->rs2));
        case INSTRUCTION_OP_SLTU:
            return snprintf(buffer, buffer_size, "%-8s %s, %s, %s", "sltu", get_abi_name(di->rd), get_abi_name(di->rs1), get_abi_name(di->rs2));
        case INSTRUCTION_OP_XOR:
            return snprintf(buffer, buffer_size, "%-8s %s, %s, %s", "xor", get_abi_name(di->rd), get_abi_name(di->rs1), get_abi_name(di->rs2));
        case INSTRUCTION_OP_SRL:
            return snprintf(buffer, buffer_size, "%-8s %s, %s, %s", "srl", get_abi_name(di->rd), get_abi_name(di->rs1), get_abi_name(di->rs2));
        case INSTRUCTION_OP_SRA:
            return snprintf(buffer, buffer_size, "%-8s %s, %s, %s", "sra", get_abi_name(di->rd), get_abi_name(di->rs1), get_abi_name(di->rs2));
        case INSTRUCTION_OP_OR:
            return snprintf(buffer, buffer_size, "%-8s %s, %s, %s", "or", get_abi_name(di->rd), get_abi_name(di->rs1), get_abi_name(di->rs2));
        case INSTRUCTION_OP_AND:
            return snprintf(buffer, buffer_size, "%-8s %s, %s, %s", "and", get_abi_name(di->rd), get_abi_name(di->rs1), get_abi_name(di->rs2));

        // --- I-Type (Memory Synchronization) ---
        case INSTRUCTION_OP_FENCE:
            return snprintf(buffer, buffer_size, "%-8s", "fence"); // Podría añadir pred/succ
        case INSTRUCTION_OP_FENCE_I:
            return snprintf(buffer, buffer_size, "%-8s", "fence.i");

        // --- I-Type (System calls / CSRs) ---
        case INSTRUCTION_OP_ECALL:
            return snprintf(buffer, buffer_size, "%-8s", "ecall");
        case INSTRUCTION_OP_EBREAK:
            return snprintf(buffer, buffer_size, "%-8s", "ebreak");
        case INSTRUCTION_OP_CSRRW:
            return snprintf(buffer, buffer_size, "%-8s %s, 0x%03x, %s", "csrrw", get_abi_name(di->rd), (uint32_t)di->imm & 0xFFF, get_abi_name(di->rs1));
        case INSTRUCTION_OP_CSRRS:
            return snprintf(buffer, buffer_size, "%-8s %s, 0x%03x, %s", "csrrs", get_abi_name(di->rd), (uint32_t)di->imm & 0xFFF, get_abi_name(di->rs1));
        case INSTRUCTION_OP_CSRRC:
            return snprintf(buffer, buffer_size, "%-8s %s, 0x%03x, %s", "csrrc", get_abi_name(di->rd), (uint32_t)di->imm & 0xFFF, get_abi_name(di->rs1));
        case INSTRUCTION_OP_CSRRWI:
            return snprintf(buffer, buffer_size, "%-8s %s, 0x%03x, %u", "csrrwi", get_abi_name(di->rd), (uint32_t)di->imm & 0xFFF, (uint32_t)di->rs1);
        case INSTRUCTION_OP_CSRRSI:
            return snprintf(buffer, buffer_size, "%-8s %s, 0x%03x, %u", "csrrsi", get_abi_name(di->rd), (uint32_t)di->imm & 0xFFF, (uint32_t)di->rs1);
        case INSTRUCTION_OP_CSRRCI:
            return snprintf(buffer, buffer_size, "%-8s %s, 0x%03x, %u", "csrrci", get_abi_name(di->rd), (uint32_t)di->imm & 0xFFF, (uint32_t)di->rs1);

        // Caso por defecto para operaciones válidas pero no manejadas explícitamente
        case INSTRUCTION_OP_UNKNOWN: // Este caso es para di->valid = false, ya manejado arriba
            return snprintf(buffer, buffer_size, "<internal error: should be invalid>");
        case INSTRUCTION_OP_NUM: // Marcador, no es una instrucción real
             return snprintf(buffer, buffer_size, "<internal error: OP_NUM>");
        default:
            return snprintf(buffer, buffer_size, "<unhandled valid op: %d format: %s>", di->op, rve_instruction_format_to_cstr(di->format));
    }
	// clang-format off
}
