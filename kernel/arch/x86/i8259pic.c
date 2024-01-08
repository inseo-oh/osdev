// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "_internal.h"
#include <kernel/arch/arch.h>
#include <kernel/kernel.h>
#include <stdbool.h>
#include <stdint.h>

// Base I/O addresses
#define MASTER_PIC_BASE 0x20
#define SLAVE_PIC_BASE  0xa0

// Master PIC I/O addresses
#define MASTER_PIC_IO_CMD  (MASTER_PIC_BASE + 0)
#define MASTER_PIC_IO_DATA (MASTER_PIC_BASE + 1)

// Slave PIC I/O addresses
#define SLAVE_PIC_IO_CMD  (SLAVE_PIC_BASE + 0)
#define SLAVE_PIC_IO_DATA (SLAVE_PIC_BASE + 1)

// Commnads, and flags for ICW1 and ICW4
#define CMD_EOI           0x20
#define ICW1_FLAG_ICW4    (1 << 0)
#define ICW1_FLAG_INITVAL (1 << 4)
#define ICW4_FLAG_8086    (1 << 0)

// Other stuff
#define SLAVE_PIC_PIN_IN_MASTER 2

struct I8259 {
        uint16_t cmd_port, data_port;
};

static uint8_t irq_to_mask(uint8_t irq) {
        ASSERT(irq < 8);
        return 1 << irq;
}

// Meaning of icw3 depends on whether the PIC is primary or secondary.
//
// - Primary: `(1 << n)`, where `n` is the num of IRQ where secondary PIC
//   is connected to(SLAVE_PIC_PIN_IN_MASTER).
// - Secondary: The `n` from the above
static void
init_pic(struct I8259 const *pic, uint8_t vector_off, uint8_t icw3) {
        ioport_out8(pic->cmd_port, ICW1_FLAG_INITVAL | ICW1_FLAG_ICW4);
        // ICW2 ~ ICW4 is sent through data port
        ioport_out8(pic->data_port, vector_off);     // ICW2
        ioport_out8(pic->data_port, icw3);           // ICW3
        ioport_out8(pic->data_port, ICW4_FLAG_8086); // ICW4
}

static void send_eoi(struct I8259 const *pic) {
        ioport_out8(pic->cmd_port, CMD_EOI);
}

static uint8_t get_irq_mask(struct I8259 const *pic) {
        return ioport_in8(pic->data_port);
}

static void set_irq_mask(struct I8259 const *pic, uint8_t mask) {
        ioport_out8(pic->data_port, mask);
}

static bool is_irq_masked(struct I8259 const *pic, uint8_t irq) {
        uint8_t mask = irq_to_mask(irq);
        return (get_irq_mask(pic) & mask) == mask;
}

static void mask_irq(struct I8259 const *pic, uint8_t irq) {
        set_irq_mask(pic, get_irq_mask(pic) | irq_to_mask(irq));
}

static void unmask_irq(struct I8259 const *pic, uint8_t irq) {
        set_irq_mask(pic, get_irq_mask(pic) & ~irq_to_mask(irq));
}

static const struct I8259 MASTER_PIC = {MASTER_PIC_IO_CMD, MASTER_PIC_IO_DATA},
                          SLAVE_PIC = {SLAVE_PIC_IO_CMD, SLAVE_PIC_IO_DATA};

void i8259pic_init(void) {
        init_pic(
                &MASTER_PIC, PIC_MASTER_VECTOR_OFF, 1 << SLAVE_PIC_PIN_IN_MASTER
        );
        init_pic(&SLAVE_PIC, PIC_SLAVE_VECTOR_OFF, SLAVE_PIC_PIN_IN_MASTER);
        set_irq_mask(&MASTER_PIC, 0);
        set_irq_mask(&SLAVE_PIC, 0);
}

void i8259pic_send_eoi(uint8_t irq) {
        ASSERT(irq < 16);
        if (irq >= 8) {
                send_eoi(&SLAVE_PIC);
        }
        send_eoi(&MASTER_PIC);
}

bool i8259pic_is_irq_masked(uint8_t irq) {
        ASSERT(irq < 16);
        return (irq >= 8) ? is_irq_masked(&SLAVE_PIC, irq - 8)
                          : is_irq_masked(&MASTER_PIC, irq);
}

void i8259pic_mask_irq(uint8_t irq) {
        ASSERT(irq < 16);
        if (irq >= 8) {
                mask_irq(&SLAVE_PIC, irq - 8);
        } else {
                mask_irq(&MASTER_PIC, irq);
        }
}

void i8259pic_unmask_irq(uint8_t irq) {
        ASSERT(irq < 16);
        if (irq >= 8) {
                unmask_irq(&SLAVE_PIC, irq - 8);
        } else {
                unmask_irq(&MASTER_PIC, irq);
        }
}
