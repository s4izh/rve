#include "rve/types.h"
#include "rve/decoder.h"

typedef struct {
    const char* name;         // Nombre descriptivo del caso
    word        instruction;  // Instrucción codificada en 32 bits
    bool        expect_valid; // Si se espera que la decodificación sea válida
    // --- Campos esperados (solo relevantes si expect_valid es true) ---
    instruction_format_t expect_format;
    instruction_op_t     expect_op;
    reg_t                expect_rd;  // Usar un valor comodín (ej. 0xFF) si no aplica
    reg_t                expect_rs1; // Usar un valor comodín (ej. 0xFF) si no aplica
    reg_t                expect_rs2; // Usar un valor comodín (ej. 0xFF) si no aplica
    uint32_t             expect_imm; // Usar un valor comodín si no aplica
} decoder_test_case_t;

#define NA 0xFF // O cualquier valor fuera de rango 0-31 para regs
#define NA_IMM 0xBADBAD // Un valor improbable para inmediato

// --- Casos de Prueba para el Decodificador ---
const decoder_test_case_t decoder_tests[] = {
    // --- U-Type ---
    {"lui x5, 0xABCD0",    0xABCD02B7, true, INSTRUCTION_FORMAT_U, INSTRUCTION_OP_LUI,   5, NA, NA, 0xABCD0000},
    {"auipc x6, 0x1",      0x00001317, true, INSTRUCTION_FORMAT_U, INSTRUCTION_OP_AUIPC, 6, NA, NA, 0x00001000},
    {"auipc x1, -4096",    0xff000097, true, INSTRUCTION_FORMAT_U, INSTRUCTION_OP_AUIPC, 1, NA, NA, 0xFFFFF000},

    // --- J-Type ---
    {"jal x1, +20",        0x014000EF, true, INSTRUCTION_FORMAT_J, INSTRUCTION_OP_JAL,   1, NA, NA, 20}, // 20 = 0x14
    {"jal x0, -32",        0xfe1ff06f, true, INSTRUCTION_FORMAT_J, INSTRUCTION_OP_JAL,   0, NA, NA, -32},// -32 = 0xFFFFFFE0

    // // --- I-Type (JALR) ---
    {"jalr x0, 0(x1)",     0x00008067, true, INSTRUCTION_FORMAT_I, INSTRUCTION_OP_JALR,  0,  1, NA, 0},   // ret típico
    {"jalr x5, 16(x2)",    0x010102E7, true, INSTRUCTION_FORMAT_I, INSTRUCTION_OP_JALR,  5,  2, NA, 16},

	// // --- I-Type (Loads) ---
    {"lw x7, 12(x5)",      0x00C2A383, true, INSTRUCTION_FORMAT_I, INSTRUCTION_OP_LW,    7,  5, NA, 12},
	{"lb x10, -1(x2)",     0xfff10503, true, INSTRUCTION_FORMAT_I, INSTRUCTION_OP_LB,   10,  2, NA, -1},
    {"lhu x3, 1024(x4)",   0x40025183, true, INSTRUCTION_FORMAT_I, INSTRUCTION_OP_LHU,   3,  4, NA, 1024},
    {"lbu x1, 0(x0)",      0x00004083, true, INSTRUCTION_FORMAT_I, INSTRUCTION_OP_LBU,   1,  0, NA, 0},

    // // --- B-Type ---
    {"beq x1, x2, +8",     0x00208463, true, INSTRUCTION_FORMAT_B, INSTRUCTION_OP_BEQ,  NA, 1,  2, 8},
    {"bne x5, x0, -16",    0xfe0298e3, true, INSTRUCTION_FORMAT_B, INSTRUCTION_OP_BNE,  NA, 5,  0, -16},
    {"blt x3, x4, +4",     0x0041C263, true, INSTRUCTION_FORMAT_B, INSTRUCTION_OP_BLT,  NA, 3,  4, 4},
    {"bgeu x6, x7, 0",     0x00737063, true, INSTRUCTION_FORMAT_B, INSTRUCTION_OP_BGEU, NA, 6,  7, 0},

	// // --- S-Type ---
    {"sw x2, 8(x5)",       0x0022a423, true, INSTRUCTION_FORMAT_S, INSTRUCTION_OP_SW,   NA, 5,  2, 8},
    {"sb x0, -4(x6)",      0xfe030e23, true, INSTRUCTION_FORMAT_S, INSTRUCTION_OP_SB,   NA, 6,  0, -4},
    {"sh x15, 60(sp)",     0x02f11e23, true, INSTRUCTION_FORMAT_S, INSTRUCTION_OP_SH,   NA, 2, 15, 60}, // sp=x2

	// // --- I-Type (Op-Imm) ---
    {"addi x5, x6, 42",    0x02A30293, true, INSTRUCTION_FORMAT_I, INSTRUCTION_OP_ADDI,  5,  6, NA, 42},
    {"addi x7, x7, -1",    0xFFF38393, true, INSTRUCTION_FORMAT_I, INSTRUCTION_OP_ADDI,  7,  7, NA, -1},
    {"slti x8, x9, 10",    0x00A4A413, true, INSTRUCTION_FORMAT_I, INSTRUCTION_OP_SLTI,  8,  9, NA, 10},
    {"sltiu x10, x11, 1",  0x0015B513, true, INSTRUCTION_FORMAT_I, INSTRUCTION_OP_SLTIU,10, 11, NA, 1}, // imm=1
    {"xori x12, x13, -1",  0xFFF6C613, true, INSTRUCTION_FORMAT_I, INSTRUCTION_OP_XORI, 12, 13, NA, -1}, // -1 = 0xFFF
    {"ori x14, x15, 0xFF", 0x0FF7E713, true, INSTRUCTION_FORMAT_I, INSTRUCTION_OP_ORI,  14, 15, NA, 0x0FF},
    {"andi x1, x1, 0",     0x0000F093, true, INSTRUCTION_FORMAT_I, INSTRUCTION_OP_ANDI,  1,  1, NA, 0},

    // // --- I-Type (Shifts) --- // shamt está en bits 24:20
    {"slli x5, x6, 5",     0x00531293, true, INSTRUCTION_FORMAT_I, INSTRUCTION_OP_SLLI,  5,  6, NA, 5},  // imm = shamt
    {"srli x7, x8, 31",    0x01F45393, true, INSTRUCTION_FORMAT_I, INSTRUCTION_OP_SRLI,  7,  8, NA, 31}, // imm = shamt
    {"srai x9, x10, 10",   0x40a55493, true, INSTRUCTION_FORMAT_I, INSTRUCTION_OP_SRAI,  9, 10, NA, 10}, // imm = shamt

	// // --- R-Type ---
    {"add x3, x1, x2",     0x002081B3, true, INSTRUCTION_FORMAT_R, INSTRUCTION_OP_ADD,   3,  1,  2, 0},
    {"sub x4, x5, x6",     0x40628233, true, INSTRUCTION_FORMAT_R, INSTRUCTION_OP_SUB,   4,  5,  6, 0},
    {"sll x7, x8, x9",     0x009413B3, true, INSTRUCTION_FORMAT_R, INSTRUCTION_OP_SLL,   7,  8,  9, 0},
    {"slt x10, x1, x0",    0x0000A533, true, INSTRUCTION_FORMAT_R, INSTRUCTION_OP_SLT,  10,  1,  0, 0},
    {"sltu x11, x12, x13", 0x00D635B3, true, INSTRUCTION_FORMAT_R, INSTRUCTION_OP_SLTU, 11, 12, 13, 0},
    {"xor x14, x15, x1",   0x0017C733, true, INSTRUCTION_FORMAT_R, INSTRUCTION_OP_XOR,  14, 15,  1, 0},
    {"srl x2, x3, x4",     0x0041D133, true, INSTRUCTION_FORMAT_R, INSTRUCTION_OP_SRL,   2,  3,  4, 0},
    {"sra x5, x6, x7",     0x407352B3, true, INSTRUCTION_FORMAT_R, INSTRUCTION_OP_SRA,   5,  6,  7, 0},
    {"or x8, x9, x10",     0x00A4E433, true, INSTRUCTION_FORMAT_R, INSTRUCTION_OP_OR,    8,  9, 10, 0},
    {"and x11, x12, x13",  0x00D675B3, true, INSTRUCTION_FORMAT_R, INSTRUCTION_OP_AND,  11, 12, 13, 0},

    // // --- System ---
    {"ecall",              0x00000073, true, INSTRUCTION_FORMAT_I, INSTRUCTION_OP_ECALL, 0, 0, NA, 0}, // rd=0, rs1=0, imm=0
    {"ebreak",             0x00100073, true, INSTRUCTION_FORMAT_I, INSTRUCTION_OP_EBREAK,0, 0, NA, 1}, // rd=0, rs1=0, imm=1

    // // --- Invalid Examples ---
    {"Invalid Opcode",     0xFFFFFFFF, false, INSTRUCTION_FORMAT_UNKNOWN, INSTRUCTION_OP_UNKNOWN, NA, NA, NA, NA_IMM},
    {"JALR with bad funct3",0x00009067, false, INSTRUCTION_FORMAT_UNKNOWN, INSTRUCTION_OP_UNKNOWN, NA, NA, NA, NA_IMM}, // JALR funct3=1
    {"LOAD with bad funct3",0x00C2B383, false, INSTRUCTION_FORMAT_UNKNOWN, INSTRUCTION_OP_UNKNOWN, NA, NA, NA, NA_IMM}, // LW funct3=3
    // Agrega más casos inválidos (funct7 incorrecto para shifts/sub/sra, etc.)
};

