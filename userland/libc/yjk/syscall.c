// SPDX-FileCopyrightText: 2023, Inseo Oh (dhdlstjtr@gmail.com)
//
// SPDX-License-Identifier: BSD-2-Clause
#include "syscall.h"
#include <bits/syscall.h>
#include <errno.h>
#include <stdint.h>
#include <sys/types.h>

int64_t syscall4(unsigned num, uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
        int64_t result;
        __asm__ volatile("syscall"
                         : "=a"(result)
                         : "a"(num), "D"(arg0), "S"(arg1), "d"(arg2), "b"(arg3)
                         : "memory");
        if (result < 0) {
                errno = (int)result * -1;
                return -1;
        }
        return result;
}

int64_t syscall3(unsigned num, uint64_t arg0, uint64_t arg1, uint64_t arg2) {
        return syscall4(num, arg0, arg1, arg2, 0);
}

int64_t syscall2(unsigned num, uint64_t arg0, uint64_t arg1) {
        return syscall3(num, arg0, arg1, 0);
}

int64_t syscall1(unsigned num, uint64_t arg0) {
        return syscall2(num, arg0, 0);
}

int64_t syscall0(unsigned num) {
        return syscall1(num, 0);
}
