// SPDX-FileCopyrightText: 2024, Inseo Oh (dhdlstjtr@gmail.com)
//
// SPDX-License-Identifier: BSD-2-Clause
#pragma once
#include <stddef.h>

typedef struct FILE FILE;

extern FILE *dprnout;

void dprint(void const *buf, size_t nbyte);
