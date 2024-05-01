// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#pragma once
#include "kernel/lock/spinlock.h"
#include "kernel/utility/utility.h"
#include <stdbool.h>
#include <stdint.h>

struct X86_BaseSegmentDescriptor {
        uint16_t limit_b15_to_b0;
        uint16_t base_b15_to_b0;
        uint8_t base_b23_to_b16;
        uint8_t access_byte;
        uint8_t limit_b19_to_b16_and_flags;
        uint8_t base_b31_to_b24;
};

// Must be placed right after X86_BaseSegmentDescriptor, as this is extension to
// normal segment descriptors.
struct X86_SystemSegmentDescriptorExt {
        uint32_t base_b63_to_b32;
        uint32_t _reserved;
};

union X86_SegmentDescriptor {
        struct X86_BaseSegmentDescriptor base;
        struct X86_SystemSegmentDescriptorExt system_descriptor_ext;
};

struct X86_TSS_Addr64 {
        uint32_t low;
        uint32_t high;
};

struct X86_TSS {
        uint32_t _reserved0;
        struct X86_TSS_Addr64 rsp[3];
        uint32_t _reserved1[2];
        // Note that IST starts from 1, so ist[0] is IST1, ist[1] is IST2, ...
        struct X86_TSS_Addr64 ist[7];
        uint32_t _reserved2[2];
        uint16_t _reserved3;
        uint16_t iopb;
};

// Indices for Processor_LocalState's misc_state array
#define X86_MISC_STATE_SYSCALL_KERNEL_STACK_TOP_INDEX 0
#define X86_MISC_STATE_SYSCALL_SAVED_USER_RSP_INDEX   1
#define X86_MISC_STATE_SYSCALL_SAVED_USER_RBP_INDEX   2
// NOTE: This GS.Base field is only used when "expensive" GS.Base check method
//       was used.
#define X86_MISC_STATE_SAVED_GS_BASE_UPPER32_INDEX 3
#define X86_MISC_STATE_VALUES_COUNT                4

#define X86_MISC_STATE_SYSCALL_KERNEL_STACK_TOP_OFFSET \
        (8 * X86_MISC_STATE_SYSCALL_KERNEL_STACK_TOP_INDEX)
#define X86_MISC_STATE_SYSCALL_SAVED_USER_RSP_OFFSET \
        (8 * X86_MISC_STATE_SYSCALL_SAVED_USER_RSP_INDEX)
#define X86_MISC_STATE_SYSCALL_SAVED_USER_RBP_OFFSET \
        (8 * X86_MISC_STATE_SYSCALL_SAVED_USER_RBP_INDEX)
#define X86_MISC_STATE_SAVED_GS_BASE_UPPER32_OFFSET \
        (8 * X86_MISC_STATE_SAVED_GS_BASE_UPPER32_INDEX)

#define PROCESSOR_LOCALSTATE_FLAG_BSP                (1 << 0)
#define PROCESSOR_LOCALSTATE_FLAG_X86_SHOULD_HALT    (1 << 6)
#define PROCESSOR_LOCALSTATE_FLAG_X86_SMAP_SUPPORTED (1 << 7)

struct Processor_LocalState {
        // This must be the first item!
        uintptr_t x86_misc_state[X86_MISC_STATE_VALUES_COUNT];
        struct Thread *running_thread;
        struct List x86_ipimessages;
        struct SpinLock x86_ipimessages_lock;
        union X86_SegmentDescriptor x86_gdt[7];
        struct X86_TSS x86_tss;
        uint8_t flags;
        uint8_t cpu_num;
        struct Processor_LocalState *x86_self; // Pointer to self
};

struct Processor_Thread {
        void *x86_ist1_stack_base, *x86_ist1_rsp, *x86_syscall_kernel_stack_base, *x86_syscall_kernel_rsp,
             *x86_saved_rsp, *x86_saved_user_rsp, *x86_saved_user_rbp;
};

#define PAGE_SIZE 4096UL
#define LITTLE_ENDIAN