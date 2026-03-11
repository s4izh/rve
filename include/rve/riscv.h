#ifndef RISCV_OPCODES_H
#define RISCV_OPCODES_H

// Opcodes para la especificacion RISC-V RV32I Base Integer Instruction Set
// Fuente: RISC-V Unprivileged ISA Specification (Volumen I)
// Nota: Estos son solo los 7 bits del opcode principal (bits 6:0).
//       La decodificacion completa requiere funct3 y/o funct7 en muchos casos.

// === Formato U (Upper Immediate) ===
// Usado para cargar inmediatos grandes en los bits superiores de un registro.
#define RISCV_OPCODE_LUI      0x37 // Load Upper Immediate (lui rd, imm)
#define RISCV_OPCODE_AUIPC    0x17 // Add Upper Immediate to PC (auipc rd, imm)

// === Formato J (Jump) ===
// Usado para saltos incondicionales.
#define RISCV_OPCODE_JAL      0x6F // Jump and Link (jal rd, imm)

// === Formato I (Immediate) ===
// Usados para operaciones con inmediatos cortos, cargas, y JALR.
#define RISCV_OPCODE_JALR     0x67 // Jump and Link Register (jalr rd, rs1, imm)
#define RISCV_OPCODE_LOAD     0x03 // Loads (lb, lh, lw, lbu, lhu rd, rs1, imm)
#define RISCV_OPCODE_OP_IMM   0x13 // Operaciones Aritmetico/Logicas con Inmediato
                             // (addi, slti, sltiu, xori, ori, andi, slli, srli, srai rd, rs1, imm)
#define RISCV_OPCODE_MISC_MEM 0x0F // Memory Synchronization (fence, fence.i)
                             // Aunque 'fence' tiene formato propio, comparte bits con I y
                             // a veces se agrupa aqui en la decodificacion inicial.
#define RISCV_OPCODE_SYSTEM   0x73 // Llamadas al sistema / CSRs
                             // (ecall, ebreak, csrrw, csrrs, csrrc, csrrwi, csrrsi, csrrci)
                             // Incluye instrucciones de la extension Zicsr,
                             // que es necesaria para el manejo basico del sistema.


// === Formato B (Branch) ===
// Usado para saltos condicionales.
#define RISCV_OPCODE_BRANCH   0x63 // Branches (beq, bne, blt, bge, bltu, bgeu rs1, rs2, imm)

// === Formato S (Store) ===
// Usado para almacenar datos en memoria.
#define RISCV_OPCODE_STORE    0x23 // Stores (sb, sh, sw rs1, rs2, imm)

// === Formato R (Register) ===
// Usado para operaciones Aritmetico/Logicas entre registros.
#define RISCV_OPCODE_OP       0x33 // Operaciones Aritmetico/Logicas Registro-Registro
                             // (add, sub, slt, sltu, xor, or, and, sll, srl, sra rd, rs1, rs2)

// === Para RISCV_OPCODE_LOAD (0x03) - Formato I ===
// funct3 distingue el tipo de carga
#define RISCV_FUNCT3_LB   0x0 // Load Byte
#define RISCV_FUNCT3_LH   0x1 // Load Halfword
#define RISCV_FUNCT3_LW   0x2 // Load Word
#define RISCV_FUNCT3_LBU  0x4 // Load Byte Unsigned
#define RISCV_FUNCT3_LHU  0x5 // Load Halfword Unsigned

// === Para RISCV_OPCODE_MISC_MEM (0x0F) - Formato I ===
// funct3 distingue FENCE de FENCE.I
#define RISCV_FUNCT3_FENCE    0x0 // FENCE instruction
#define RISCV_FUNCT3_FENCE_I  0x1 // FENCE.I instruction

// === Para RISCV_OPCODE_OP_IMM (0x13) - Formato I ===
// funct3 distingue la operacion aritmetica/logica inmediata
#define RISCV_FUNCT3_ADDI   0x0 // Add Immediate
#define RISCV_FUNCT3_SLTI   0x2 // Set Less Than Immediate
#define RISCV_FUNCT3_SLTIU  0x3 // Set Less Than Immediate Unsigned
#define RISCV_FUNCT3_XORI   0x4 // XOR Immediate
#define RISCV_FUNCT3_ORI    0x6 // OR Immediate
#define RISCV_FUNCT3_ANDI   0x7 // AND Immediate
// Para SLLI, SRLI, SRAI, funct3 es especifico:
#define RISCV_FUNCT3_SLLI   0x1 // Shift Left Logical Immediate
#define RISCV_FUNCT3_SRLI_SRAI 0x5 // Shift Right Logical/Arithmetic Immediate

// --- funct7 especifico para OP_IMM (bits 31:25 dentro del inmediato) ---
// Para SLLI, SRLI, SRAI, el bit mas significativo de funct7 distingue SRLI de SRAI
// El resto de bits de funct7 deben ser 0 para RV32I.
#define RISCV_FUNCT7_SLLI   0x00 // Identificador para SLLI (junto con opcode y funct3)
#define RISCV_FUNCT7_SRLI   0x00 // Identificador para SRLI (junto con opcode y funct3)
#define RISCV_FUNCT7_SRAI   0x20 // Identificador para SRAI (junto con opcode y funct3) - Bit 30 esta a 1

// === Para RISCV_OPCODE_AUIPC (0x17) - Formato U ===
// No usa funct3 ni funct7 para distinguir (opcode es suficiente)

