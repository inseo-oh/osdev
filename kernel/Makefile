# SPDX-FileCopyrightText: (c) 2024 Inseo Oh <dhdlstjtr@gmail.oom>
#
# SPDX-License-Identifier: BSD-2-Clause

ifndef YJK_PROJECT_ROOT
$(error YJK_PROJECT_ROOT is not set! Make sure you run the top-level Makefile)
endif

YJK_TARGET_NAME = kernel
YJK_TARGET_TYPE = EXEC

# Root sources
YJK_OBJS = console.o panic.o ticktime.o syscall.o acpi.o videoconsole.o objpath.o CxxRuntime.o
# Multitasking
YJK_OBJS += tasks/scheduler.o tasks/thread.o tasks/process.o tasks/exec.o
# Memory management
YJK_OBJS += memory/virtzone.o memory/physpage.o memory/physzone.o
# Interrupts
YJK_OBJS += interrupt/interrupts.o
# Locking support
YJK_OBJS += lock/spinlock.o lock/mutex.o
# Kernel Heap
YJK_OBJS += heap/kmalloc.o heap/vmmalloc.o
# Kernel CLI
YJK_OBJS += cli/cli.o cli/cliarg.o
YJK_OBJS += cli/clicmd_testmalloc.o
YJK_OBJS += cli/clicmd_testpagealloc.o
# Kernel internal utilities
YJK_OBJS += utility/list.o utility/ubsan.o utility/avltree.o utility/strutil.o utility/queue.o utility/bitmap.o

# Architecture-specific objects
ifeq ($(YJK_ARCH), x86)
YJK_OBJS += arch/x86/entry.o
YJK_OBJS += arch/x86/i8254timer.o arch/x86/i8259pic.o arch/x86/Idt.o
YJK_OBJS += arch/x86/ioapic.o arch/x86/ioport.o arch/x86/Isr.o
YJK_OBJS += arch/x86/interrupts.o
YJK_OBJS += arch/x86/lapic.o
YJK_OBJS += arch/x86/madt.o 
YJK_OBJS += arch/x86/mmu.o 
YJK_OBJS += arch/x86/processor.o 
YJK_OBJS += arch/x86/SmpBoot.o arch/x86/stacktrace.o arch/x86/Syscall.o
YJK_OBJS += arch/x86/uartconsole.o arch/x86/uaccess.o
# - Invalid architecture
else
$(error $(YJK_ARCH) is not a valid architecture)
endif

# CRT objects
YJK_EXPLICIT_CRT = 1
YJK_CRTI_OBJ     = crt/crti.o
YJK_CRTN_OBJ     = crt/crtn.o

YJK_INSTALL_DIR = ${YJK_BOOT_ROOT}/yjk

include $(YJK_PROJECT_ROOT)/support/common.mk

YJK_COMMON_FLAGS += -nostdlib -ffreestanding -mno-red-zone -mcmodel=kernel
YJK_LINK_FLAGS   += -T arch/$(YJK_ARCH)/link.ld

