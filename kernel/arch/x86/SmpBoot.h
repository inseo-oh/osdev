// SPDX-FileCopyrightText: (c) 2024 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#pragma once
#include <stddef.h>

#define SMPBOOT_AP_BOOT_CODE_PHYS_BASE 0x8000

namespace Kernel::SmpBoot {

size_t ap_boot_code_page_count();
void ap_did_boot(void);
size_t next_ap_to_init(void);
void start(void);

}