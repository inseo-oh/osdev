// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "cliarg.h"
#include "kernel/kernel.h"
#include "kernel/utility/utility.h"
#include <stdbool.h>

// TODO: Move this to somewhere else
static bool is_whitespace(char chr) {
        switch (chr) {
        case ' ':
        case '\t':
                return true;
        default:
                return false;
        }
}

static void skip_spaces(char **str) {
        for (; **str && is_whitespace(**str); ++(*str)) {}
}

WARN_UNUSED_RESULT int
cliarg_next_str(char *out, size_t buf_size, char **arg_str) {
        skip_spaces(arg_str);
        unsigned max_len = buf_size - 1;
        unsigned len = 0;
        char *dest = out;
        for (; **arg_str && !is_whitespace(**arg_str);
             ++(*arg_str), ++len, ++dest) {
                if (max_len <= len) {
                        console_alert(
                                "Buffer overflow while processing the command "
                                "line (Max length: %u)",
                                max_len
                        );
                        return -1;
                }
                dest[0] = **arg_str;
                dest[1] = '\0';
        }
        return (int)len;
}

WARN_UNUSED_RESULT bool cliarg_next_unsigned(unsigned *out, char **arg_str) {
        skip_spaces(arg_str);
        unsigned result = 0;
        bool found_digit = false;
        for (; **arg_str && !is_whitespace(**arg_str); ++(*arg_str)) {
                found_digit = true;
                unsigned chr = (unsigned)**arg_str;
                if ((chr < '0') || ('9' < chr)) {
                        console_alert("Expected decimal digit, got %c", chr);
                        return false;
                }
                unsigned digit = chr - '0';
                unsigned new_result = result * 10 + digit;
                if (new_result < result) {
                        console_alert(
                                "Integer overflow while processing the command "
                                "line"
                        );
                        return false;
                }
                result = new_result;
        }
        *out = result;
        return found_digit;
}
