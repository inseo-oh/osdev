// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include <kernel/arch/arch.h>
#include <kernel/kernel.h>
#include <stddef.h>
#include <stdint.h>

#define ASSERT_PORT_RANGE(__port) ASSERT((__port) < 65536)

void ioport_out8(ioaddr_t port, uint8_t val) {
        ASSERT_PORT_RANGE(port);
        __asm__ volatile("out dx, al" ::"d"(port), "a"(val));
}

void ioport_out16(ioaddr_t port, uint16_t val) {
        __asm__ volatile("out dx, ax" ::"d"(port), "a"(val));
}

uint8_t ioport_in8(ioaddr_t port) {
        ASSERT_PORT_RANGE(port);
        uint8_t result = 0;
        __asm__ volatile("in al, dx" : "=a"(result) : "d"(port));
        return result;
}

uint16_t ioport_in16(ioaddr_t port) {
        ASSERT_PORT_RANGE(port);
        uint16_t result = 0;
        __asm__ volatile("in ax, dx" : "=a"(result) : "d"(port));
        return result;
}

uint16_t ioport_rep_ins16(ioaddr_t port, void *buf, size_t len) {
        ASSERT_PORT_RANGE(port);
        uint16_t result = 0;
        __asm__ volatile("rep insw"
                         : "=a"(result)
                         : "d"(port), "D"(buf), "c"(len));
        return result;
}
