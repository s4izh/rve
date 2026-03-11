#if 0
#include "rve/emulator.h"
#include "rve/decoder.h"

#include "rve/riscv.h"
#include <stdio.h>
#include <stdlib.h>

#include <stdbool.h>

typedef void (*instruction_callback_t)(uint32_t address, uint32_t instruction);

int process_binary_file(const char *filepath, instruction_callback_t func)
{
	if (!filepath || !func) {
		fprintf(stderr, "Error: filepath o func no pueden ser NULL.\n"); return -1;
	}

	FILE *file = fopen(filepath, "rb");
	if (!file) {
		perror("Error al abrir el fichero");
		return -1;
	}

	uint8_t buffer[4];
	uint32_t instruction = 0;
	uint32_t current_address = 0;
	size_t bytes_read;

	printf("Procesando fichero: %s\n", filepath);

	while ((bytes_read = fread(buffer, 1, 4, file)) == 4) {
		instruction = (uint32_t)buffer[0] | ((uint32_t)buffer[1] << 8) |
			      ((uint32_t)buffer[2] << 16) | ((uint32_t)buffer[3] << 24);

		func(current_address, instruction);
		current_address += 4;
	}

	int result = 0;
	if (ferror(file)) {
		perror("Error durante la lectura del fichero");
		result = -1;
	} else if (!feof(file)) {
		fprintf(stderr,
			"Warning: El tamano del fichero no es multiplo de 4 bytes. "
			"Se ignoraron los ultimos %zu bytes en la direccion 0x%X.\n",
			bytes_read, current_address);
	} else {
		printf("Fin del fichero alcanzado con exito en la direccion 0x%X.\n",
		       current_address);
	}

	if (fclose(file) != 0) {
		perror("Error al cerrar el fichero");
		if (result == 0) {
			result = -1;
		}
	}

	printf("Procesamiento terminado.\n");
	return result;
}

void my_instruction_handler(uint32_t address, uint32_t instruction)
{
	char buffer[32];

	decoded_instruction_t decoded = rve_decode_instruction(instruction);
	if (decoded.valid) {
		rve_decoded_format_to_buffer(&decoded, buffer, sizeof(buffer));

		u32 column_width = 25;

		printf("%-*s Decoded: %s, Format: %s, Imm: 0x%08X, Rd: %d, Rs1: %d, Rs2: %d\n",
		       column_width, buffer, rve_instruction_op_to_cstr(decoded.op),
		       rve_instruction_format_to_cstr(decoded.format), decoded.imm, decoded.rd,
		       decoded.rs1, decoded.rs2);

	} else {
		printf("Invalid instruction: 0x%08X\n", instruction);
	}
}

int main(int argc, char *argv[])
{
#ifdef TESTS
	int failed_tests = run_decoder_tests();

	if (failed_tests > 0) {
		printf("\nDecoder tests finished with errors.\n");
		return EXIT_FAILURE;
	} else {
		printf("\nAll decoder tests passed!\n");
	}

	if (argc != 2) {
		fprintf(stderr, "Uso: %s <fichero_binario>\n", argv[0]);
		return EXIT_FAILURE;
	}

	const char *input_filename = argv[1];

	int status = process_binary_file(input_filename, my_instruction_handler);

	if (status == 0) {
		printf("Fichero procesado correctamente.\n");
		return EXIT_SUCCESS;
	} else {
		fprintf(stderr, "Hubo un error durante el procesamiento del fichero.\n");
		return EXIT_FAILURE;
	}
#endif // TESTS

	return 0;
}

// int main(int argc, char *argv[])
// {
// 	if (argc != 2) {
// 		fprintf(stderr, "Usage: %s <binary>\n", argv[0]);
// 		exit(EXIT_FAILURE);
// 	}

// 	int res;
// 	const char *program_filename = argv[1];

// 	emulator_t emulator;

// 	res = rve_run_binary(&emulator, program_filename);
// 	if (res < 0) {
// 		fprintf(stderr, "Failed to run the binary: %s\n",
// 			program_filename);
// 		exit(EXIT_FAILURE);
// 	}

// 	return EXIT_SUCCESS;
// }
#endif
