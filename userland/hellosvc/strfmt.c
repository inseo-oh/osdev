// SPDX-FileCopyrightText: 2023, Inseo Oh (dhdlstjtr@gmail.com)
//
// SPDX-License-Identifier: BSD-2-Clause
#include "strfmt.h"
#include <stdarg.h>
#include <stdint.h>

typedef struct BufContext BufContext;

struct BufContext {
        char *next_char;
        unsigned remaining_len;
};

static void put_char(BufContext *bc, char ch) {
        if (!bc->remaining_len) {
                return;
        }
        bc->next_char[0] = ch;
        bc->next_char[1] = '\0';
        ++bc->next_char;
        --bc->remaining_len;
}

static void put_string(BufContext *bc, char const *str) {
        for (char const *next_ch = str; next_ch[0]; ++next_ch) {
                put_char(bc, next_ch[0]);
        }
}

static void put_unsigned_dec(BufContext *bc, uint64_t u) {
        unsigned digit_count = 0;
        if (u < 10) {
                digit_count = 1;
        } else {
                for (uint64_t current = u; current != 0;
                     current /= 10, ++digit_count) {}
        }
        uint64_t divisor = 1;
        for (unsigned i = 1; i < digit_count; ++i, divisor *= 10) {}
        for (; divisor != 0; divisor /= 10) {
                unsigned digit = (u / divisor) % 10;
                char ch = (char)(digit + '0');
                put_char(bc, ch);
        }
}

static void put_unsigned_hex(BufContext *bc, uint64_t u, unsigned digit_count) {
        put_string(bc, "0x");
        uint64_t divisor = 1;
        for (unsigned i = 1; i < digit_count; ++i, divisor *= 16) {}
        for (; divisor != 0; divisor /= 16) {
                unsigned digit = (u / divisor) % 16;
                char ch;
                if (digit < 10) {
                        ch = (char)(digit + '0');
                } else {
                        ch = (char)(digit - 10 + 'A');
                }
                put_char(bc, ch);
        }
}

void vstrfmt(char *buf, size_t buf_size, char const *fmt, va_list ap) {
        enum {
                PERCENT_OR_CHAR,      // % or a char(No conversion)
                TYPE_SPECIFIER,       // (Optional) l
                CONVERSION_SPECIFIER, // What kind of conversion?
        } stage = PERCENT_OR_CHAR;

        BufContext ctx = {.next_char = buf, .remaining_len = buf_size - 1};
        buf[0] = '\0'; // XXX: Workaround for readability-non-const-parameter

        enum {
                INT_TYPE_INT,
                INT_TYPE_LONG,
        } int_type = INT_TYPE_INT;

        for (char const *fmt_ch = fmt; fmt_ch[0]; ++fmt_ch) {
                char ch = fmt_ch[0];
                switch (stage) {
                case PERCENT_OR_CHAR:
                        if (ch == '%') {
                                stage = TYPE_SPECIFIER;
                        } else {
                                put_char(&ctx, ch);
                        }
                        break;
                case TYPE_SPECIFIER:
                        if (ch == 'l') {
                                int_type = INT_TYPE_LONG;
                        } else {
                                // Not a type specifier
                                --fmt_ch; // Use the same char next time
                        }
                        stage = CONVERSION_SPECIFIER;
                        break;
                case CONVERSION_SPECIFIER:
                        switch (ch) {
                        case 'c':
                                put_char(&ctx, va_arg(ap, int));
                                break;
                        case 's':
                                put_string(&ctx, va_arg(ap, char *));
                                break;
                        case 'u':
                                switch (int_type) {
                                case INT_TYPE_INT:
                                        put_unsigned_dec(
                                                &ctx, va_arg(ap, unsigned)
                                        );
                                        break;
                                case INT_TYPE_LONG:
                                        put_unsigned_dec(
                                                &ctx, va_arg(ap, unsigned long)
                                        );
                                        break;
                                }
                                break;
                        case 'x':
                                switch (int_type) {
                                case INT_TYPE_INT:
                                        put_unsigned_hex(
                                                &ctx, va_arg(ap, unsigned), 8
                                        );
                                        break;
                                case INT_TYPE_LONG:
                                        put_unsigned_hex(
                                                &ctx,
                                                va_arg(ap, unsigned long),
                                                16
                                        );
                                        break;
                                }
                                break;
                        case 'p':
                                put_unsigned_hex(
                                        &ctx, va_arg(ap, uintptr_t), 16
                                );
                                break;
                        default:
                                put_char(&ctx, '?');
                        }
                        stage = PERCENT_OR_CHAR;
                        break;
                }
        }
}

void strfmt(char *buf, size_t buf_size, char const *fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        vstrfmt(buf, buf_size, fmt, ap);
        va_end(ap);
}
