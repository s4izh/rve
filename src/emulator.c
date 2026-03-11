/**
 * @file emulator.c
 * @brief High-level emulator harness implementation.
 */

#include "rve/emulator.h"
#include "rve/soc.h"
#include "rve/hart.h"
#include "rve/peripherals/tohost.h"
#include "rve/peripherals/uart.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Built-in Linux ABI syscall handler.
 *
 * Handles the minimal set needed by bare-metal C programs compiled with a
 * RISC-V gcc toolchain:
 *  - a7 = 93 (exit)  -> halt, exit_code = a0
 *  - a7 = 64 (write) -> write a2 bytes from guest address a1 to fd a0
 *                       (only stdout / stderr are supported)
 *
 * @return true if the syscall was handled and execution should continue;@n
 *         false to halt (includes the exit syscall).
 */
static bool default_syscall_handler(soc_t *soc, hart_retire_t *retire, void *userdata)
{
    (void)userdata;
    (void)retire;

    word a7 = soc->hart.gpr[17];
    word a0 = soc->hart.gpr[10];
    word a1 = soc->hart.gpr[11];
    word a2 = soc->hart.gpr[12];

    switch (a7) {
        case 93: // exit
            soc->hart.gpr[10] = a0;
            return false;

        case 64: { // write(fd, buf, count)
            FILE *f = (a0 == 1) ? stdout : (a0 == 2) ? stderr : NULL;
            if (!f) { soc->hart.gpr[10] = (word)-1; return true; }
            word written = 0;
            for (word i = 0; i < a2; i++) {
                word byte_val = 0;
                if (!bus_read(&soc->bus, a1 + i, 8, &byte_val)) break;
                fputc((int)(byte_val & 0xFF), f);
                written++;
            }
            soc->hart.gpr[10] = written;
            return true;
        }

        default:
            fprintf(stderr, "EMULATOR: Unhandled syscall a7=%u at PC 0x%08X\n",
                    a7, retire->pc);
            return false;
    }
}

void emulator_init(emulator_t *e, word reset_pc)
{
    memset(e, 0, sizeof(*e));
    soc_init(&e->soc, reset_pc);

    peripheral_tohost_t *tohost = malloc(sizeof(peripheral_tohost_t));
    tohost->halt_flag = &e->halted;
    tohost->exit_code = &e->exit_code;
    soc_add_peripheral(&e->soc, 0x80001000, 0x80001008,
                       tohost, &peripheral_tohost_ops, "TOHOST");

    soc_add_memory(&e->soc, 0x00000000,  4u * 1024u * 1024u);
    soc_add_memory(&e->soc, 0x80000000, 16u * 1024u * 1024u);

	uart_ns16550a_t *uart = uart_ns16550a_create(stdout, NULL);
	soc_add_peripheral(&e->soc, 0x10000000, 0x10000008,
					uart, &uart_ns16550a_ops, "UART0");

    e->syscall_handler  = default_syscall_handler;
    e->syscall_userdata = NULL;
    e->trace_fn         = NULL;
    e->trace_userdata   = NULL;
    e->halted           = false;
    e->exit_code        = 0;
    e->cycle_count      = 0;
}

int emulator_load_binary(emulator_t *e, const char *path, word load_addr)
{
    FILE *f = fopen(path, "rb");
    if (!f) { perror("EMULATOR: Cannot open binary"); return -1; }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) {
        fprintf(stderr, "EMULATOR: Empty or unreadable file: %s\n", path);
        fclose(f);
        return -1;
    }

    char *buf = malloc((size_t)size);
    if (!buf) {
        fprintf(stderr, "EMULATOR: OOM loading binary\n");
        fclose(f);
        return -1;
    }

    if ((long)fread(buf, 1, (size_t)size, f) != size) {
        perror("EMULATOR: Read error");
        free(buf);
        fclose(f);
        return -1;
    }
    fclose(f);

    int rc = soc_load_binary(&e->soc, buf, (size_t)size, load_addr);
    free(buf);

    if (rc != 0)
        fprintf(stderr, "EMULATOR: Failed to load binary at 0x%08X\n", load_addr);
    return rc;
}

void emulator_set_syscall_handler(emulator_t *e, syscall_handler_t fn, void *userdata)
{
    e->syscall_handler  = fn;
    e->syscall_userdata = userdata;
}

void emulator_set_trace(emulator_t *e, trace_fn_t fn, void *userdata)
{
    e->trace_fn       = fn;
    e->trace_userdata = userdata;
}

hart_retire_t emulator_step(emulator_t *e)
{
    if (e->halted) {
        hart_retire_t dummy = {0};
        dummy.trap = TRAP_ILLEGAL_INSTR;
        return dummy;
    }

    hart_retire_t r = soc_step(&e->soc);
    e->cycle_count++;

    if (e->trace_fn)
        e->trace_fn(&r, e->trace_userdata);

    // ECALL -- policy decision, not hardware
    if (r.trap == TRAP_ECALL_M) {
        bool handled = e->syscall_handler
                       && e->syscall_handler(&e->soc, &r, e->syscall_userdata);

        if (!handled) {
            e->halted    = true;
            e->exit_code = (int)e->soc.hart.gpr[10]; // a0
        }
        if (handled) r.trap = TRAP_NONE;
        return r;
    }

    if (r.trap != TRAP_NONE) {
        fprintf(stderr, "[%6llu] TRAP: %s at PC 0x%08X  "
                        "mcause=0x%08X  mtval=0x%08X\n",
                (unsigned long long)e->cycle_count,
                rve_trap_to_cstr(r.trap), r.pc,
                e->soc.hart.csr.mcause,
                e->soc.hart.csr.mtval);
        /* The hardware trap handler takes over; execution continues. */
    }

    return r;
}

int emulator_run(emulator_t *e, uint64_t max_steps)
{
    while (!e->halted) {
        emulator_step(e);
        if (max_steps && e->cycle_count >= max_steps) {
            fprintf(stderr, "EMULATOR: max_steps (%llu) reached\n",
                    (unsigned long long)max_steps);
            e->halted    = true;
            e->exit_code = 1;
            break;
        }
    }
    return e->exit_code;
}
