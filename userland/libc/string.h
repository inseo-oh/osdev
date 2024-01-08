// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#pragma once
#include "sys/types.h"

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/string.h.html

#if defined(__cplusplus)
extern "C" {
#endif

void *memcpy(void *restrict s1, void const *restrict s2, size_t n);
void *memset(void *s, int c, size_t n);
size_t strlen(char const *s);
char *strcpy(char *restrict s1, char const *restrict s2);
char *strcat(char *restrict s1, char const *restrict s2);
char *strchr(char const *s, int c);

#if defined(__cplusplus)
}
#endif
