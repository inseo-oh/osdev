// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#ifdef YJK_ULTRA_PARANOID_MODE
#include "kernel/kernel.h"
#include <stdint.h>
#include <stdnoreturn.h>

struct SourceLocation {
        char const *file_name;
        uint32_t line;
        uint32_t column;
};

static noreturn void
log_ub_and_die(struct SourceLocation loc, char const *ub_name) {
        panic("UBSAN: %s @ %s:%u:%u\n",
              ub_name,
              loc.file_name,
              loc.line,
              loc.column);
}

// UBSan provides much more information other than just location, but this is
// still useful than nothing.
#define HANDLER(__ub_name)                                            \
        void __ubsan_handle_##__ub_name(struct SourceLocation *loc) { \
                log_ub_and_die(*loc, #__ub_name);                     \
        }

HANDLER(type_mismatch_v1)
HANDLER(pointer_overflow)
HANDLER(shift_out_of_bounds)
HANDLER(load_invalid_value)
HANDLER(divrem_overflow)
HANDLER(out_of_bounds)
HANDLER(add_overflow)
HANDLER(mul_overflow)
HANDLER(sub_overflow)
HANDLER(negate_overflow)
#endif
