// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "_internal.h"
#include "kernel/arch/arch.h"
#include "kernel/kernel.h"
#include <stdbool.h>
#include <stdint.h>

// Baudrate for uartconsole
#define BAUDRATE 115200

static char const *LOG_TAG = "uartconsole";

// Base I/O addresses
#define COM1_IO_BASE 0x3F8 // COM1
#define COM2_IO_BASE 0x2F8 // COM2
#define COM_IO_BASE  COM1_IO_BASE

// Transmitter/Receiver (DATA)
//
// NOTE: The same port is shared with DLL. To access DATA, LCR's DLAB flag must
// be 0.
//
// Use `write_data`/`read_data` instead of accessing this port directly.
#define IO_DATA COM_IO_BASE
// Divisor Latch Low(DLL)
//
// NOTE: The same port is shared with DATA. To access DLL, LCR's DLAB flag must
// be 1.
//
// Use `write_dl` instead of accessing this port directly.
#define IO_DLL COM_IO_BASE
// Divisor Latch High(DLH)
// NOTE: The same port is shared with IER. To access DLH, LCR's DLAB flag must
// be 1.
//
// Use `write_dl` instead of accessing this port directly.
#define IO_DLH (COM_IO_BASE + 1)

// Modem Control Register (MCR)
#define IO_MCR            (COM_IO_BASE + 4)
#define MCR_FLAG_DTR      (1 << 0) // DTR signal
#define MCR_FLAG_RTS      (1 << 1) // RTS signal
#define MCR_FLAG_OUT1     (1 << 2) // OUT1 signal(Not used by PC)
#define MCR_FLAG_OUT2     (1 << 3) // OUT2 signal(Used to enable IRQs on PC)
#define MCR_FLAG_LOOPBACK (1 << 4) // Loopback mode

// Line Control Register (LCR)
#define IO_LCR                  (COM_IO_BASE + 3)
#define LCR_FLAG_WORD_LEN_FIVE  (0x0 << 0) // Use 5-bit as word length
#define LCR_FLAG_WORD_LEN_SIX   (0x1 << 0) // Use 6-bit as word length
#define LCR_FLAG_WORD_LEN_SEVEN (0x2 << 0) // Use 7-bit as word length
#define LCR_FLAG_WORD_LEN_EIGHT (0x3 << 0) // Use 8-bit as word length
#define LCR_FLAG_MULTI_STOP_BITS \
        (1 << 2                  \
        ) // When set to 1: 5-bit -> 1.5 stop-bit, otherwise -> 2 stop-bit
#define LCR_FLAG_PARITY_ENABLE (1 << 3) // Enable parity
#define LCR_FLAG_PARITY_EVEN   (0 << 4) // Use even parity
#define LCR_FLAG_PARITY_ODD    (1 << 4) // Use odd parity
#define LCR_FLAG_STICKY_PARITY \
        (1 << 5) // Use sticky parity (Even parity -> Parity is always 1,
                 // Odd parity -> Parity is always 0)
#define LCR_FLAG_SET_BREAK \
        (1 << 6) // Enter break condition by pulling Tx low
                 // (Receiving UART will see this as long stream of zeros)
#define LCR_FLAG_DLAB (1 << 7) // Divisor Latch Access Bit

// Interrupt Enable Register (IER)
//
// NOTE: The same port is shared with DLH. To access IER, LCR's DLAB
// flag must be 0.
//
// Use `write_ier` instead of accessing this port directly.
#define IO_IER                (COM_IO_BASE + 1)
#define IER_FLAG_RX_AVAIL     (1 << 0) // New data is available on Rx
#define IER_FLAG_TX_EMPTY     (1 << 1) // Transmitter is empty
#define IER_FLAG_RX_STATUS    (1 << 2) // Rx status changed
#define IER_FLAG_MODEM_STATUS (1 << 3) // Modem status changed

// Interrupt Identification Register (IIR)
#define IO_IIR                  (COM_IO_BASE + 2)
#define IIR_FLAG_NO_INT_PENDING (1 << 0) // No interrupt pending
#define IIR_FLAG_MODEM_STATUS \
        (0x0 << 1) // Modem status changed(Priority #4, lowest). To clear
                   // interrupt, read MSR.
#define IIR_FLAG_TX_EMPTY \
        (0x1 << 1) // Tx is empty(Priority #3). To clear interrupt, write Tx or
                   // read IIR.
#define IIR_FLAG_RX_AVAIL \
        (0x2 << 1)                    // Data is available at Rx(Priority #2).
                                      // To clear interrupt, read Rx data.
#define IIR_FLAG_RX_STATUS (0x3 << 1) // Rx status changed(Priority #1,
// highest). To clear interrupt, read LSR.

