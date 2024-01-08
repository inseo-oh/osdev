// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdnoreturn.h>

#if __x86_64__
#include "x86/arch.h" // IWYU pragma: export
#else
#error "Unknown platform"
#endif

////////////////////////////////////////////////////////////////////////////////
// I/O ports
////////////////////////////////////////////////////////////////////////////////

typedef uintptr_t ioaddr_t;

void ioport_out8(ioaddr_t port, uint8_t val);
void ioport_out16(ioaddr_t port, uint16_t val);
uint8_t ioport_in8(ioaddr_t port);
uint16_t ioport_in16(ioaddr_t port);
uint16_t ioport_rep_ins16(ioaddr_t port, void *buf, size_t len);

////////////////////////////////////////////////////////////////////////////////
// MMU
////////////////////////////////////////////////////////////////////////////////

#define MMU_ADDRSPACE_INVALID ((mmu_addrspace_t)0)

typedef uintptr_t mmu_addrspace_t;
typedef uint8_t mmu_prot_t;

#define MMU_PROT_USER  (1 << 0)
#define MMU_PROT_WRITE (1 << 1)
#define MMU_PROT_EXEC  (1 << 2)

WARN_UNUSED_RESULT bool
mmu_map(mmu_addrspace_t handle,
        uintptr_t physaddr,
        void *virtaddr,
        mmu_prot_t prot);
void mmu_update_options(
        mmu_addrspace_t handle, void *virtaddr, mmu_prot_t prot
);
void mmu_unmap(mmu_addrspace_t handle, void *virtaddr);
bool mmu_lowmem_identity_map(uintptr_t physaddr, mmu_prot_t prot);
void mmu_lowmem_identity_unmap(uintptr_t physaddr);
// Returns MMU_ADDRSPACE_INVALID on OOM.
mmu_addrspace_t mmu_addrspace_create(void);
void mmu_addrspace_delete(mmu_addrspace_t addrspace);
void mmu_activate_user_vm_addrspace(mmu_addrspace_t addrspace);
void mmu_deactivate_user_vm_addrspace(void);
void mmu_activate_addrspace(
        mmu_addrspace_t addrspace, uintptr_t addrspace_base
);
mmu_addrspace_t mmu_active_user_vm_addrspace(void);
uintptr_t mmu_virt_to_phys(void *virtaddr);
bool mmu_is_accessible(void *virtaddr, mmu_prot_t requires);
WARN_UNUSED_RESULT mmu_addrspace_t mmu_init_for_bsp(void *direct_mapped_base);

void mmu_init_for_ap(unsigned ap_index);
WARN_UNUSED_RESULT bool mmu_prepare_aps(unsigned ap_count);

////////////////////////////////////////////////////////////////////////////////
// Processor
////////////////////////////////////////////////////////////////////////////////

struct Process;

struct Processor_LocalState *processor_current();
struct Thread *processor_running_thread(struct Processor_LocalState const *state
);
void processor_set_running_thread(
        struct Processor_LocalState *state, struct Thread *thread
);

bool processor_thread_init(struct Processor_Thread *out, void *stack_top);
void processor_thread_deinit(
        struct Processor_Thread *thread, struct Process *process
);
noreturn void processor_thread_enter_initial_kernel_thread(
        struct Processor_Thread *new_thread, void (*entry_point)()
);
void processor_thread_enter(
        struct Processor_Thread *from_thread,
        struct Processor_Thread *to_thread,
        bool is_user_thread,
        void (*entry_point)()
);
void processor_thread_context_switch(
        struct Processor_Thread *old_thread,
        struct Processor_Thread *new_thread,
        bool is_user_thread
);

void processor_init_for_bsp(void);
void processor_init_for_ap(unsigned ap_index);
WARN_UNUSED_RESULT bool processor_prepare_aps(unsigned ap_count);
void processor_halt_others(void);
// This function should be used in places like spinlock's spin loop. It both
// acts as hint to the processor, and also handle pending inter-processor
// messages.
void processor_wait_during_spinloop();

////////////////////////////////////////////////////////////////////////////////
// User-mode memory access
////////////////////////////////////////////////////////////////////////////////

void uaccess_begin();
void uaccess_end();

////////////////////////////////////////////////////////////////////////////////
// Interrupts
////////////////////////////////////////////////////////////////////////////////

#define ENTER_NO_INTERRUPT_SECTION() \
        bool __prev_interrupt_flag = interrupts_disable();

#define LEAVE_NO_INTERRUPT_SECTION() \
        if (__prev_interrupt_flag) { \
                interrupts_enable(); \
        }

bool interrupts_enable();
bool interrupts_disable();
void interrupts_wait();
bool interrupts_are_enabled();

////////////////////////////////////////////////////////////////////////////////
// Misc
////////////////////////////////////////////////////////////////////////////////

void stacktrace_show(void);
