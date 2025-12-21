#include "rve/cpu.h"
#include "rve/bus.h"
#include "rve/decoder.h"

void cpu_init(cpu_t *cpu, bus_t *bus, u64 pc)
{
	cpu->bus = bus;
	cpu->pc = pc;

	for (u32 i = 0; i < XLEN; ++i)
		cpu->regs[i] = 0;
}

static inline word sext(word value, u32 size)
{
	return (s_word)(value << (XLEN - size)) >> (XLEN - size);
}

static bool cpu_read(cpu_t *cpu, word address, u8 size, word *value)
{
	if (!bus_read(cpu->bus, address, size, value)) {
		fprintf(stderr, "ERROR: Failed to read from bus at address 0x%08X\n", address);
		return false;
	}
	return true;
}

static bool cpu_write(cpu_t *cpu, word address, u8 size, word value)
{
	if (!bus_write(cpu->bus, address, size, value)) {
		fprintf(stderr, "ERROR: Failed to write to bus at address 0x%08X\n", address);
		return false;
	}
	return true;
}

trap_t cpu_execute(cpu_t *cpu, decoded_instruction_t *di)
{
	dword tmp_overflow;
	u64 value = 0;
	trap_t trap = TRAP_OK;

	word read_value = 0;

	switch (di->op) {
		case INSTRUCTION_OP_LUI:
			cpu->regs[di->rd] = (di->imm << 12) & 0x000;
			break;
		case INSTRUCTION_OP_AUIPC:
			cpu->regs[di->rd] = cpu->pc + ((di->imm << 12) & 0x000);
			break;
		case INSTRUCTION_OP_JAL:
			cpu->regs[di->rd] = cpu->pc + 4;
			cpu->pc = sext(di->imm, 21);
			break;
		case INSTRUCTION_OP_JALR:
			value = cpu->pc + 4;
			cpu->pc = (di->rs1 + sext(di->imm, 12)) & ~(word)1;
			cpu->regs[di->rd] = value;
			break;
		case INSTRUCTION_OP_BEQ:
			if (cpu->regs[di->rs1] == cpu->regs[di->rs2])
				cpu->pc += sext(di->imm, 13);
			break;
		case INSTRUCTION_OP_BNE:
			if (cpu->regs[di->rs1] != cpu->regs[di->rs2])
				cpu->pc += sext(di->imm, 13);
			break;
		case INSTRUCTION_OP_BLT:
			if ((s_word)cpu->regs[di->rs1] < (s_word)cpu->regs[di->rs2])
				cpu->pc += sext(di->imm, 13);
			break;
		case INSTRUCTION_OP_BGE:
			if ((s_word)cpu->regs[di->rs1] >= (s_word)cpu->regs[di->rs2])
				cpu->pc += sext(di->imm, 13);
			break;
		case INSTRUCTION_OP_BLTU:
			if (cpu->regs[di->rs1] < cpu->regs[di->rs2])
				cpu->pc += sext(di->imm, 13);
			break;
		case INSTRUCTION_OP_BGEU:
			if (cpu->regs[di->rs1] >= cpu->regs[di->rs2])
				cpu->pc += sext(di->imm, 13);
			break;
		case INSTRUCTION_OP_LB:
			if (!cpu_read(cpu, cpu->regs[di->rs1] + sext(di->imm, 12), 8, &read_value)) {
				return TRAP_ERR;
			}
			cpu->regs[di->rd] = sext(value, 8);
			break;
		case INSTRUCTION_OP_LH:
			if (!cpu_read(cpu, cpu->regs[di->rs1] + sext(di->imm, 12), 16, &read_value)) {
				return TRAP_ERR;
			}
			cpu->regs[di->rd] = sext(value, 16);
			break;
		case INSTRUCTION_OP_LW:
			if (!cpu_read(cpu, cpu->regs[di->rs1] + sext(di->imm, 12), 32, &read_value)) {
				return TRAP_ERR;
			}
			cpu->regs[di->rd] = sext(read_value, 32);
			break;
		case INSTRUCTION_OP_LBU:
			if (!cpu_read(cpu, cpu->regs[di->rs1] + sext(di->imm, 12), 8, &read_value)) {
				return TRAP_ERR;
			}
			cpu->regs[di->rd] = read_value;
			break;
		case INSTRUCTION_OP_LHU:
			if (!cpu_read(cpu, cpu->regs[di->rs1] + sext(di->imm, 12), 16, &read_value)) {
				return TRAP_ERR;
			}
			cpu->regs[di->rd] = read_value;
			break;
		case INSTRUCTION_OP_SB:
			value = cpu->regs[di->rs2] & 0xFF;
			if (!cpu_write(cpu, cpu->regs[di->rs1] + sext(di->imm, 7), 8, value)) {
				return TRAP_ERR;
			}
			break;
		case INSTRUCTION_OP_SH:
			value = cpu->regs[di->rs2] & 0xFFFF;
			if (!cpu_write(cpu, cpu->regs[di->rs1] + sext(di->imm, 7), 16, value)) {
				return TRAP_ERR;
			}
			break;
		case INSTRUCTION_OP_SW:
			value = cpu->regs[di->rs2];
			if (!cpu_write(cpu, cpu->regs[di->rs1] + sext(di->imm, 7), value, 32)) {
				return TRAP_ERR;
			}
			break;
		case INSTRUCTION_OP_ADDI:
			cpu->regs[di->rd] = cpu->regs[di->rs1] + sext(di->imm, 12);
			break;
		case INSTRUCTION_OP_SLTI:
			cpu->regs[di->rd] = cpu->regs[di->rs1] < sext(di->imm, 12);
			break;
		case INSTRUCTION_OP_SLTIU:
			cpu->regs[di->rd] = cpu->regs[di->rs1] < di->imm;
			break;
		case INSTRUCTION_OP_XORI:
			cpu->regs[di->rd] = cpu->regs[di->rs1] ^ sext(di->imm, 12);
			break;
		case INSTRUCTION_OP_ORI:
			cpu->regs[di->rd] = cpu->regs[di->rs1] | sext(di->imm, 12);
			break;
		case INSTRUCTION_OP_ANDI:
			cpu->regs[di->rd] = cpu->regs[di->rs1] & sext(di->imm, 12);
			break;
		case INSTRUCTION_OP_SLLI:
			cpu->regs[di->rd] = cpu->regs[di->rs1] << di->imm;
			break;
		case INSTRUCTION_OP_SRLI:
			cpu->regs[di->rd] = cpu->regs[di->rs1] >> di->imm;
			break;
		case INSTRUCTION_OP_SRAI:
			cpu->regs[di->rd] = (s_word)cpu->regs[di->rs1] >> di->imm;
			break;
		case INSTRUCTION_OP_ADD:
			tmp_overflow = (dword)cpu->regs[di->rs1] + (dword)cpu->regs[di->rs2];
			cpu->regs[di->rd] = (word)(tmp_overflow & XLEN_MASK);
			break;
		case INSTRUCTION_OP_SUB:
			tmp_overflow = (dword)cpu->regs[di->rs1] - (dword)cpu->regs[di->rs2];
			cpu->regs[di->rd] = (word)(tmp_overflow & XLEN_MASK);
			break;
		case INSTRUCTION_OP_SLL:
			cpu->regs[di->rd] = cpu->regs[di->rs1] << (cpu->regs[di->rs2] & 0x1F);
			break;
		case INSTRUCTION_OP_SLT:
			cpu->regs[di->rd] = (s_word)cpu->regs[di->rs1] < (s_word)cpu->regs[di->rs2];
			break;
		case INSTRUCTION_OP_SLTU:
			cpu->regs[di->rd] = cpu->regs[di->rs1] < cpu->regs[di->rs2];
			break;
		case INSTRUCTION_OP_XOR:
			cpu->regs[di->rd] = cpu->regs[di->rs1] ^ cpu->regs[di->rs2];
			break;
		case INSTRUCTION_OP_SRL:
			cpu->regs[di->rd] = cpu->regs[di->rs1] >> (cpu->regs[di->rs2] & 0x1F);
			break;
		case INSTRUCTION_OP_SRA:
			cpu->regs[di->rd] = (s_word)cpu->regs[di->rs1] >> (cpu->regs[di->rs2] & 0x1F);
			break;
		case INSTRUCTION_OP_OR:
			cpu->regs[di->rd] = cpu->regs[di->rs1] | cpu->regs[di->rs2];
			break;
		case INSTRUCTION_OP_AND:
			cpu->regs[di->rd] = cpu->regs[di->rs1] & cpu->regs[di->rs2];
			break;
		case INSTRUCTION_OP_FENCE:
		case INSTRUCTION_OP_FENCE_I:
		case INSTRUCTION_OP_ECALL:
		case INSTRUCTION_OP_EBREAK:
		case INSTRUCTION_OP_CSRRW:
		case INSTRUCTION_OP_CSRRS:
		case INSTRUCTION_OP_CSRRC:
		case INSTRUCTION_OP_CSRRWI:
		case INSTRUCTION_OP_CSRRSI:
		case INSTRUCTION_OP_CSRRCI:
			fprintf(stderr, "UNIMPLEMENTED: %s\n", rve_instruction_op_to_cstr(di->op));
			trap = TRAP_ERR;
			break;
		default:
			fprintf(stderr, "ERROR: Unknown instruction: %d\n", di->op);
			trap = TRAP_ERR;
			break;
	}
	cpu->regs[0] = 0;
	return trap;
}

word cpu_fetch(cpu_t *cpu)
{
	u64 ins = 0;
	if (!bus_read(cpu->bus, cpu->pc, 32, &ins)) {
		fprintf(stderr, "ERROR: Failed to read instruction from bus at address 0x%08X\n", cpu->pc);
		return 0;
	}

	cpu->pc += 4;
	return (word)ins;
}

// Helper to print CPU state
void cpu_print(const cpu_t *cpu, const char *title)
{
	printf("--- %s ---\n", title);
	printf("PC: 0x%08X\n", cpu->pc);
	for (int i = 0; i < XLEN; i += 4) {
		// clang-format off
        printf("x%-2d (%-4s): %08X  x%-2d (%-4s): %08X  x%-2d (%-4s): %08X  x%-2d (%-4s): %08X\n",
               i,  get_abi_name(i), cpu->regs[i],
               i+1,get_abi_name(i+1),cpu->regs[i+1],
               i+2,get_abi_name(i+2),cpu->regs[i+2],
               i+3,get_abi_name(i+3),cpu->regs[i+3]);
		// clang-format on
	}
	printf("---------------\n");
}
