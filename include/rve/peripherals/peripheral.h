/**
 * @file peripheral.h
 * @brief Generic peripheral interface for the address-mapped bus.
 *
 * A peripheral is a context pointer plus a @ref peripheral_ops_t vtable.
 * Any device (memory, UART, TOHOST, ...) implements this interface and
 * registers itself on the @ref bus_t to receive read/write calls.
 */

#ifndef RVE_PERIPHERAL_H
#define RVE_PERIPHERAL_H

#include "rve/types.h"

typedef struct peripheral_t   peripheral_t;
typedef struct peripheral_ops_t peripheral_ops_t;

/** @brief Initialise peripheral state.  Called once after registration.      */
typedef bool (*peripheral_init_fn)  (void *ctx);
/** @brief Tear down peripheral state. Called before freeing the context.     */
typedef bool (*peripheral_deinit_fn)(void *ctx);
/** @brief Read @p size_bits from @p addr into @p result.                     */
typedef bool (*peripheral_read_fn)  (void *ctx, word addr, u8 size_bits, word *result);
/** @brief Write @p data (@p size_bits wide) to @p addr.                      */
typedef bool (*peripheral_write_fn) (void *ctx, word addr, word data, u8 size_bits);

/**
 * @brief Peripheral operations table (vtable).
 *
 * Any pointer may be NULL if the operation is not supported.
 */
struct peripheral_ops_t {
    peripheral_init_fn    init;   /**< Optional: initialise state.              */
    peripheral_deinit_fn  deinit; /**< Optional: tear down state.               */
    peripheral_read_fn    read;   /**< Optional: handle a bus read.             */
    peripheral_write_fn   write;  /**< Optional: handle a bus write.            */
    /** @brief Optional: advance internal state by @p cycles_elapsed.          */
    void (*tick)(void *ctx, u64 cycles_elapsed);
};

/** @brief Maximum length of a peripheral name string including NUL. */
#define PERIPHERAL_NAME_MAX 32

/**
 * @brief A single registered peripheral on the bus.
 */
struct peripheral_t {
    bool active;                      /**< True when this slot is in use.       */
    char name[PERIPHERAL_NAME_MAX];   /**< Human-readable name.                 */
    word addr_start;                  /**< First owned address (inclusive).     */
    word addr_end;                    /**< First address beyond range (exclusive). */
    void            *ctx;             /**< Peripheral-private context pointer.  */
    peripheral_ops_t *ops;            /**< Operations vtable.                   */
    void            *config;          /**< Reserved for future configuration.   */
};

/**
 * @brief Heap-allocate and partially initialise a peripheral.
 *
 * The returned struct is not yet registered on any bus.  Prefer using
 * @ref bus_add_peripheral directly unless you need heap ownership.
 *
 * @param addr_start  Start of address range.
 * @param addr_end    End of address range (exclusive).
 * @param ctx         Peripheral context pointer.
 * @param ops         Operations vtable.
 * @param name        Human-readable name (truncated to PERIPHERAL_NAME_MAX-1).
 * @return            Newly allocated @ref peripheral_t; never NULL (asserts).
 */
peripheral_t *peripheral_init(word addr_start, word addr_end,
                              void *ctx, peripheral_ops_t *ops,
                              const char *name);

/**
 * @brief Print a one-line debug summary of @p p to stdout.
 * @param p  Peripheral to describe.
 */
void peripheral_debug(peripheral_t *p);

#endif /* RVE_PERIPHERAL_H */
