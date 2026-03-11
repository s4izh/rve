#ifndef RVE_COSIM_H
#define RVE_COSIM_H

#include "rve/soc.h"
#include "rve/hart.h"
#include "rve/types.h"

#include <stdbool.h>
#include <stdint.h>

// ---------------------------------------------------------------------------
// Cosim interface
//
// A thin comparison layer that sits between soc_step() and your DPI bindings.
// The SV side reports what the RTL retired; this layer steps the reference
// model and compares.
//
// This is entirely separate from emulator.h. The SoC doesn't know it exists.
//
// Typical DPI usage:
//
//   import "DPI-C" function chandle cosim_create(input int reset_pc);
//   import "DPI-C" function void    cosim_destroy(input chandle h);
//   import "DPI-C" function int     cosim_step(
//       input chandle h,
//       input int  rtl_pc,
//       input int  rtl_next_pc,
//       input bit  rtl_rd_written,
//       input byte rtl_rd,
//       input int  rtl_rd_value,
//       input bit  rtl_mem_write,
//       input int  rtl_mem_addr,
//       input int  rtl_mem_data,
//       input byte rtl_mem_size,
//       input bit  irq_timer,
//       input bit  irq_external,
//       input bit  irq_software
//   );
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// RTL retirement record  -- what the hardware side reports each cycle
// ---------------------------------------------------------------------------

typedef struct {
    word    pc;
    word    next_pc;

    bool    rd_written;
    reg_t   rd;
    word    rd_value;

    bool    mem_write;
    word    mem_addr;
    word    mem_data;
    u8      mem_size;        // bits: 8, 16, 32

    trap_t  trap;
} rtl_retire_t;

// ---------------------------------------------------------------------------
// Interrupt inputs  -- driven by RTL interrupt lines each cycle
// ---------------------------------------------------------------------------

typedef struct {
    bool timer;
    bool external;
    bool software;
} irq_lines_t;

// ---------------------------------------------------------------------------
// Comparison result
// ---------------------------------------------------------------------------

#define COSIM_MM_PC        (1u << 0)
#define COSIM_MM_NEXT_PC   (1u << 1)
#define COSIM_MM_RD        (1u << 2)
#define COSIM_MM_RD_VALUE  (1u << 3)
#define COSIM_MM_MEM_ADDR  (1u << 4)
#define COSIM_MM_MEM_DATA  (1u << 5)
#define COSIM_MM_MEM_SIZE  (1u << 6)
#define COSIM_MM_TRAP      (1u << 7)

typedef struct {
    bool            match;
    uint32_t        mismatch_flags;  // bitmask of COSIM_MM_* above
    hart_retire_t   expected;        // what the reference model produced
    rtl_retire_t    actual;          // what the RTL reported
    uint64_t        step;            // instruction count at mismatch
} cosim_result_t;

// ---------------------------------------------------------------------------
// Cosim context
// ---------------------------------------------------------------------------

typedef struct {
    soc_t       soc;
    uint64_t    step_count;

    // Optional mismatch callback -- fired on every mismatch before cosim_step
    // returns. If NULL, mismatches are printed to stderr.
    void (*on_mismatch)(const cosim_result_t *result, void *userdata);
    void *mismatch_userdata;
} cosim_t;

// ---------------------------------------------------------------------------
// API
// ---------------------------------------------------------------------------

void cosim_init(cosim_t *c, word reset_pc);

// Set a custom mismatch handler (replaces default stderr print).
void cosim_set_mismatch_handler(cosim_t *c,
                                void (*fn)(const cosim_result_t *, void *),
                                void *userdata);

// Load a binary so the reference model has the same memory image as the RTL.
int cosim_load_binary(cosim_t *c, const char *path, word load_addr);

// Step the reference model one instruction and compare against rtl.
// irq reflects the interrupt lines the RTL is asserting this cycle.
// Returns the comparison result. result.match == true means agreement.
cosim_result_t cosim_step(cosim_t *c,
                          const rtl_retire_t *rtl,
                          irq_lines_t irq);

// Print a human-readable diff of a mismatch result.
void cosim_print_mismatch(const cosim_result_t *result);

// ---------------------------------------------------------------------------
// DPI entry points  -- compile dpi.c into your shared library
// These have C linkage and use only plain C types so SV can import them.
// ---------------------------------------------------------------------------

#ifdef RVE_BUILD_DPI
void *cosim_dpi_create (uint32_t reset_pc);
void  cosim_dpi_destroy(void *handle);
int   cosim_dpi_step   (void    *handle,
                        uint32_t rtl_pc,
                        uint32_t rtl_next_pc,
                        uint8_t  rtl_rd_written,
                        uint8_t  rtl_rd,
                        uint32_t rtl_rd_value,
                        uint8_t  rtl_mem_write,
                        uint32_t rtl_mem_addr,
                        uint32_t rtl_mem_data,
                        uint8_t  rtl_mem_size,
                        uint8_t  irq_timer,
                        uint8_t  irq_external,
                        uint8_t  irq_software);
#endif // RVE_BUILD_DPI

#endif // RVE_COSIM_H
