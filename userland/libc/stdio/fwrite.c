// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "stdio.h"
#include <unistd.h>

// https://pubs.opengroup.org/onlinepubs/9699919799/functions/fwrite.html
size_t fwrite(void const *restrict ptr, size_t size, size_t nitems, FILE *restrict stream) {
        if (!size || !nitems) {
                return 0;
        }
        for (size_t i = 0; i < nitems; ++i) {
                uint8_t *item = &((uint8_t *)ptr)[size * i];
                for (size_t j = 0; j < size; ++j) {
                        fputc(item[j], stream);
                }
        }
        // TODO: Update file timestamps
        return 0;
}
