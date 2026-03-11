# rve

A instruction-accurate RISC-V RV32IM emulator written in C99.
It runs flat raw binaries, exposes a C API, and is
validated against the upstream `riscv-tests` suite.

## Background

This project started as a co-simulator for a custom RISC-V processor I was
building from scratch in RTL.  Because the hardware was being developed
incrementally, with only a subset of the ISA wired up at any point in time,
I needed a software reference model I could keep in sync with the hardware's
current capabilities.  Off-the-shelf simulators implement the full spec and
are hard to trim down, so it was simpler to write one from scratch and grow it
alongside the hardware.

TODO: explain how to use this as a co-simulator.

## Building

### Prerequisites

#### With Nix

The repo ships a `flake.nix` that provides a dev shell with the host
compiler plus cross-compilers for RISC-V (needed only to rebuild
`riscv-tests`):

```bash
nix develop          # drops you into the dev shell
source set_env.sh
make
```

### Without Nix

Only `gcc` and  `make` is required, tested on `gcc 15.2.0` and `GNU Make 4.4.1`.

```bash
source set_env.sh
make
```

The binary lands at `bin/rve`.

## Running the emulator

```
Usage: bin/rve [options] <binary>

Options:
  --trace          Print one line per retired instruction
  --regs           Dump register file on exit
  --pc <hex>       Set reset PC            (default: 0x00000000)
  --load <hex>     Load binary at address  (default: 0x00000000)
  --max <n>        Stop after n instructions
  --help           Show this message

The binary must be a flat raw image (not ELF).
```

### Examples

Run a riscv-tests image silently (must run `./tools/compile_tests.sh` first).

```bash
./bin/rve --pc 0x80000000 --load 0x80000000 \
    external/riscv-tests/isa/rv32ui-p-add.bin
```

Same test with a full instruction trace:

```bash
./bin/rve --trace --pc 0x80000000 --load 0x80000000 \
    external/riscv-tests/isa/rv32ui-p-add.bin
```

Trace + register dump, limit to 10 000 instructions:

```bash
./bin/rve --trace --regs --max 10000 \
    --pc 0x80000000 --load 0x80000000 \
    external/riscv-tests/isa/rv32ui-p-ma_data.bin
```

Trace output format (one line per instruction):

```
[     1] 0x80000000  auipc    t0, 0x0           t0 <- 0x80000000
[     2] 0x80000004  addi     t1, t0, 312       t1 <- 0x80000138
[     3] 0x80000008  sw       t1, 0(t0)         mem[0x80000000] <- 0x80000138 (32b)
```

## Test suite

### Building the test binaries

The `external/riscv-tests/isa/` binaries are imported as a `git submodule`.
To rebuild from source (requires the RISC-V cross-compiler from the Nix dev shell, or one you have):

```bash
nix develop
./tools/compile_tests.sh
```

### Running the tests

`run_tests.sh` runs the pre-built `riscv-tests` images in
`external/riscv-tests/isa/` through the emulator and compares exit
codes:

```bash
./tools/compile_tests.sh
./tools/run_tests.sh # default rv32ui rv32um
./tools/run_tests.sh rv32ui
./tools/run_tests.sh rv32ui rv32um

# re-run failing tests with --trace automatically
./tools/run_tests.sh --trace-fail

# use a custom emulator binary
./tools/run_tests.sh --emulator ./bin/rve rv32ui
```

A clean run looks like:

```
PASS  rv32ui-p-add
PASS  rv32ui-p-addi
...
Results: 50 / 50 passed
```

