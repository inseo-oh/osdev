// SPDX-FileCopyrightText: 2023, Inseo Oh (dhdlstjtr@gmail.com)
//
// SPDX-License-Identifier: BSD-2-Clause
#pragma once
#include <bits/syscall.h>
#include <stdint.h>
#include <sys/types.h>

int64_t syscall4(
        unsigned num, uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3
);
int64_t syscall3(unsigned num, uint64_t arg0, uint64_t arg1, uint64_t arg2);
int64_t syscall2(unsigned num, uint64_t arg0, uint64_t arg1);
int64_t syscall1(unsigned num, uint64_t arg0);
int64_t syscall0(unsigned num);
