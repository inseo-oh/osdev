// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#pragma once
#error Use utility.h instead
#include <stdbool.h>
#include <stddef.h>

void kmemset(void *dest, int byte, size_t len);
bool mem_equals(void const *mem1, void const *mem2, size_t n);
void kmemcpy(void *__restrict__ dest, void const *__restrict__ src, size_t len);
void str_copy(
        char *__restrict__ dest, size_t dest_size, char const *__restrict__ src
);
bool str_equals_up_to(char const *str1, char const *str2, size_t n);
bool str_equals(char const *str1, char const *str2);
size_t kstrlen(char const *str);
