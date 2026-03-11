#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# setup_tests.sh  —  Build and strip riscv-tests ISA tests.
#
# Usage:
#   ./tools/setup_tests.sh [ISA...]
#
# Examples:
#   ./tools/setup_tests.sh                     # default: rv32ui rv32um
#   ./tools/setup_tests.sh rv32ui              # only base integer
#   ./tools/setup_tests.sh rv32ui rv32um rv32mi
#
# Each ISA name maps to the -p- (physical, no VM) variants only.
# Results land in: external/riscv-tests/isa/
# ---------------------------------------------------------------------------

set -euo pipefail

RISCV_TESTS_DIR="$PROJECT_ROOT/external/riscv-tests"
ISA_DIR="$RISCV_TESTS_DIR/isa"

# Default ISA sets to build — override via args
ISA_SETS=( "${@:-rv32ui rv32um}" )
# If no args, use the default array properly
if [[ $# -eq 0 ]]; then
    ISA_SETS=( rv32ui rv32um )
fi

# ---------------------------------------------------------------------------
# Sanity check
# ---------------------------------------------------------------------------

if [[ ! -d "$RISCV_TESTS_DIR" || ! -f "$RISCV_TESTS_DIR/configure.ac" ]]; then
    echo "ERROR: riscv-tests submodule not found at $RISCV_TESTS_DIR"
    echo "       Run: git submodule update --init --recursive"
    exit 1
fi

# ---------------------------------------------------------------------------
# Auto-detect toolchain prefix
# ---------------------------------------------------------------------------

detect_prefix() {
    for candidate in \
        riscv64-none-elf-    \
        riscv32-none-elf-    \
        riscv64-unknown-elf- \
        riscv32-unknown-elf- \
        riscv64-linux-gnu-   \
    ; do
        if command -v "${candidate}gcc" &>/dev/null; then
            echo "$candidate"; return
        fi
    done
    echo ""
}

# RISCV_PREFIX="$(detect_prefix)"
RISCV_PREFIX=riscv32-none-elf-
if [[ -z "$RISCV_PREFIX" ]]; then
    echo "ERROR: No RISC-V gcc found in PATH."
    exit 1
fi
echo "Toolchain prefix : $RISCV_PREFIX"
echo "ISA sets         : ${ISA_SETS[*]}"

# ---------------------------------------------------------------------------
# Configure (once)
# ---------------------------------------------------------------------------

cd "$RISCV_TESTS_DIR"
[[ ! -f configure ]] && autoconf
[[ ! -f Makefile  ]] && ./configure

# ---------------------------------------------------------------------------
# Build only the requested ISA subdirs, forcing XLEN=32
# ---------------------------------------------------------------------------

for ISA in "${ISA_SETS[@]}"; do
    # Derive XLEN from name (rv32 -> 32, rv64 -> 64)
    if [[ "$ISA" == rv32* ]]; then XLEN=32; else XLEN=64; fi

    XLEN=32

    echo "Building $ISA (XLEN=$XLEN)..."
    make -j"$(nproc)" \
        RISCV_PREFIX="$RISCV_PREFIX" \
        XLEN="$XLEN" \
        "isa/$ISA" 2>/dev/null || \
    # Fallback: build all isa and let the strip step filter
    make -j"$(nproc)" \
        RISCV_PREFIX="$RISCV_PREFIX" \
        XLEN="$XLEN" \
        isa
    break  # the isa target builds everything, one pass is enough
done

cd "$PROJECT_ROOT"

# ---------------------------------------------------------------------------
# Strip ELFs to flat binaries (only requested ISA sets, -p- variants only)
# ---------------------------------------------------------------------------

echo "Stripping ELFs to flat binaries..."
STRIPPED=0

for ISA in "${ISA_SETS[@]}"; do
    echo "Processing ISA set: $ISA"
    for f in "$ISA_DIR"/${ISA}-p-*; do
        echo "file: $f"
        [[ "$f" == *.dump ]] && continue
        [[ "$f" == *.bin  ]] && continue
        [[ -f "$f" ]] || continue
        "${RISCV_PREFIX}objcopy" -O binary "$f" "${f}.bin"
        ((STRIPPED++)) || true
    done
done

echo "Stripped $STRIPPED test binaries."
echo ""
echo "Ready. Run: ./tools/run_tests.sh ${ISA_SETS[*]}"
