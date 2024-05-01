// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#pragma once
#include "kernel/utility/utility.h"
#include <stdbool.h>
#include <stddef.h>

WARN_UNUSED_RESULT int
cliarg_next_str(char *out, size_t buf_size, char **arg_str);
WARN_UNUSED_RESULT bool cliarg_next_unsigned(unsigned *out, char **arg_str);
