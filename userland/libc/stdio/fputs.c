// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "stdio.h"
#include <string.h>
#include <unistd.h>

// https://pubs.opengroup.org/onlinepubs/9699919799/functions/fputs.html
int fputs(char const *restrict s, FILE *restrict stream) {
        ssize_t result = write(stream->fd, s, strlen(s));
        if (result < 0) {
                __builtin_trap(); // TODO: Handle error
                return EOF;
        }
        // TODO: Update file timestamps
        return 0;
}
