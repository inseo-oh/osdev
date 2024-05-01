// SPDX-FileCopyrightText: (c) 2024 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#pragma once

void __assert_fail(char const *s, char const *file, int line);

#define assert(_x)  (void)((_x) ? ((void)0) : __assert_fail(#_x, __FILE__, __LINE__))