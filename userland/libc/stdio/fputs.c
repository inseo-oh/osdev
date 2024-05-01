// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "stdio.h"
#include <assert.h>
#include <string.h>
#include <unistd.h>

// https://pubs.opengroup.org/onlinepubs/9699919799/functions/fputs.html
int fputs(char const *restrict s, FILE *restrict stream) {
        size_t len = strlen(s);
        if (stream->unwritten_data_max_len < stream->unwritten_data_len + len) {
                int result = fflush(stream);
                if (result == EOF) {
                        assert(!"todo: handle error");
                        return EOF;
                }
        }
        char const *src = s;
        while(len) {
                size_t copy_len = len;
                if (stream->unwritten_data_max_len < len) {
                        copy_len = stream->unwritten_data_max_len;
                }
                memcpy(stream->unwritten_data_ptr, src, copy_len);
                len -= copy_len;
                src += copy_len;
                stream->unwritten_data_len += copy_len;
                if (stream->unwritten_data_max_len <= stream->unwritten_data_len) {
                        int result = fflush(stream);
                        if (result == EOF) {
                                assert(!"todo: handle error");
                                return EOF;
                        }
                }
        }

        ssize_t result = write(stream->fd, s, strlen(s));
        if (result < 0) {
                assert(!"todo: handle error");
                return EOF;
        }
        // TODO: Update file timestamps
        return 0;
}
