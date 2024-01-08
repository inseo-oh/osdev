// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#pragma once
#include <stddef.h>

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/stdlib.h.html

#if defined(__cplusplus)
extern "C" {
#endif

void exit(int status);
void abort(void);
void *malloc(size_t size);
void free(void *ptr);
int atexit(void (*func)(void));
int atoi(char const *str);
char *getenv(char const *name);
void *calloc(size_t nelem, size_t elsize);
int abs(int i);

#if defined(__cplusplus)
}
#endif