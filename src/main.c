#include "rve/emulator.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <binary>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	int res;
	const char *program_filename = argv[1];

	emulator_t emulator;

	res = rve_run_binary(&emulator, program_filename);
	if (res < 0) {
		fprintf(stderr, "Failed to run the binary: %s\n",
			program_filename);
		exit(EXIT_FAILURE);
	}

	return EXIT_SUCCESS;
}
