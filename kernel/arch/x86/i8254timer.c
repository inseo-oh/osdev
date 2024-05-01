// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "_internal.h"
#include "kernel/arch/arch.h"
#include "kernel/kernel.h"
#include <stdint.h>

// I/O addresses
#define IO_CH0_DATA 0x40
#define IO_CH2_DATA 0x42
#define IO_MODE     0x43

// PIT's frequency is roughly 1.193182MHz
#define PIT_FREQ 1193182

// Select channel(Bit 6:7)
#define MODE_FLAG_SEL_CH0 (0 << 6) // Use channel 0

// Access mode(Bit 4:5)
#define MODE_FLAG_ACCESS_LATCH   (0 << 4) // Latch current counter
#define MODE_FLAG_ACCESS_LSB_MSB (3 << 4) // Low, Hibyte order
// Operating mode(Bit 1:3)
#define MODE_FLAG_OP_ONESHOT (1 << 1) // Hardware re-triggerable one-shot
#define MODE_FLAG_OP_RATEGEN (2 << 1) // Mode - Rate generator
// Binary/BCD mode(Bit 0)
#define MODE_FLAG_BIN (0 << 0) // Binary mode

// Constants for counting 1ms
#define DESIRED_PERIOD_MS 1
#define DESIRED_FREQ_HZ   (1000 / DESIRED_PERIOD_MS)
#define DESIRED_CNT_VALUE (PIT_FREQ / DESIRED_FREQ_HZ)

static void short_delay() { ioport_in8(IO_MODE); }

static uint16_t read_counter() {
        ioport_out8(IO_MODE, MODE_FLAG_SEL_CH0 | MODE_FLAG_ACCESS_LATCH);
        uint16_t lob = ioport_in8(IO_CH0_DATA);
        uint16_t hib = ioport_in8(IO_CH0_DATA);
        return (hib << 8) | lob;
}

void i8254timer_reset_to_1ms(void) {
        ioport_out8(
                IO_MODE,
                MODE_FLAG_SEL_CH0 | MODE_FLAG_ACCESS_LSB_MSB |
                        MODE_FLAG_OP_RATEGEN | MODE_FLAG_BIN
        );
        ioport_out8(IO_CH0_DATA, DESIRED_CNT_VALUE & 0xff);
        short_delay();
        ioport_out8(IO_CH0_DATA, DESIRED_CNT_VALUE >> 8);
}

void i8254timer_stop(void) {
        ioport_out8(
                IO_MODE,
                MODE_FLAG_SEL_CH0 | MODE_FLAG_ACCESS_LSB_MSB |
                        MODE_FLAG_OP_RATEGEN | MODE_FLAG_BIN
        );
}

void i8254timer_oneshot_count(unsigned millis) {
        // ASSERT((DESIRED_CNT_VALUE * millis) <= 0xffff);
        ASSERT(1);
        ioport_out8(
                IO_MODE,
                MODE_FLAG_SEL_CH0 | MODE_FLAG_ACCESS_LSB_MSB |
                        MODE_FLAG_OP_RATEGEN | MODE_FLAG_BIN
        );
        uint32_t expected = 0xffff - (DESIRED_CNT_VALUE * millis);
        ioport_out8(IO_CH0_DATA, 0xff);
        short_delay();
        ioport_out8(IO_CH0_DATA, 0xff);
        while (expected < read_counter()) {}
}
