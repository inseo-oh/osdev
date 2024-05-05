# SPDX-FileCopyrightText: (c) 2024 Inseo Oh <dhdlstjtr@gmail.oom>
#
# SPDX-License-Identifier: BSD-2-Clause

ifndef YJK_PROJECT_ROOT
$(error YJK_PROJECT_ROOT is not set! Make sure you run the top-level Makefile)
endif

YJK_TARGET_NAME = crt0.o
YJK_OBJS        = crt0.o
YJK_TARGET_TYPE = MANUAL_LINK
YJK_INSTALL_DIR = ${YJK_SYS_ROOT}/usr/lib

include $(YJK_PROJECT_ROOT)/support/common.mk


$(YJK_TARGET):
	cp $(YJK_LINK_OBJS) $@