// Line Status Register (LSR)
#define IO_LSR               (COM_IO_BASE + 5)
#define LSR_FLAG_DATA_READY  (1 << 0) // Data is ready on Rx
#define LSR_FLAG_OVERRUN_ERR (1 << 1) // Overrun Error(OE)
#define LSR_FLAG_PARITY_ERR  (1 << 2) // Parity Error(PE)
#define LSR_FLAG_FRAMING_ERR (1 << 3) // Framing Error(FE)
#define LSR_FLAG_BREAK_RECV  (1 << 4) // Received break
#define LSR_FLAG_TX_HOLDING_REG_EMPTY \
        (1 << 5) // Tx holding register is empty. New data can be written.
#define LSR_FLAG_TX_SHIFT_REG_EMPTY \
        (1 << 6) // Tx shift register is empty. Tx is now idle.

// Modem Status Register (MSR)
#define IO_MSR                    (COM_IO_BASE + 6)
#define MSR_FLAG_CTS_DELTA        (1 << 0) // CTS state changed
#define MSR_FLAG_DSR_DELTA        (1 << 1) // DSR state changed
#define MSR_FLAG_RI_TRAILING_EDGE (1 << 2) // RI went from 1 to 0
#define MSR_FLAG_DCD_DELTA        (1 << 3) // DCD state changed
#define MSR_FLAG_CTS              (1 << 4) // Status of CTS line
#define MSR_FLAG_DSR              (1 << 5) // Status of DSR line
#define MSR_FLAG_RI               (1 << 6) // Status of RI line
#define MSR_FLAG_DCD              (1 << 7) // Status of DCD line

// Baud rate when divisor is 1
#define BASE_BAUD_RATE 115200

static void set_dlab_flag() {
        ioport_out8(IO_LCR, ioport_in8(IO_LCR) | LCR_FLAG_DLAB);
}

static void clear_dlab_flag() {
        ioport_out8(IO_LCR, ioport_in8(IO_LCR) & ~LCR_FLAG_DLAB);
}

static void write_ier(uint8_t val) {
        clear_dlab_flag();
        ioport_out8(IO_IER, val);
}

static void write_dl(uint16_t val) {
        set_dlab_flag();
        ioport_out8(IO_DLL, val);
        ioport_out8(IO_DLH, val >> 8);
}

static void write_data(uint8_t val) {
        clear_dlab_flag();
        ioport_out8(IO_DATA, val);
}

static uint8_t read_data() {
        clear_dlab_flag();
        return ioport_in8(IO_DATA);
}

static bool run_loopback_test() {
        uint8_t old_mcr = ioport_in8(IO_MCR);
        ioport_out8(IO_MCR, old_mcr | MCR_FLAG_LOOPBACK);
        write_data(0x69);
        bool test_ok = read_data() == 0x69;
        ioport_out8(IO_MCR, old_mcr);
        return test_ok;
}

static uint16_t baudrate_to_divisor(uint32_t baud_rate) {
        for (uint32_t divisor = 1; divisor <= 0xffff; ++divisor) {
                if ((BASE_BAUD_RATE / divisor) == baud_rate) {
                        return divisor;
                }
        }
        panic("Bad baud rate %u", baud_rate);
}

static void put_char(struct Console_Driver *driver, char chr) {
        (void)driver;
        while ((ioport_in8(IO_LSR) & LSR_FLAG_TX_HOLDING_REG_EMPTY) !=
               LSR_FLAG_TX_HOLDING_REG_EMPTY) {}
        write_data(chr);
}

static int get_char(struct Console_Driver *driver) {
        (void)driver;
        if ((ioport_in8(IO_LSR) & LSR_FLAG_DATA_READY) != LSR_FLAG_DATA_READY) {
                return -1;
        }
        return read_data();
}

static void flush(struct Console_Driver *driver) { (void)driver; }

void uartconsole_init() {
        static struct Console_Driver console_driver = {
                .put_char_fn = put_char,
                .get_char_fn = get_char,
                .flush_fn = flush,
        };
        if (!run_loopback_test()) {
                LOGE(LOG_TAG,
                     "Loopback test failed. Not initializing serial port.");
                return;
        }
        write_ier(0); // Disable all interrupts
        ioport_out8(
                IO_MCR,
                MCR_FLAG_DTR | MCR_FLAG_RTS | MCR_FLAG_OUT1 | MCR_FLAG_OUT2
        );
        uint16_t dl_val = baudrate_to_divisor(BAUDRATE);
        ASSERT(dl_val != 0);
        write_dl(dl_val);
        ioport_out8(IO_LCR, LCR_FLAG_WORD_LEN_EIGHT);
        console_register_driver(&console_driver);
}
