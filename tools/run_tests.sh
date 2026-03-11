#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# run_tests.sh  —  Run riscv-tests against the rve emulator.
#
# Usage:
#   ./tools/run_tests.sh [ISA...]
#
# Examples:
#   ./tools/run_tests.sh                  # default: rv32ui rv32um
#   ./tools/run_tests.sh rv32ui
#   ./tools/run_tests.sh rv32ui rv32um rv32mi
#
# Options (must come before ISA names):
#   --emulator <path>    Path to rve binary (default: ./build/rve)
#   --trace-fail         Re-run failing tests with --trace
#
# The riscv-tests pass/fail convention:
#   sw t0, 0(t1)  where t1 = 0x80001000
#   t0 = 1                      -> PASS
#   t0 = (test_num << 1) | 1    -> FAIL at test_num
# ---------------------------------------------------------------------------

set -euo pipefail

ISA_DIR="$PROJECT_ROOT/external/riscv-tests/isa"
EMULATOR="$PROJECT_ROOT/bin/rve"
TRACE_FAIL=0
ISA_SETS=()

# Parse options
while [[ $# -gt 0 ]]; do
    case "$1" in
        --emulator)   EMULATOR="$2"; shift 2 ;;
        --trace-fail) TRACE_FAIL=1;  shift   ;;
        -*)           echo "Unknown option: $1"; exit 1 ;;
        *)            ISA_SETS+=("$1"); shift ;;
    esac
done

[[ ${#ISA_SETS[@]} -eq 0 ]] && ISA_SETS=( rv32ui rv32um )

# ---------------------------------------------------------------------------
# Checks
# ---------------------------------------------------------------------------

if [[ ! -x "$EMULATOR" ]]; then
    echo "ERROR: emulator not found at '$EMULATOR' — run 'make' first."
    exit 1
fi

if [[ ! -d "$ISA_DIR" ]]; then
    echo "ERROR: $ISA_DIR not found — run './tools/setup_tests.sh' first."
    exit 1
fi

# ---------------------------------------------------------------------------
# Run
# ---------------------------------------------------------------------------

PASS=0
FAIL=0
ERRORS=()

for ISA in "${ISA_SETS[@]}"; do
    # Derive load/PC address from ISA name
    # rv32*: 0x80000000  (standard riscv-tests link address)
    MEM_BASE="0x80000000"
    MEM_SIZE="0x10000"    # 64KiB — enough for all -p- tests

    BINARIES=( "$ISA_DIR"/${ISA}-p-*.bin )

    if [[ ${#BINARIES[@]} -eq 0 ]] || [[ ! -f "${BINARIES[0]}" ]]; then
        echo "WARNING: No binaries found for $ISA — run './tools/setup_tests.sh $ISA'"
        continue
    fi

    echo "--- $ISA ---"

    for BIN in "${BINARIES[@]}"; do
        NAME=$(basename "$BIN" .bin)

        # if "$EMULATOR" --pc "$MEM_BASE" --load "$MEM_BASE" --mem "${MEM_BASE}:${MEM_SIZE}" "$BIN" > output.log 2>&1; then
        if "$EMULATOR" --pc "$MEM_BASE" --load "$MEM_BASE" "$BIN" > output.log 2>&1; then
            # Process returned 0 (Clean ECALL exit)
            STATUS="PASS"
            ((PASS++)) || true
        else
            # Process returned non-zero (Trap, Crash, or Test Failure)
            EXIT_CODE=$?

            TOHOST_VAL=$(grep -oP "mem\[0x80001000\] <- \K0x[0-9a-f]+" output.log | tail -1 || true)

            if [[ "$TOHOST_VAL" == "0x1" ]]; then
                STATUS="PASS" # Some environments write tohost AND exit 0
                ((PASS++)) || true
            else
                STATUS="FAIL(code $EXIT_CODE)"
                ((FAIL++)) || true
                ERRORS+=("$NAME: Failed with exit code $EXIT_CODE")
            fi
        fi

        # TRACE_OUT=$("$EMULATOR" \
        #     --trace \
        #     --pc   "$MEM_BASE" \
        #     --load "$MEM_BASE" \
        #     --mem  "${MEM_BASE}:${MEM_SIZE}" \
        #     "$BIN" 2>/dev/null)

        # EXIT_STATUS=$?

        # if [[ $EXIT_STATUS -eq 0 ]]; then
        #     STATUS="PASS"
        #     ((PASS++)) || true
        # else
        #     TOHOST_LINE=$(echo "$TRACE_OUT" | grep -i "mem\[0x80001000\]" | tail -1 || true)

        #     if [[ -z "$TOHOST_LINE" ]]; then
        #         STATUS="CRASH"
        #         ((FAIL++)) || true
        #         ERRORS+=("$NAME: no tohost write")
        #     else
        #         VALUE=$(echo "$TOHOST_LINE" | grep -oP '0x[0-9A-Fa-f]+$' || echo "0x0")
        #         VALUE_DEC=$(printf "%d" "$VALUE" 2>/dev/null || echo "0")

        #         if [[ "$VALUE_DEC" -eq 1 ]]; then
        #             STATUS="PASS"
        #             ((PASS++)) || true
        #         else
        #             FAIL_TEST=$(( VALUE_DEC >> 1 ))
        #             STATUS="FAIL(test $FAIL_TEST)"
        #             ((FAIL++)) || true
        #             ERRORS+=("$NAME: failed at test case $FAIL_TEST")
        #         fi
        #     fi
        # fi

        if [[ "$STATUS" == "PASS" ]]; then
            printf "  \033[32mPASS\033[0m  %s\n" "$NAME"
        else
            printf "  \033[31m%-18s\033[0m  %s\n" "$STATUS" "$NAME"
        fi

        if [[ "$STATUS" != "PASS" && "$TRACE_FAIL" -eq 1 ]]; then
            echo "  --- trace: $NAME ---"
            "$EMULATOR" \
                --trace --regs \
                --pc   "$MEM_BASE" \
                --load "$MEM_BASE" \
                --mem  "${MEM_BASE}:${MEM_SIZE}" \
                "$BIN" 2>&1 | head -60
            echo "  --- end trace ---"
        fi
    done
done

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------

TOTAL=$(( PASS + FAIL ))
echo ""
printf "Results: %d / %d passed\n" "$PASS" "$TOTAL"

if [[ ${#ERRORS[@]} -gt 0 ]]; then
    echo ""
    echo "Failures:"
    for E in "${ERRORS[@]}"; do
        printf "  - %s\n" "$E"
    done
fi

echo ""
[[ $FAIL -eq 0 ]] && echo "All tests passed." && exit 0
exit 1