// === Para RISCV_OPCODE_STORE (0x23) - Formato S ===
// funct3 distingue el tipo de almacenamiento
#define RISCV_FUNCT3_SB   0x0 // Store Byte
#define RISCV_FUNCT3_SH   0x1 // Store Halfword
#define RISCV_FUNCT3_SW   0x2 // Store Word

// === Para RISCV_OPCODE_OP (0x33) - Formato R ===
// funct3 y funct7 juntos distinguen la operacion registro-registro
#define RISCV_FUNCT3_ADD_SUB  0x0 // Add / Subtract
#define RISCV_FUNCT3_SLL      0x1 // Shift Left Logical
#define RISCV_FUNCT3_SLT      0x2 // Set Less Than
#define RISCV_FUNCT3_SLTU     0x3 // Set Less Than Unsigned
#define RISCV_FUNCT3_XOR      0x4 // XOR
#define RISCV_FUNCT3_SRL_SRA  0x5 // Shift Right Logical / Arithmetic
#define RISCV_FUNCT3_OR       0x6 // OR
#define RISCV_FUNCT3_AND      0x7 // AND

// --- funct7 especifico para OP (bits 31:25) ---
// Solo se necesita para diferenciar ADD/SUB y SRL/SRA (y extensiones como M)
#define RISCV_FUNCT7_ADD      0x00 // Para ADD (con funct3 = 0x0)
#define RISCV_FUNCT7_SUB      0x20 // Para SUB (con funct3 = 0x0) - Bit 30 esta a 1
#define RISCV_FUNCT7_SLL      0x00 // Para SLL (con funct3 = 0x1)
#define RISCV_FUNCT7_SLT      0x00 // Para SLT (con funct3 = 0x2)
#define RISCV_FUNCT7_SLTU     0x00 // Para SLTU (con funct3 = 0x3)
#define RISCV_FUNCT7_XOR      0x00 // Para XOR (con funct3 = 0x4)
#define RISCV_FUNCT7_SRL      0x00 // Para SRL (con funct3 = 0x5)
#define RISCV_FUNCT7_SRA      0x20 // Para SRA (con funct3 = 0x5) - Bit 30 esta a 1
#define RISCV_FUNCT7_OR       0x00 // Para OR  (con funct3 = 0x6)
#define RISCV_FUNCT7_AND      0x00 // Para AND (con funct3 = 0x7)

#define RISCV_FUNCT7_MULDIV   0x01 // Para la extension M

#define RISCV_FUNCT3_MUL      0x0
#define RISCV_FUNCT3_MULH     0x1
#define RISCV_FUNCT3_MULHSU   0x2
#define RISCV_FUNCT3_MULHU    0x3
#define RISCV_FUNCT3_DIV      0x4
#define RISCV_FUNCT3_DIVU     0x5
#define RISCV_FUNCT3_REM      0x6
#define RISCV_FUNCT3_REMU     0x7

// === Para RISCV_OPCODE_LUI (0x37) - Formato U ===
// No usa funct3 ni funct7 para distinguir (opcode es suficiente)

// === Para RISCV_OPCODE_BRANCH (0x63) - Formato B ===
// funct3 distingue el tipo de salto condicional
#define RISCV_FUNCT3_BEQ    0x0 // Branch if Equal
#define RISCV_FUNCT3_BNE    0x1 // Branch if Not Equal
#define RISCV_FUNCT3_BLT    0x4 // Branch if Less Than
#define RISCV_FUNCT3_BGE    0x5 // Branch if Greater or Equal
#define RISCV_FUNCT3_BLTU   0x6 // Branch if Less Than Unsigned
#define RISCV_FUNCT3_BGEU   0x7 // Branch if Greater or Equal Unsigned

// === Para RISCV_OPCODE_JALR (0x67) - Formato I ===
// funct3 es 0 para JALR en RV32I
#define RISCV_FUNCT3_JALR   0x0 // Jump and Link Register

// === Para RISCV_OPCODE_JAL (0x6F) - Formato J ===
// No usa funct3 ni funct7 para distinguir (opcode es suficiente)

// === Para RISCV_OPCODE_SYSTEM (0x73) - Formato I (con variaciones) ===
// funct3 distingue entre CSRs, ECALL/EBREAK, etc.
#define RISCV_FUNCT3_PRIV   0x0 // ECALL, EBREAK, y otras instrucciones privilegiadas (WFI, MRET, SRET, etc.)
#define RISCV_FUNCT3_CSRRW  0x1 // Atomic Read/Write CSR
#define RISCV_FUNCT3_CSRRS  0x2 // Atomic Read and Set Bits in CSR
#define RISCV_FUNCT3_CSRRC  0x3 // Atomic Read and Clear Bits in CSR
#define RISCV_FUNCT3_CSRRWI 0x5 // Atomic Read/Write CSR Immediate
#define RISCV_FUNCT3_CSRRSI 0x6 // Atomic Read and Set Bits in CSR Immediate
#define RISCV_FUNCT3_CSRRCI 0x7 // Atomic Read and Clear Bits in CSR Immediate

// --- Valores en imm[11:0] / funct12 para distinguir ECALL/EBREAK (cuando funct3=0) ---
#define RISCV_FUNCT12_ECALL  0x000 // Environment Call
#define RISCV_FUNCT12_EBREAK 0x001 // Environment Breakpoint
#define RISCV_FUNCT12_MRET   0x302

#endif // RISCV_OPCODES_H
