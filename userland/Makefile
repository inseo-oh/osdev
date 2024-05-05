# SPDX-FileCopyrightText: (c) 2024 Inseo Oh <dhdlstjtr@gmail.oom>
#
# SPDX-License-Identifier: BSD-2-Clause

all:
	$(MAKE) -C libc_crt0 $(YJK_MAKE_FLAGS)
	$(MAKE) -C libc $(YJK_MAKE_FLAGS)
	$(MAKE) -C hellosvc $(YJK_MAKE_FLAGS)

clean:
	$(MAKE) -C libc_crt0 clean $(YJK_MAKE_FLAGS)
	$(MAKE) -C libc clean $(YJK_MAKE_FLAGS)
	$(MAKE) -C hellosvc clean $(YJK_MAKE_FLAGS)