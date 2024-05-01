// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause

#include "string.h"

// https://pubs.opengroup.org/onlinepubs/9699919799/functions/strlen.html
size_t strlen(char const *s) {
        size_t len = 0;
        for (; *s; ++s, ++len) {}
        return len;
}
