/**
 * @file uart.h
 * @brief NS16550A-compatible UART peripheral.
 *
 * Implements the subset needed for polled TX/RX:
 *
 * | Offset | Name | Dir | Description |
 * |--------|------|-----|-------------|
 * | 0x00   | THR  | W   | Transmit: byte written here goes to tx_file |
 * | 0x00   | RBR  | R   | Receive: byte read here comes from rx_file |
 * | 0x01   | IER  | RW  | Interrupt Enable (stub -- interrupts not wired) |
 * | 0x02   | IIR  | R   | Interrupt ID: always 0x01 (no interrupt pending) |
 * | 0x02   | FCR  | W   | FIFO Control (stub -- accepted, ignored) |
 * | 0x03   | LCR  | RW  | Line Control (DLAB bit tracked for divisor latch) |
 * | 0x04   | MCR  | RW  | Modem Control (stub) |
 * | 0x05   | LSR  | R   | Line Status: always 0x60 (THRE|TEMT -- TX ready) |
 * | 0x06   | MSR  | R   | Modem Status: always 0x00 |
 * | 0x07   | SCR  | RW  | Scratch register |
 * | 0x00   | DLL  | RW  | Divisor latch low  (when LCR.DLAB=1) |
 * | 0x01   | DLH  | RW  | Divisor latch high (when LCR.DLAB=1) |
 *
 * RX: if rx_file is NULL, RBR always reads 0 and LSR.DR is never set.@n
 * TX: bytes written to THR are fputc'd to tx_file (defaults to stdout).
 */

#ifndef RVE_UART_H
#define RVE_UART_H

#include "rve/types.h"
#include "rve/peripherals/peripheral.h"

#include <stdio.h>

#define UART_LSR_DR   (1u << 0)  /**< Data Ready: RX byte is available.           */
#define UART_LSR_THRE (1u << 5)  /**< TX Holding Register Empty: ready to send.   */
#define UART_LSR_TEMT (1u << 6)  /**< TX shift register empty.                    */

/**
 * @brief NS16550A UART peripheral context.
 */
typedef struct {
    FILE *tx_file;   /**< Destination for transmitted bytes (default: stdout). */
    FILE *rx_file;   /**< Source for received bytes (NULL = no RX).            */

    /* Visible registers */
    u8 ier;          /**< Interrupt Enable Register.   */
    u8 lcr;          /**< Line Control Register.       */
    u8 mcr;          /**< Modem Control Register.      */
    u8 scr;          /**< Scratch Register.            */
    u8 dll;          /**< Divisor Latch Low byte.      */
    u8 dlh;          /**< Divisor Latch High byte.     */

    /* RX one-byte buffer */
    bool rx_ready;   /**< True when rx_byte holds a valid received byte. */
    u8   rx_byte;    /**< Buffered received byte.                        */
} uart_ns16550a_t;

/** @brief Peripheral operations table for the NS16550A UART. */
extern peripheral_ops_t uart_ns16550a_ops;

/**
 * @brief Allocate and zero-initialise a UART context.
 *
 * @param tx_file  Output stream for transmitted bytes; may be NULL (defaults to stdout).
 * @param rx_file  Input stream for received bytes; may be NULL (disables RX).
 * @return         Newly allocated context; caller takes ownership.
 */
uart_ns16550a_t *uart_ns16550a_create(FILE *tx_file, FILE *rx_file);

#endif /* RVE_UART_H */
