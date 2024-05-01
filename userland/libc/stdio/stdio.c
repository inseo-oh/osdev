// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "stdio.h"

static FILE s_stdout, s_stderr = {.fd = 1};

FILE *stdout = &s_stdout;
FILE *stderr = &s_stderr;

// https://pubs.opengroup.org/onlinepubs/9699919799/functions/fdopen.html
FILE *__libc_fdopen_inner(FILE *out, int filedes, char const *mode) {
        (void)mode;
        out->unwritten_data_ptr = out->unwritten_data;
        out->unwritten_data_len = 0;
        out->unwritten_data_max_len = sizeof(out->unwritten_data)/sizeof(*out->unwritten_data);
        out->fd = filedes;
        return out;
}
