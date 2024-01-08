// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#pragma once
#include <stddef.h>

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/sys_types.h.html

#if defined(__cplusplus)
extern "C" {
#endif

typedef int pid_t;
typedef size_t size_t;
typedef long long ssize_t;

#if defined(__cplusplus)
}
#endif