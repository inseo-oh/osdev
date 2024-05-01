// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "sched.h"
#include <yjk/syscall.h>
#include "kernel/utility/utility.h"

// https://pubs.opengroup.org/onlinepubs/9699919799/functions/sched_yield.html
int sched_yield(void) {
        // __asm__ volatile("int " STRINGIFY(SYSCALL_SCHED_YIELD_VECTOR));
        syscall0(SYSCALL_INDEX_SCHED_YIELD);
        return 0;
}