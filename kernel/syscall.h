// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#pragma once
#include <stddef.h>
#include <stdint.h>

void syscall_impl_sched_yield(void);
int64_t syscall_impl_write(int u_fd, void const *u_buf, size_t u_count);
int64_t syscall_impl_read(int u_fd, void *u_buf, size_t u_count);