/**
 * @file uart.c
 * @brief NS16550A-compatible UART peripheral implementation.
 */

#include "rve/peripherals/uart.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/**
 * @brief Return true when the Divisor Latch Access Bit (DLAB) is set.
 *
 * When DLAB is set the offset-0/1 registers alias to DLL/DLH instead of
 * THR/RBR and IER.
 */
static inline bool dlab(const uart_ns16550a_t *u)
{
    return (u->lcr & 0x80u) != 0;
}

static bool uart_init(void *ctx)
{
    uart_ns16550a_t *u = (uart_ns16550a_t *)ctx;
    u->ier      = 0x00;
    u->lcr      = 0x00;
    u->mcr      = 0x00;
    u->scr      = 0x00;
    u->dll      = 0x01; // arbitrary non-zero divisor
    u->dlh      = 0x00;
    u->rx_ready = false;
    u->rx_byte  = 0x00;
    return true;
}

static bool uart_read(void *ctx, word addr, u8 size_bits, word *result)
{
    (void)size_bits;
    uart_ns16550a_t *u = (uart_ns16550a_t *)ctx;
    u8 offset = (u8)(addr & 0x7u);
    u8 val    = 0;

    if (offset == 0x00) {
        if (dlab(u)) {
            val = u->dll;
        } else {
            // RBR -- consume one byte from rx_file if available
            if (u->rx_ready) {
                val = u->rx_byte;
                u->rx_ready = false;
                // Pre-fetch next byte so LSR.DR reflects reality
                if (u->rx_file) {
                    int c = fgetc(u->rx_file);
                    if (c != EOF) { u->rx_byte = (u8)c; u->rx_ready = true; }
                }
            }
        }
    } else if (offset == 0x01) {
        val = dlab(u) ? u->dlh : u->ier;
    } else if (offset == 0x02) {
        // IIR: no interrupt pending
        val = 0x01;
    } else if (offset == 0x03) {
        val = u->lcr;
    } else if (offset == 0x04) {
        val = u->mcr;
    } else if (offset == 0x05) {
        // LSR: TX always ready; DR set if RX byte is waiting
        val = UART_LSR_THRE | UART_LSR_TEMT;
        if (u->rx_ready) val |= UART_LSR_DR;
    } else if (offset == 0x06) {
        // MSR: no modem lines
        val = 0x00;
    } else if (offset == 0x07) {
        val = u->scr;
    }

    *result = val;
    return true;
}

static bool uart_write(void *ctx, word addr, word data, u8 size_bits)
{
    (void)size_bits;
    uart_ns16550a_t *u = (uart_ns16550a_t *)ctx;
    u8 offset = (u8)(addr & 0x7u);
    u8 val    = (u8)(data & 0xFFu);

    if (offset == 0x00) {
        if (dlab(u)) {
            u->dll = val;
        } else {
            // THR -- transmit byte
            FILE *f = u->tx_file ? u->tx_file : stdout;
            fputc(val, f);
        }
    } else if (offset == 0x01) {
        if (dlab(u)) u->dlh = val;
        else         u->ier = val;
    } else if (offset == 0x02) {
        // FCR -- FIFO control, ignore
    } else if (offset == 0x03) {
        u->lcr = val;
    } else if (offset == 0x04) {
        u->mcr = val;
    } else if (offset == 0x07) {
        u->scr = val;
    }
    // LSR (0x05) and MSR (0x06) are read-only -- writes silently ignored
    return true;
}

peripheral_ops_t uart_ns16550a_ops = {
    .init   = uart_init,
    .deinit = NULL,
    .read   = uart_read,
    .write  = uart_write,
    .tick   = NULL,
};

uart_ns16550a_t *uart_ns16550a_create(FILE *tx_file, FILE *rx_file)
{
    uart_ns16550a_t *u = calloc(1, sizeof(*u));
    if (!u) return NULL;
    u->tx_file = tx_file ? tx_file : stdout;
    u->rx_file = rx_file;
    uart_init(u);

    if (rx_file) {
        int c = fgetc(rx_file);
        if (c != EOF) { u->rx_byte = (u8)c; u->rx_ready = true; }
    }
    return u;
}
