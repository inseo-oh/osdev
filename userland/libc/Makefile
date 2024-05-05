# SPDX-FileCopyrightText: (c) 2024 Inseo Oh <dhdlstjtr@gmail.oom>
#
# SPDX-License-Identifier: BSD-2-Clause

ifndef YJK_PROJECT_ROOT
$(error YJK_PROJECT_ROOT is not set! Make sure you run the top-level Makefile)
endif

YJK_TARGET_NAME = libc.a
YJK_TARGET_TYPE = STATIC_LIB

YJK_OBJS  = assert.o errno.o
YJK_OBJS += unistd/write.o unistd/read.o
YJK_OBJS += sched/sched_yield.o
YJK_OBJS += stdio/stdio.o stdio/fputs.o stdio/fputc.o stdio/fwrite.o stdio/fflush.o stdio/fprintf.o
YJK_OBJS += string/memcpy.o string/strlen.o
YJK_OBJS += yjk/syscall.o yjk/dprint.o

YJK_INSTALL_DIR = ${YJK_SYS_ROOT}/usr/lib

include $(YJK_PROJECT_ROOT)/support/common.mk

