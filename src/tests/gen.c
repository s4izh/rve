// Generates a minimal RISC-V flat binary that:
//   1. Sets up a stack pointer
//   2. Counts from 0 to 4 in a loop
//   3. Calls exit(0) via ecall (Linux ABI: a7=93, a0=exit_code)
//
// Assemble with:
//   riscv32-unknown-elf-gcc -march=rv32im -mabi=ilp32 \
//       -nostdlib -Ttext=0x0 -o test.elf test.s
//   riscv32-unknown-elf-objcopy -O binary test.elf test.bin
//
// Or just run this generator:
//   gcc test_gen.c -o test_gen && ./test_gen test.bin

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void write_word(FILE *f, uint32_t w)
{
    // little-endian
    fputc((w >>  0) & 0xFF, f);
    fputc((w >>  8) & 0xFF, f);
    fputc((w >> 16) & 0xFF, f);
    fputc((w >> 24) & 0xFF, f);
}

int main(int argc, char *argv[])
{
    const char *outfile = (argc >= 2) ? argv[1] : "test.bin";

    FILE *f = fopen(outfile, "wb");
    if (!f) { perror("fopen"); return 1; }

    // The program, assembled by hand.
    // Each instruction is a 32-bit little-endian word.
    // Load address = 0x00000000, reset PC = 0x00000000.

    // addi sp, x0, 0x400    # sp = 0x400  (top of our 1KiB scratch space)
    // lui  a5, 0            # a5 = 0      (loop counter)
    // addi a4, x0, 5        # a4 = 5      (loop limit)
    //
    // loop:
    //   addi a5, a5, 1      # a5++
    //   blt  a5, a4, loop   # if a5 < 5 goto loop
    //
    // addi a0, x0, 0        # a0 = 0  (exit code)
    // addi a7, x0, 93       # a7 = 93 (Linux exit syscall)
    // ecall

    uint32_t program[] = {
        0x40000113,  // addi  sp, zero, 0x400   -- sp = 1024
        0x00000793,  // addi  a5, zero, 0        -- counter = 0
        0x00500713,  // addi  a4, zero, 5         -- limit = 5

        // loop: (PC = 0x0C)
        0x00178793,  // addi  a5, a5, 1           -- counter++
        0xfee7c8e3,  // blt   a5, a4, -16         -- if counter < limit, back to loop

        // exit:
        0x00000513,  // addi  a0, zero, 0          -- exit code = 0
        0x05d00893,  // addi  a7, zero, 93         -- syscall = exit
        0x00000073,  // ecall
    };

    for (size_t i = 0; i < sizeof(program)/sizeof(program[0]); i++)
        write_word(f, program[i]);

    fclose(f);
    printf("Wrote %zu instructions to %s\n",
           sizeof(program)/sizeof(program[0]), outfile);
    return 0;
}
