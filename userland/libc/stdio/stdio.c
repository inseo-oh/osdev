// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "stdio.h"

static FILE s_stdout = {.fd = 1};
static FILE s_stderr = {.fd = 2};

FILE *stdout = &s_stdout;
FILE *stderr = &s_stderr;