#define CHECK(condition, test_passed, failures, ...) \
    if (!(condition)) { \
        printf("    FAIL: "); \
        printf(__VA_ARGS__); \
        printf("\n"); \
        *test_passed = false; \
        *failures = *failures + 1; \
    }

// Función para comparar campos relevantes basado en formato esperado
static void check_decoded_fields(const decoder_test_case_t* test, const decoded_instruction_t* decoded, bool* test_passed, int* failures) {

    CHECK(decoded->format == test->expect_format, test_passed, failures,
          "Format mismatch! Expected: %s, Got: %s", rve_instruction_format_to_cstr(test->expect_format), rve_instruction_format_to_cstr(decoded->format));
    CHECK(decoded->op == test->expect_op, test_passed, failures,
          "Operation mismatch! Expected: %d, Got: %d", test->expect_op, decoded->op);

    // Comprobar campos solo si aplican al formato/operación ESPERADO
    switch(test->expect_format) {
        case INSTRUCTION_FORMAT_R:
            if (test->expect_rd != NA) CHECK(decoded->rd == test->expect_rd, test_passed, failures, "rd mismatch! E:%d G:%d", test->expect_rd, decoded->rd);
            if (test->expect_rs1 != NA) CHECK(decoded->rs1 == test->expect_rs1, test_passed, failures, "rs1 mismatch! E:%d G:%d", test->expect_rs1, decoded->rs1);
            if (test->expect_rs2 != NA) CHECK(decoded->rs2 == test->expect_rs2, test_passed, failures, "rs2 mismatch! E:%d G:%d", test->expect_rs2, decoded->rs2);
            // R-type imm should be 0 implicitly
            CHECK(decoded->imm == 0, test_passed, failures, "R-type imm not 0! G:%d", decoded->imm);
            break;

        case INSTRUCTION_FORMAT_I:
            if (test->expect_rd != NA) CHECK(decoded->rd == test->expect_rd, test_passed, failures, "rd mismatch! E:%d G:%d", test->expect_rd, decoded->rd);
            if (test->expect_rs1 != NA) CHECK(decoded->rs1 == test->expect_rs1, test_passed, failures, "rs1 mismatch! E:%d G:%d", test->expect_rs1, decoded->rs1);
            // rs2 no aplica
            if (test->expect_op != INSTRUCTION_OP_FENCE && test->expect_op != INSTRUCTION_OP_FENCE_I) { // FENCE no usa imm estándar
                if (test->expect_imm != NA_IMM) CHECK(decoded->imm == test->expect_imm, test_passed, failures, "imm mismatch! E:%d G:%d", test->expect_imm, decoded->imm);
            }
             // Caso especial para shifts immediate: imm contiene shamt[4:0]
             if (test->expect_op == INSTRUCTION_OP_SLLI || test->expect_op == INSTRUCTION_OP_SRLI || test->expect_op == INSTRUCTION_OP_SRAI) {
                  CHECK((decoded->imm & 0x1F) == (test->expect_imm & 0x1F), test_passed, failures, "shamt mismatch! E:%d G:%d", (test->expect_imm & 0x1F), (decoded->imm & 0x1F));
             }

            break;

        case INSTRUCTION_FORMAT_S:
        case INSTRUCTION_FORMAT_B: // S y B tienen campos similares pero imm diferente
            // rd no aplica
            if (test->expect_rs1 != NA) CHECK(decoded->rs1 == test->expect_rs1, test_passed, failures, "rs1 mismatch! E:%d G:%d", test->expect_rs1, decoded->rs1);
            if (test->expect_rs2 != NA) CHECK(decoded->rs2 == test->expect_rs2, test_passed, failures, "rs2 mismatch! E:%d G:%d", test->expect_rs2, decoded->rs2);
            if (test->expect_imm != NA_IMM) CHECK(decoded->imm == test->expect_imm, test_passed, failures, "imm mismatch! E:%d G:%d", test->expect_imm, decoded->imm);
            break;

        case INSTRUCTION_FORMAT_U:
        case INSTRUCTION_FORMAT_J: // U y J tienen campos similares
            if (test->expect_rd != NA) CHECK(decoded->rd == test->expect_rd, test_passed, failures, "rd mismatch! E:%d G:%d", test->expect_rd, decoded->rd);
            // rs1, rs2 no aplican
            if (test->expect_imm != NA_IMM) CHECK(decoded->imm == test->expect_imm, test_passed, failures, "imm mismatch! E:%d G:%d", test->expect_imm, decoded->imm);
            break;

        default:
            // No debería llegar aquí si la validez ya se comprobó
            break;
    }
}

