// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#pragma once
#include <stdarg.h>
#include <sys/types.h>

#ifdef __LIBC_INTERNAL
typedef struct FILE FILE;
FILE *__libc_fdopen_inner(FILE *out, int filedes, char const *mode);
#endif

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/stdio.h.html

#if defined(__cplusplus)
extern "C" {
#endif

#define SEEK_CUR 0
#define SEEK_END 1
#define SEEK_SET 2

#define EOF  (-1)

// NOTE: Contents of FILE is not specified by standard, meaning *nobody* but implementation
//       is supposed to access any of these fields.
//       Which is great because we can put whatever hell needed to manage file I/O :D
typedef struct FILE {
        // While the FILE has its own small buffer, it may be provided externally as well.
        // This is to allow functions like snprintf point to the destination buffer instead of
        // the internal one. 
        char unwritten_data[256];
        char *unwritten_data_ptr;
        size_t unwritten_data_len, unwritten_data_max_len;
        int fd; 
} FILE;

typedef size_t size_t;

extern FILE *stdout;
extern FILE *stderr;

FILE *fopen(char const *restrict pathname, char const *restrict mode);
int fflush(FILE *stream);
int fclose(FILE *stream);
int fputs(char const *restrict s, FILE *restrict stream);
int fputc(int c, FILE *stream);
size_t
fread(void *restrict ptr, size_t size, size_t nitems, FILE *restrict stream);
size_t
fwrite(void const *restrict ptr,
       size_t size,
       size_t nitems,
       FILE *restrict stream);
void setbuf(FILE *restrict stream, char *restrict buf);
int vfprintf(FILE *restrict stream, char const *restrict format, va_list ap);
int sprintf(char *restrict s, char const *restrict format, ...);
int fprintf(FILE *restrict stream, char const *restrict format, ...);
int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);
int feof(FILE *stream);

#if defined(__cplusplus)
}
#endif
