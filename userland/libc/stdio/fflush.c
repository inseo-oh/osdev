// SPDX-FileCopyrightText: (c) 2024 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "stdio.h"
#include <assert.h>
#include <unistd.h>
#include <yjk/magicfd.h>
#include <yjk/dprint.h>

// https://pubs.opengroup.org/onlinepubs/9699919799/functions/fflush.html
int fflush(FILE *stream) {
        // TODO: Support read mode streams
        if (!stream->unwritten_data_len) {
                return 0;
        }
        switch(stream->fd) {
        case MAGICFD_NOFD:
                break;
        case MAGICFD_DPRINT:
                dprint(stream->unwritten_data_ptr, stream->unwritten_data_len);
                stream->unwritten_data_len = 0;
                break;
        default: {
                ssize_t result = write(stream->fd, stream->unwritten_data_ptr, stream->unwritten_data_len);
                if (result < 0) {
                        assert(!"TODO: Handle error"); 
                        return EOF;
                }
                stream->unwritten_data_len = 0;
                // TODO: Update file timestamps
                break;
        }
        }
        return 0;
}
