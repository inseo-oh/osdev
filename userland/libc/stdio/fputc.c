// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "stdio.h"
#include <unistd.h>

// https://pubs.opengroup.org/onlinepubs/9699919799/functions/fputc.html
int fputc(int c, FILE *stream) {
        unsigned char chr = c;
        ssize_t result = write(stream->fd, &chr, 1);
        if (result < 0) {
                __builtin_trap(); // TODO: Handle error
                return EOF;
        }
        // TODO: Update file timestamps
        return 0;
}
