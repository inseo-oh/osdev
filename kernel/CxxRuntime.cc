
// SPDX-FileCopyrightText: (c) 2024 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause

extern "C" {
#define NORETURN_WORKAROUND
#include "kernel/kernel.h"
}

namespace Kernel::CxxRuntime {

extern "C" void __cxa_pure_virtual() {
    panic("__cxa_pure_virtual called");
}

}
