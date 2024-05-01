// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "kernel.h"
#include "kernel/arch/arch.h"
#include "kernel/utility/utility.h"
#include "kernel/lock/spinlock.h"
#include "kernel/tasks/tasks.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

static struct List s_drivers;
static struct SpinLock s_lock;

void console_register_driver(struct Console_Driver *driver) {
        list_insert_tail(&s_drivers, &driver->node_head);
}

void console_flush(void) {
        for (struct Console_Driver *driver = s_drivers.head; driver;
             driver = driver->node_head.next) {
                driver->flush_fn(driver);
        }
}

void console_put_char(char chr) {
        bool got_newline = false;
        for (struct Console_Driver *driver = s_drivers.head; driver;
             driver = driver->node_head.next) {
                driver->put_char_fn(driver, chr);
                if (chr == '\n') {
                        got_newline = true;
                }
        }
        if (got_newline) {
                console_flush();
        }
}

void console_put_string(char const *str) {
        for (char const *next_ch = str; next_ch[0]; ++next_ch) {
                console_put_char(next_ch[0]);
        }
}

void console_put_string_with_pad(char const *str, size_t width) {
        size_t remaining_width = width;
        for (char const *next_ch = str; next_ch[0];
             ++next_ch, --remaining_width) {
                console_put_char(next_ch[0]);
        }
        for (; remaining_width; --remaining_width) {
                console_put_char(' ');
        }
}

void console_put_unsigned_dec(uint64_t uint) {
        unsigned digit_count = 0;
        if (uint < 10) {
                digit_count = 1;
        } else {
                for (uint64_t current = uint; current != 0;
                     current /= 10, ++digit_count) {}
        }
        ASSERT(digit_count);
        int divisor = 1;
        for (unsigned i = 1; i < digit_count; ++i, divisor *= 10) {}
        for (; divisor != 0; divisor /= 10) {
                unsigned digit = (uint / divisor) % 10;
                ASSERT(digit <= 9);
                char chr = (char)(digit + '0');
                console_put_char(chr);
        }
}

void console_put_unsigned_hex(uint64_t uint, unsigned digit_count) {
        ASSERT(digit_count);
        console_put_string("0x");
        uint64_t divisor = 1;
        for (unsigned i = 1; i < digit_count; ++i, divisor *= 16) {}
        for (; divisor != 0; divisor /= 16) {
                unsigned digit = (uint / divisor) % 16;
                ASSERT(digit <= 15);
                char chr;
                if (digit < 10) {
                        chr = (char)(digit + '0');
                } else {
                        chr = (char)((digit - 10) + 'A');
                }
                console_put_char(chr);
        }
}

int console_get_char(void) {
        for (struct Console_Driver *driver = s_drivers.head; driver;
             driver = driver->node_head.next) {
                int chr = driver->get_char_fn(driver);
                if (chr != -1) {
                        return chr;
                }
        }
        return -1;
}

int console_wait_char(void) {
        bool prev_interrupt_state = interrupts_enable();
        int chr;
        while ((chr = console_get_char()) == -1) {
                interrupts_wait();
                scheduler_yield();
        }
        if (!prev_interrupt_state) {
                interrupts_disable();
        }
        return chr;
}

size_t console_get_line(char *buf, size_t buf_size, bool include_newline) {
        ASSERT(buf_size);
        size_t remaining_len = buf_size - 1; // Leave room for '\0'
        char *dest = buf;
        size_t out_len = 0;
        dest[0] = '\0';
        while (1) {
                int chr = console_wait_char();
                if (chr == '\r') {
                        chr = '\n';
                }
                switch (chr) {
                case '\n':
                        if (!include_newline) {
                                return out_len;
                        }
                        break;
                case '\x7f':
                case '\b':
                        if (dest != buf) {
                                dest[0] = '\0';
                                --dest;
                                ++remaining_len;
                                console_put_string("\b \b");
                        }
                        continue;
                default:
                        break;
                }
                bool is_non_printable =
                        ((chr < 0x20) || (0x7F <= chr)) && (chr != '\n');
                if (is_non_printable) {
                        continue;
                }
                if (remaining_len == 0) {
                        // TODO: Buffer the input so that it can be read by
                        //       later call.
                        TODO();
                }
                console_put_char((char)chr);
                console_flush();
                dest[0] = (char)chr;
                dest[1] = '\0';
                ++out_len;
                --remaining_len;
                ++dest;
                if (include_newline) {
                        switch (chr) {
                        case '\n':
                        case '\r':
                                return out_len;
                        default:
                                break;
                        }
                }
        }
}

