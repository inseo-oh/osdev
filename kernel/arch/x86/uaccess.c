// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "kernel/arch/arch.h"
#include <stdbool.h>

static bool is_smap_supported() {
        ENTER_NO_INTERRUPT_SECTION();
        bool is_smap_supported = processor_current()->flags &
                                 PROCESSOR_LOCALSTATE_FLAG_X86_SMAP_SUPPORTED;
        LEAVE_NO_INTERRUPT_SECTION();
        return is_smap_supported;
}

void uaccess_begin() {
        if (!is_smap_supported()) {
                return;
        }
        __asm__ volatile("stac");
}

void uaccess_end() {
        if (!is_smap_supported()) {
                return;
        }
        __asm__ volatile("clac");
}
