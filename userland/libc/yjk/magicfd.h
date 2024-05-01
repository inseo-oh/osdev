// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// Magic file descriptor numbers. Libc has special behaviors when detects one of these values.

#if defined(__cplusplus)
extern "C" {
#endif

#define MAGICFD_DPRINT  -0x4450524e // 'DPRN'  |  Use DPRINT syscall, which goes directly to kernel's console output.
#define MAGICFD_NOFD    -0x4e4f4644 // 'NOFD'  |  The output doesn't go to any actual FD or console. Intended to be used by functions like snprintf.

#if defined(__cplusplus)
}
#endif