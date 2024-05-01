// SPDX-FileCopyrightText: (c) 2024 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "assert.h"
#include <yjk/dprint.h>

static const char MSG[] = "Assertion failed";

void __assert_fail(char const *s, char const *file, int line) {
        (void)s;
        (void)file;
        (void)line;
        dprint(MSG, sizeof(MSG)/sizeof(*MSG) - 1);
        while(1);
}
