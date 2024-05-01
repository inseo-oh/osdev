/*
 * SPDX-FileCopyrightText: 2023, Inseo Oh (dhdlstjtr@gmail.com)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include "kernel/arch/arch.h"
#include "kernel/kernel.h"
#include <stddef.h>
#include <stdint.h>

void stacktrace_show_using_rbp(void *rbp) {
        uint64_t *next_rbp = (uint64_t *)rbp;
        size_t index = 0;
        while (next_rbp) {
                uintptr_t ret_rip = next_rbp[1];
                console_printf("%lu: %p\n", index, (void *)ret_rip);
                next_rbp = (uint64_t *)*next_rbp;
                if (!next_rbp || !mmu_is_accessible(next_rbp, 0)) {
                        break;
                }
                ++index;
        }
}

void stacktrace_show(void) {
        void *rbp;
        __asm__ volatile("mov %0, rbp" : "=r"(rbp));
        stacktrace_show_using_rbp(rbp);
}
