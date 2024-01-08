// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#pragma once
#include "sys/types.h"
#include <stdint.h>

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/unistd.h.html

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#if defined(__cplusplus)
extern "C" {
#endif

typedef pid_t pid_t;
typedef intptr_t intptr_t;

pid_t fork(void);
int execv(char const *path, char *const argv[]);
int execve(char const *path, char *const argv[], char *const envp[]);
int execvp(char const *file, char *const argv[]);
pid_t getpid(void);
ssize_t write(int fildes, void const *buf, size_t nbyte);
ssize_t read(int fildes, void *buf, size_t nbyte);

#if defined(__cplusplus)
}
#endif