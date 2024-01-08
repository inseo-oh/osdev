// SPDX-FileCopyrightText: 2023, Inseo Oh (dhdlstjtr@gmail.com)
//
// SPDX-License-Identifier: BSD-2-Clause
#pragma once
#include <stdarg.h>
#include <stddef.h>

void vstrfmt(char *buf, size_t buf_size, char const *fmt, va_list ap);
void strfmt(char *buf, size_t buf_size, char const *fmt, ...);
