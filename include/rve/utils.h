/**
 * @file utils.h
 * @brief Miscellaneous utility helpers.
 */

#ifndef RVE_UTILS_H
#define RVE_UTILS_H

#include <stdlib.h>

/**
 * @brief Read a file into a newly allocated buffer.
 *
 * The buffer is NUL-terminated (one extra byte is allocated beyond @p size).
 * The caller is responsible for freeing *buffer.
 *
 * @param filename  Path to the file to open.
 * @param buffer    Receives a pointer to the allocated buffer on success.
 * @param size      Receives the file size in bytes (not including the NUL).
 * @return          0 on success, -1 on error (errno set).
 */
int rve_file_to_string(const char *filename, char **buffer, size_t *size);

#endif /* RVE_UTILS_H */
