# SPDX-FileCopyrightText: (c) 2024 Inseo Oh <dhdlstjtr@gmail.oom>
#
# SPDX-License-Identifier: BSD-2-Clause
.code64
.intel_syntax noprefix

# Header for _init function
.section .init
.global _init
_init:
        push rbp
        mov rbp, rsp

# Header for _fini function
.section .fini
.global _fini
_fini:
	push rbp
	mov rbp, rsp
