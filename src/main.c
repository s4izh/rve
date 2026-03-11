#include "rve/emulator.h"
#include "rve/hart.h"
#include "rve/decoder.h"
#include "rve/types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Trace callback
//
// Called after every retired instruction when --trace is passed.
// Prints one line per instruction in the format:
//
//   [  1234] 0x00000010  addi     sp, sp, -16       rd=x2  0x7FFFFF00
//   [  1235] 0x00000014  sw       ra, 12(sp)        mem[0x7FFFFF0C]=0x00010234
//   [  1236] 0x00000018  jal      ra, 40            -> 0x00000040
// ---------------------------------------------------------------------------

static void trace_callback(const hart_retire_t *r, void *userdata)
{
    uint64_t *cycle = (uint64_t *)userdata;

    char asm_buf[64];
    rve_decoded_format_to_buffer(&r->di, asm_buf, sizeof(asm_buf));

    // Cycle count + PC + disassembly
    printf("[%6llu] 0x%08X  %-32s", (unsigned long long)*cycle, r->pc, asm_buf);

    // Append the most interesting side-effect
    if (r->trap != TRAP_NONE) {
        printf("  TRAP(%d)", r->trap);
    } else if (r->mem_write) {
        printf("  mem[0x%08X] <- 0x%08X (%ub)",
               r->mem_write_addr, r->mem_write_value, r->mem_write_size);
    } else if (r->mem_read) {
        printf("  mem[0x%08X] -> 0x%08X (%ub)",
               r->mem_read_addr, r->mem_read_value, r->mem_read_size);
    } else if (r->rd_written) {
        printf("  %s <- 0x%08X", get_abi_name(r->rd), r->rd_value);
    } else if (r->next_pc != r->pc + 4) {
        // Control-flow change not already captured above (e.g. JAL to x0)
        printf("  -> 0x%08X", r->next_pc);
    }

    printf("\n");
    (*cycle)++;
}

// ---------------------------------------------------------------------------
// Usage
// ---------------------------------------------------------------------------

static void print_usage(const char *argv0)
{
    fprintf(stderr,
        "Usage: %s [options] <binary>\n"
        "\n"
        "Options:\n"
        "  --trace          Print one line per retired instruction\n"
        "  --regs           Dump register file on exit\n"
        "  --pc <hex>       Set reset PC (default: 0x00000000)\n"
        "  --load <hex>     Load binary at this address (default: 0x00000000)\n"
        "  --max <n>        Stop after n instructions\n"
        "  --help           Show this message\n"
        "\n"
        "The binary is a flat raw image (not ELF). It is loaded verbatim\n"
        "into the SoC's address space starting at --load.\n",
        argv0);
}

// ---------------------------------------------------------------------------
// Argument parsing
// ---------------------------------------------------------------------------

typedef struct {
    const char *binary;
    word        reset_pc;
    word        load_addr;
    uint64_t    max_steps;
    bool        trace;
    bool        dump_regs;
} args_t;

static bool parse_args(int argc, char *argv[], args_t *out)
{
    memset(out, 0, sizeof(*out));
    out->reset_pc  = 0x00000000;
    out->load_addr = 0x00000000;
    out->max_steps = 0;
    out->trace     = false;
    out->dump_regs = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            return false;
        } else if (strcmp(argv[i], "--trace") == 0) {
            out->trace = true;
        } else if (strcmp(argv[i], "--regs") == 0) {
            out->dump_regs = true;
        } else if (strcmp(argv[i], "--pc") == 0) {
            if (++i >= argc) { fprintf(stderr, "--pc requires a value\n"); return false; }
            out->reset_pc = (word)strtoul(argv[i], NULL, 16);
        } else if (strcmp(argv[i], "--load") == 0) {
            if (++i >= argc) { fprintf(stderr, "--load requires a value\n"); return false; }
            out->load_addr = (word)strtoul(argv[i], NULL, 16);
        } else if (strcmp(argv[i], "--max") == 0) {
            if (++i >= argc) { fprintf(stderr, "--max requires a value\n"); return false; }
            out->max_steps = (uint64_t)strtoull(argv[i], NULL, 10);
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            return false;
        } else {
            if (out->binary) {
                fprintf(stderr, "Unexpected argument: %s\n", argv[i]);
                return false;
            }
            out->binary = argv[i];
        }
    }

    if (!out->binary) {
        fprintf(stderr, "No binary specified.\n");
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    args_t args;
    if (!parse_args(argc, argv, &args)) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    emulator_t emu;
    emulator_init(&emu, args.reset_pc);

	u64 cycle_count = 0;
    if (args.trace)
        emulator_set_trace(&emu, trace_callback, (void*)&cycle_count);

    if (emulator_load_binary(&emu, args.binary, args.load_addr) != 0)
        return EXIT_FAILURE;

    fprintf(stderr, "rve: loaded '%s' at 0x%08X, reset PC=0x%08X\n",
            args.binary, args.load_addr, args.reset_pc);

    int exit_code = emulator_run(&emu, args.max_steps);

    fprintf(stderr, "rve: halted after %llu instructions, exit code %d\n",
            (unsigned long long)emu.cycle_count, exit_code);

    if (args.dump_regs)
        hart_print(&emu.soc.hart, "Final register state");

    return exit_code == 0 ? EXIT_SUCCESS : exit_code;
}
