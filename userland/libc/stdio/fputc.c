// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "stdio.h"
#include <assert.h>
#include <unistd.h>
#include <yjk/magicfd.h>
#include <yjk/dprint.h>

// https://pubs.opengroup.org/onlinepubs/9699919799/functions/fputc.html
int fputc(int c, FILE *stream) {
        if (stream->unwritten_data_len == stream->unwritten_data_max_len) {
                int result = fflush(stream);
                if (result == EOF) {
                        assert(!"todo: handle error");
                        return EOF;
                }
        }
        stream->unwritten_data[stream->unwritten_data_len] = c;
        stream->unwritten_data_len++;
        if ((stream->unwritten_data_len == stream->unwritten_data_max_len) || (c == '\n')) {
                int result = fflush(stream);
                if (result == EOF) {
                        assert(!"todo: handle error");
                        return EOF;
                }
        }
        return 0;
}
