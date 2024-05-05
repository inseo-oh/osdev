# SPDX-FileCopyrightText: (c) 2024 Inseo Oh <dhdlstjtr@gmail.oom>
#
# SPDX-License-Identifier: BSD-2-Clause
.code64
.intel_syntax noprefix

# Footer for _init function
.section .init
_init:
        pop rbp
        ret

# Footer for _fini function
.section .fini
_fini:
        pop rbp
        ret
	    