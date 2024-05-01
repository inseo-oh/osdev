// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#define __LIBC_INTERNAL
#include "stdio.h"
#include <yjk/magicfd.h>
#include <yjk/dprint.h>

extern int main(int argc, char **argv);

void _start() {
        __libc_fdopen_inner(stdout, 1, "w");
        __libc_fdopen_inner(stderr, 2, "w");
        __libc_fdopen_inner(dprnout, MAGICFD_DPRINT, "w");
        char *argv[] = {"<todo: argv>"};
        main(1, argv);
        while (1) {}
}
