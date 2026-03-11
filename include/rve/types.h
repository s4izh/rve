/**
 * @file types.h
 * @brief Fundamental primitive type aliases used throughout rve.
 *
 * Provides short, consistent names for standard integer widths,
 * floating-point types, and the core RISC-V word types.
 */

#ifndef RVE_TYPES_H
#define RVE_TYPES_H

#include <stdint.h>
#include <stdbool.h>

#if defined(__SIZEOF_INT128__)
typedef __int128          i128; /**< Signed 128-bit integer (compiler extension). */
typedef unsigned __int128 u128; /**< Unsigned 128-bit integer (compiler extension). */
#else
#error "128-bit integers are not supported by this compiler"
#endif

/* Signed integer types */
typedef int8_t  i8;   /**< Signed  8-bit integer.  */
typedef int16_t i16;  /**< Signed 16-bit integer.  */
typedef int32_t i32;  /**< Signed 32-bit integer.  */
typedef int64_t i64;  /**< Signed 64-bit integer.  */

/* Unsigned integer types */
typedef uint8_t  u8;  /**< Unsigned  8-bit integer. */
typedef uint16_t u16; /**< Unsigned 16-bit integer. */
typedef uint32_t u32; /**< Unsigned 32-bit integer. */
typedef uint64_t u64; /**< Unsigned 64-bit integer. */

/* Floating-point types */
typedef float  f32;   /**< 32-bit IEEE-754 single precision. */
typedef double f64;   /**< 64-bit IEEE-754 double precision. */

/* RISC-V word types */
typedef uint32_t word;    /**< Unsigned 32-bit machine word (XLEN=32).         */
typedef int32_t  s_word;  /**< Signed 32-bit machine word (arithmetic ops).    */
typedef uint64_t dword;   /**< Unsigned 64-bit double-word (multiply results). */
typedef int64_t  s_dword; /**< Signed 64-bit double-word.                      */

/* Register index aliases */
typedef uint8_t reg_t;   /**< Register file index (0..31).         */
typedef uint8_t funct_t; /**< Instruction funct3/funct7 field.     */

/** @brief Suppress unused-variable warnings for intentionally unused symbols. */
#define UNUSED(x) (void)(x)

#endif /* RVE_TYPES_H */
