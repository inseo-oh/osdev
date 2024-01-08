// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "_internal.h"
#include "arch.h"
#include <errno.h>
#include <kernel/api/bits/syscall.h>
#include <kernel/kernel.h>
#include <kernel/syscall.h>
#include <stddef.h>
#include <stdint.h>
#include <kernel/utility/utility.h>

static char const *LOG_TAG = "syscall";

#define SC_KERNEL_STACK_TOP_OFFSET \
        X86_MISC_STATE_SYSCALL_KERNEL_STACK_TOP_OFFSET
#define SC_SAVED_USER_RSP_OFFSET X86_MISC_STATE_SYSCALL_SAVED_USER_RSP_OFFSET
#define SC_SAVED_USER_RBP_OFFSET X86_MISC_STATE_SYSCALL_SAVED_USER_RBP_OFFSET

static NAKED void entry() {
        // RCX:     Saved userland RIP (Must be preserved)
        // R11:     Saved userland RFLAGS (Must be preserved)
        // GS.Base: Userland GS.Base
        // CS, SS:  Kernel CS and SS
        // Interrupts are disabled (IF flag was masked using MSR_SFMASK)
        //
        // Parmaeter / Return value registers:
        // RAX: [Input] System call number, [Output] Return value
        // RDI: [Input] Argument 0
        // RSI: [Input] Argument 1
        // RDX: [Input] Argument 2
        // RBX: [Input] Argument 3
        // These register choices are designed to mimick the System V x86-64 ABI
        // to avoid copying values as much as possible. However, SysV ABI uses
        // RCX as Argument 3, and since that contains old RIP value, we use RBX
        // instead.

        // clang-format off
        __asm__ volatile(
                // Return -ENOSYS if system call number is outside range
                "cmp rax, " STRINGIFY(SYSCALL_COUNT) "\n"
                "jl 1f\n"
                "mov rax, -" STRINGIFY(ENOSYS) "\n"
                "sysretq\n"
                // It is in valid system call number range
                "1:\n"
                "swapgs\n"
                "mov gs:" STRINGIFY(SC_SAVED_USER_RSP_OFFSET) ", rsp\n"
                "mov gs:" STRINGIFY(SC_SAVED_USER_RBP_OFFSET) ", rbp\n"
                "mov rsp, gs:" STRINGIFY(SC_KERNEL_STACK_TOP_OFFSET) "\n"
                "mov rbp, rsp\n"
                // Save general purpose registers. SysV ABI doesn't preserve
                // following registers: RAX, RDI, RSI, RDX, RCX, and R8~R11
                // But since RAX, RDI, RSI, RDX, RCX are used by either SYSCALL
                // or arguments, we only need to save R8~R11 and RCX.
                "push rcx\n"
                "push r11\n"
                "push r10\n"
                "push r9\n"
                "push r8\n"
                // Now we call the handler function
                "mov rcx, rbx\n" // Set argument 3
                // -> Store effective address of syscall table to RBX
                "lea rbx, [rip + syscall_x86_table]\n" 
                // -> And use RBX and system call number to calculate final
                //    address, and read function address from there.
                "mov rax, qword ptr [rbx + (8 * rax)]\n"
                // -> Call the handler
                "call rax\n"
                // Now RAX contains the return value.
                // Restore registers we saved above
                "pop r8\n"
                "pop r9\n"
                "pop r10\n"
                "pop r11\n"
                "pop rcx\n"
                // Return to userland code
                "mov rsp, gs:" STRINGIFY(SC_SAVED_USER_RSP_OFFSET) "\n"
                "mov rbp, gs:" STRINGIFY(SC_SAVED_USER_RBP_OFFSET) "\n"
                "swapgs\n"
                "sysretq\n"
        );
        // clang-format on
}

static int64_t sc_no_impl() {
        LOGE(LOG_TAG, "Attempted to call non-implemented syscall");
        return -ENOSYS;
}

uintptr_t syscall_x86_table[SYSCALL_COUNT];
#define ST syscall_x86_table

_Static_assert(
        sizeof(ST) / sizeof(*ST) == SYSCALL_COUNT, "Syscall count mismatch"
);

void syscall_init_tables(void) {
        for (size_t i = 0; i < SYSCALL_COUNT; ++i) {
                ST[i] = (uintptr_t)sc_no_impl;
        }
        ST[SYSCALL_INDEX_WRITE] = (uintptr_t)syscall_impl_write;
        ST[SYSCALL_INDEX_READ] = (uintptr_t)syscall_impl_read;
        ST[SYSCALL_INDEX_SCHED_YIELD] = (uintptr_t)syscall_impl_sched_yield;
}

void syscall_init_msrs(void) {
        // Kernel CS = STAR[47:32], Kernel SS = Kernel CS + 8
        uint64_t star = (uint64_t)GDT_KERNEL_CS << 32;
        // Userland SS = STAR[63:48] + 8, Userland CS = Userland SS + 8
        // (Why SS is loaded from + 8 offset? I have no idea)
        star |= (((uint64_t)GDT_USER_DS_OFFSET - 8) | X86_RPL_USER) << 48;
        wrmsr(MSR_STAR, star);
        wrmsr(MSR_LSTAR, (uint64_t)entry);
        wrmsr(MSR_SFMASK, RFLAGS_IF);
}
