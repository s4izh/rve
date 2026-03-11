/**
 * @file utils.c
 * @brief Miscellaneous utility helpers.
 */

#include "rve/utils.h"
#include <stdio.h>

int rve_file_to_string(const char *filename, char **buffer, size_t *size)
{
	FILE *file = fopen(filename, "rb");
	if (file == NULL) {
		perror("Error opening file");
		return -1;
	}

	fseek(file, 0, SEEK_END);
	*size = ftell(file);
	fseek(file, 0, SEEK_SET);

	*buffer = (char *)malloc(*size + 1);
	if (buffer == NULL) {
		perror("Memory allocation failed");
		fclose(file);
		return -1;
	}

	fread(*buffer, 1, *size, file);
	(*buffer)[*size] = '\0';

	fclose(file);

	return 0;
}
