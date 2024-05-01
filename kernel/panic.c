// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "kernel.h"
#include "kernel/arch/arch.h"
#include <stdarg.h>
#include <stdbool.h>

static bool s_is_nested_panic = false;

void panic(char const *fmt, ...) {
        bool prev_interrupt_state = interrupts_disable();
        (void)prev_interrupt_state;
        processor_halt_others();
        if (s_is_nested_panic) {
                while (1) {}
        }
        s_is_nested_panic = true;
        if (fmt) {
                va_list argptr;
                va_start(argptr, fmt);
                console_printf("Kernel panic: ");
                console_vprintf(fmt, argptr);
                console_flush();
        }
        while (1) {}
}

void panic_assert_fail(char const *file, int line, char *what) {
        panic("%s:%u: Assertion failed: %s", file, line, what);
}
