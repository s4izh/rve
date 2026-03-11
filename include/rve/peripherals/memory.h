/**
 * @file memory.h
 * @brief Flat RAM peripheral.
 *
 * Implements the @ref peripheral_ops_t interface for a contiguous byte-array
 * memory region.  Little-endian byte order is assumed.
 */

#ifndef RVE_MEMORY_H
#define RVE_MEMORY_H

#include "rve/types.h"
#include "rve/peripherals/peripheral.h"

#include <stdlib.h>

/** @brief Default memory region base address. */
#define RVE_MEMORY_ADDR_START 0x00000000
/** @brief Default memory region end address (exclusive). */
#define RVE_MEMORY_ADDR_END   0x00000400
/** @brief Default memory region size in bytes. */
#define RVE_MEMORY_SIZE       (RVE_MEMORY_ADDR_END - RVE_MEMORY_ADDR_START)

typedef struct memory_t memory_t;

/**
 * @brief Memory peripheral context.
 */
struct memory_t {
    u8    *raw_memory; /**< Backing byte array (heap-allocated by memory_init). */
    size_t size;       /**< Size of the backing array in bytes.                 */
    word   base_addr;  /**< Bus address that maps to raw_memory[0].             */
};

/* Peripheral-compatible callbacks (match peripheral_ops_t signatures) */
bool memory_init  (void *ctx); /**< Allocate raw_memory via calloc.             */
bool memory_deinit(void *ctx); /**< Free raw_memory.                            */

/**
 * @brief Bus read callback -- copy @p size_bits bytes from @p addr.
 * @see peripheral_read_fn
 */
bool memory_read (void *ctx, word addr, u8 size_bits, word *result);

/**
 * @brief Bus write callback -- copy @p size_bits bytes to @p addr.
 * @see peripheral_write_fn
 */
bool memory_write(void *ctx, word addr, word data, u8 size_bits);

/**
 * @brief Write a 32-bit instruction word directly (test/loader helper).
 * @param m     Memory context.
 * @param addr  Destination address.
 * @param instruction  Value to write.
 */
void memory_load_instruction(memory_t *m, word addr, word instruction);

/**
 * @brief Allocate a memory context without initialising raw_memory.
 *
 * raw_memory is NULL until @ref memory_init is called.
 *
 * @param base_addr  Base bus address.
 * @param size       Desired capacity in bytes.
 * @return           Newly allocated context, or NULL on OOM.
 */
memory_t *memory_create_ctx(word base_addr, size_t size);

/** @brief Zero the backing array (does not free/reallocate). */
void memory_flush(memory_t *m);

/**
 * @brief Copy @p program_size bytes from @p program_data into the backing array.
 *
 * @param m             Memory context (must be initialised).
 * @param addr          Destination bus address.
 * @param program_data  Source buffer.
 * @param program_size  Number of bytes to copy.
 * @return              0 on success, -1 if address is out of range.
 */
int memory_load_file(memory_t *m, word addr, char *program_data, size_t program_size);

/** @brief Exported peripheral operations table for bus registration. */
extern peripheral_ops_t memory_ops;

#endif /* RVE_MEMORY_H */
