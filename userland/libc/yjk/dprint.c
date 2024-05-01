// SPDX-FileCopyrightText: 2024, Inseo Oh (dhdlstjtr@gmail.com)
//
// SPDX-License-Identifier: BSD-2-Clause
#include <yjk/magicfd.h>
#include <yjk/syscall.h>
#include <stdio.h>
#include <stddef.h>

static FILE s_dprnout = {.fd = MAGICFD_DPRINT};

FILE *dprnout = &s_dprnout;

void dprint(void const *buf, size_t nbyte) {
        syscall2(SYSCALL_INDEX_DPRINT, (uintptr_t)buf, nbyte);
}
