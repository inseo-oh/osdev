// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "_internal.h"
#include "kernel/arch/arch.h"
#include <stdbool.h>
#include <stdint.h>

bool interrupts_enable() {
        bool were_enabled = interrupts_are_enabled();
        __asm__ volatile("sti");
        return were_enabled;
}

bool interrupts_disable() {
        bool were_enabled = interrupts_are_enabled();
        __asm__ volatile("cli");
        return were_enabled;
}

void interrupts_wait() { __asm__ volatile("hlt"); }

bool interrupts_are_enabled() {
        uint64_t flags;
        __asm__ volatile("pushfq\n"
                         "pop %0\n"
                         : "=r"(flags));
        return (flags & RFLAGS_IF) == RFLAGS_IF;
}