// Función principal de testing
int run_decoder_tests() {
    int num_tests = sizeof(decoder_tests) / sizeof(decoder_tests[0]);
    int total_failures = 0;

    printf("--- Running %d Decoder Tests ---\n", num_tests);

    for (int i = 0; i < num_tests; ++i) {
        const decoder_test_case_t* test = &decoder_tests[i];
        printf("Test %-3d: %-20s (0x%08X)...", i + 1, test->name, test->instruction);

        decoded_instruction_t decoded = rve_decode_instruction(test->instruction);
        bool current_test_passed = true;
        int current_test_failures = 0; // Cuenta fallos *dentro* de este test

        if (decoded.valid != test->expect_valid) {
             printf("\n    FAIL: Validity mismatch! Expected: %s, Got: %s\n",
                   test->expect_valid ? "true" : "false", decoded.valid ? "true" : "false");
             current_test_passed = false;
             total_failures++;
        } else {
            // Si la validez coincide...
            if (test->expect_valid) {
                // ...y se esperaba que fuera válida, comprobar campos
                check_decoded_fields(test, &decoded, &current_test_passed, &current_test_failures);
                total_failures += current_test_failures; // Añadir fallos de campos
            } else {
                // ...y se esperaba que fuera inválida, el test pasa
                // (ya comprobamos que decoded.valid == test->expect_valid)
            }
        }

        if (current_test_passed) {
            printf("    PASS\n");
        } else {
             // Imprimir detalles si falló y se esperaba que fuera válido
             if (test->expect_valid) {
                printf("    Decoded: Fmt:%d Op:%d rd:%d rs1:%d rs2:%d imm:%d(0x%X) Valid:%s\n",
                       decoded.format, decoded.op, decoded.rd, decoded.rs1, decoded.rs2,
                       decoded.imm, (uint32_t)decoded.imm, decoded.valid ? "T":"F");
             }
        }
    }

    printf("--- Decoder Test Summary: %d Failures ---\n", total_failures);
    return total_failures; // Devolver número de fallos (0 si todo OK)
}
