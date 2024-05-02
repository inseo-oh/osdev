// SPDX-FileCopyrightText: (c) 2024 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#pragma once

namespace Kernel::Idt {
    void init_bsp(void);
    void init_ap(void);
    void use_ist1(void);
}