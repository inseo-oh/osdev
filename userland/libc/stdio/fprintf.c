// SPDX-FileCopyrightText: (c) 2024 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int print_unsigned_dec(FILE *file, unsigned long i) {
    int written_len = 0;
    unsigned long divisor = 1;
    {
        unsigned long current = i;
        while (current / 10) {
            divisor *= 10;
            current /= 10;
        }
    }
    for (; divisor; divisor /= 10) {
        int digit = (i / divisor) % 10;
        if (fputc('0' + digit, file) == EOF) {
            assert(!"TODO: Handle error");
        }
        written_len++;
    }
    return written_len;
}

static int print_signed_dec(FILE *file, long i) {
    int written_len = 0;
    if (i < 0) {
        if (fputc('-', file) == EOF) {
            // TODO: Handle error
            assert(!"todo: handle error");
        }
        written_len++;
        i = -i;
    }
    print_unsigned_dec(file, i);
    return written_len;
}

static int print_hex(FILE *file, unsigned long i, bool uppercase) {
    int written_len = 0;
    char a = uppercase ? 'A' : 'a';
    unsigned long divisor = 1;
    {
        unsigned long current = i;
        while (current / 16) {
            divisor *= 16;
            current /= 16;
        }
    }
    for (; divisor; divisor /= 16) {
        int digit = (i / divisor) % 16;
        int result;
        if (digit < 10) {
            result = fputc('0' + digit, file);
        } else {
            result = fputc(a + (digit - 10), file);
        }
        if (result == EOF) {
            // TODO: Handle error
            assert(!"todo: handle error");
        }
        written_len++;
    }
    return written_len;
}

#define FMT_FLAG_ALTERNATE_FORM (1 << 0)

int vfprintf(FILE *file, char const *fmt, va_list ap) {
    uint8_t flags = 0;
    int written_len = 0;

percent_or_char:
    if (!fmt[0]) {
        goto end;
    }
    if (fmt[0] == '%') {
        ++fmt;
        goto fmt_flag;
    }
    fputc(fmt[0], file);
    written_len++;
    fmt++;
    goto percent_or_char;
fmt_flag:
    if (!fmt[0]) {
        goto end;
    }
    switch(fmt[0]) {
        case '#':
            flags |= FMT_FLAG_ALTERNATE_FORM;
            break;
        default:
            goto do_fmt;
    }
    ++fmt;
    goto fmt_flag;
do_fmt:
    if (!fmt[0]) {
        goto end;
    }
    switch(fmt[0]) {
        case 'c': {
            char c = va_arg(ap, int);
            if (fputc(c, file) == EOF) {
                // TODO: Handle error
                assert(!"todo: handle error");
            }
            written_len++;
            break;
        }
        case 's': {
            char const *s = va_arg(ap, char *);
            if (fputs(s, file) == EOF) {
               // TODO: Handle error
                assert(!"todo: handle error");
            }
            written_len += strlen(s);
            break;
        }
        case 'd': {
            int d = va_arg(ap, int);
            int len = print_signed_dec(file, d);
            if (len == EOF) {
               // TODO: Handle error
                assert(!"todo: handle error");
            }
            len += written_len;
            break;
        }
        case 'u': {
            unsigned u = va_arg(ap, unsigned);
            int len = print_unsigned_dec(file, u);
            if (len == EOF) {
               // TODO: Handle error
                assert(!"todo: handle error");
            }
            len += written_len;
            break;
        }
        case 'x':
        case 'X': {
            bool is_uppercase = fmt[0] == 'X';
            if (flags & FMT_FLAG_ALTERNATE_FORM) {
                if (fputs(is_uppercase ? "0X" : "0x", file) == EOF) {
                    // TODO: Handle error
                    assert(!"todo: handle error");
                }
                written_len += 2;
            }
            unsigned u = va_arg(ap, unsigned);
            int len = print_hex(file, u, is_uppercase);
            if (len == EOF) {
               // TODO: Handle error
                assert(!"todo: handle error");
            }
            written_len += len;
            break;
        }
        case 'p': {
            void *p = va_arg(ap, void *);
            if (fputs("0X", file) == EOF) {
               // TODO: Handle error
                assert(!"todo: handle error");
            }
            written_len += 2;
            int len = print_hex(file, (uintptr_t)p, true);
            if (len == EOF) {
               // TODO: Handle error
                assert(!"todo: handle error");
            }
            written_len += len;
            break;
        }
        default:
            if (fputc('?', file) == EOF) {
                // TODO: Handle error
                assert(!"todo: handle error");
            }
            written_len++;
            break;
    }
    ++fmt;
    goto percent_or_char;
end:
    va_end(ap);
    return written_len;
}

int fprintf(FILE *file, char const *fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        int result = vfprintf(file, fmt, ap);
        va_end(ap);
        return result;
}