void console_vprintf(char const *fmt, va_list argptr) {
        enum {
                PERCENT_OR_CHAR,      // % or a char(No conversion)
                TYPE_SPECIFIER,       // (Optional) l
                CONVERSION_SPECIFIER, // What kind of conversion?
        } stage = PERCENT_OR_CHAR;

        enum {
                INT_TYPE_INT,
                INT_TYPE_LONG,
        } int_type = INT_TYPE_INT;

        for (char const *fmt_ch = fmt; fmt_ch[0]; ++fmt_ch) {
                char chr = fmt_ch[0];
                switch (stage) {
                case PERCENT_OR_CHAR:
                        if (chr == '%') {
                                stage = TYPE_SPECIFIER;
                        } else {
                                console_put_char(chr);
                        }
                        break;
                case TYPE_SPECIFIER:
                        if (chr == 'l') {
                                int_type = INT_TYPE_LONG;
                        } else {
                                // Not a type specifier
                                --fmt_ch; // Use the same char next time
                        }
                        stage = CONVERSION_SPECIFIER;
                        break;
                case CONVERSION_SPECIFIER:
                        switch (chr) {
                        case 'c':
                                console_put_char(va_arg(argptr, int));
                                break;
                        case 's':
                                console_put_string(va_arg(argptr, char *));
                                break;
                        case 'u':
                                switch (int_type) {
                                case INT_TYPE_INT:
                                        console_put_unsigned_dec(
                                                va_arg(argptr, unsigned)
                                        );
                                        break;
                                case INT_TYPE_LONG:
                                        console_put_unsigned_dec(
                                                va_arg(argptr, unsigned long)
                                        );
                                        break;
                                }
                                break;
                        case 'x':
                                switch (int_type) {
                                case INT_TYPE_INT:
                                        console_put_unsigned_hex(
                                                va_arg(argptr, unsigned), 8
                                        );
                                        break;
                                case INT_TYPE_LONG:
                                        console_put_unsigned_hex(
                                                va_arg(argptr, unsigned long),
                                                16
                                        );
                                        break;
                                }
                                break;
                        case 'p':
                                console_put_unsigned_hex(
                                        va_arg(argptr, uintptr_t), 16
                                );
                                break;
                        default:
                                panic("Unsupported conversion specifier");
                        }
                        stage = PERCENT_OR_CHAR;
                        break;
                }
        }
        console_flush();
}

void console_printf(char const *fmt, ...) {
        va_list argptr;
        va_start(argptr, fmt);
        console_vprintf(fmt, argptr);
        va_end(argptr);
}

void console_valert(char const *fmt, va_list argptr) {
        // console_put_string(SGR_FG_RED);
        console_vprintf(fmt, argptr);
        // console_put_string(SGR_RESET "\n");
        console_put_string("\n");
}

void console_alert(char const *fmt, ...) {
        va_list argptr;
        va_start(argptr, fmt);
        console_valert(fmt, argptr);
        va_end(argptr);
}

void console_log(loglevel_t level, char const *tag, char const *fmt, ...) {
        bool prev_interrupt_state;
        spinlock_lock(&s_lock, &prev_interrupt_state);

        uint32_t ticks = ticktime_get_count();
        unsigned ticks_secs = ticks / 1000;
        unsigned ticks_ms = ticks % 1000;
        console_printf(
                "%c [%u.%c%c%c] %s | ",
                level,
                ticks_secs,

                // console_printf doesn't support width, so we handle padding
                // manually :D
                ((ticks_ms / 100) % 10) + '0',
                ((ticks_ms / 10) % 10) + '0',
                (ticks_ms % 10) + '0',

                tag
        );
        va_list argptr;
        va_start(argptr, fmt);
        console_vprintf(fmt, argptr);
        console_put_string("\n");
        va_end(argptr);

        spinlock_unlock(&s_lock, prev_interrupt_state);
}
