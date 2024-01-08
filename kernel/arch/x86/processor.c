// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "_internal.h"
#include "arch.h"
#include <cpuid.h>
#include <kernel/arch/arch.h>
#include <kernel/heap/heap.h>
#include <kernel/kernel.h>
#include <kernel/lock/spinlock.h>
#include <kernel/memory/memory.h>
#include <kernel/tasks/tasks.h>
#include <kernel/utility/utility.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static char const *LOG_TAG = "x86";

// `access_byte` flags for `X86_BaseSegmentDescriptor`

// Set by CPU when the segment was accessed
// NOTE: This only applies to non-system segment descriptor.
#define GDT_NON_SYSTEM_ACCESS_BYTE_FLAG_ACCESSED (1 << 0)
// Readable/Writable bit
// - Data segments: Writable bit
// - Code segments: Readable bit
// NOTE: This only applies to non-system segment descriptor.
#define GDT_NON_SYSTEM_ACCESS_BYTE_FLAG_RW (1 << 1)
// Direction/Conforming bit
// - Data segments: Direction bit(Segment goes up(i.e. Base < Limit) or
//   down(i.e. Limit < Base))
// - Code segments: Conforming bit(Can code be kerneluted from lower privilege
//   level? - If set to 0, code can only be kerneluted on that specific DPL)
// NOTE: This only applies to non-system segment descriptor.
#define GDT_NON_SYSTEM_ACCESS_BYTE_FLAG_DC (1 << 2)
// KERNELutable(Code segment)?
// NOTE: This only applies to non-system segment descriptor.
#define GDT_NON_SYSTEM_ACCESS_BYTE_FLAG_E (1 << 3)

// Non-system segment?
#define GDT_ACCESS_BYTE_FLAG_S (1 << 4)
// DPL0
#define GDT_ACCESS_BYTE_FLAG_DPL0 (0 << 5)
// DPL1
#define GDT_ACCESS_BYTE_FLAG_DPL1 (1 << 5)
// DPL2
#define GDT_ACCESS_BYTE_FLAG_DPL2 (2 << 5)
// DPL3
#define GDT_ACCESS_BYTE_FLAG_DPL3 (3 << 5)
// Present?
#define GDT_ACCESS_BYTE_FLAG_P (1 << 7)
// LDT
// NOTE: This only applies to system segment descriptor.
#define GDT_ACCESS_BYTE_FLAG_TYPE_LDT (0x2 << 0)
// 64-bit TSS(Available)
// NOTE: This only applies to system segment descriptor.
#define GDT_ACCESS_BYTE_FLAG_TYPE_TSS64_AVL (0x9 << 0)
// 64-bit TSS(Busy)
// NOTE: This only applies to system segment descriptor .
#define GDT_ACCESS_BYTE_FLAG_TYPE_TSS64_BUSY = (0xB << 0)

#define GDT_ACCESS_BYTE_FLAG_CODE_DATA_COMMON              \
        (GDT_ACCESS_BYTE_FLAG_P | GDT_ACCESS_BYTE_FLAG_S | \
         GDT_NON_SYSTEM_ACCESS_BYTE_FLAG_RW)
#define GDT_ACCESS_BYTE_FLAG_FOR_KERNEL GDT_ACCESS_BYTE_FLAG_DPL0
#define GDT_ACCESS_BYTE_FLAG_FOR_USER   GDT_ACCESS_BYTE_FLAG_DPL3
#define GDT_ACCESS_BYTE_FLAG_FOR_CODE   GDT_NON_SYSTEM_ACCESS_BYTE_FLAG_E
#define GDT_ACCESS_BYTE_FLAG_FOR_DATA   0
#define GDT_ACCESS_BYTE_FOR_KERNEL_CODE          \
        (GDT_ACCESS_BYTE_FLAG_CODE_DATA_COMMON | \
         GDT_ACCESS_BYTE_FLAG_FOR_KERNEL | GDT_ACCESS_BYTE_FLAG_FOR_CODE)
#define GDT_ACCESS_BYTE_FOR_KERNEL_DATA          \
        (GDT_ACCESS_BYTE_FLAG_CODE_DATA_COMMON | \
         GDT_ACCESS_BYTE_FLAG_FOR_KERNEL | GDT_ACCESS_BYTE_FLAG_FOR_DATA)
#define GDT_ACCESS_BYTE_FOR_USER_CODE            \
        (GDT_ACCESS_BYTE_FLAG_CODE_DATA_COMMON | \
         GDT_ACCESS_BYTE_FLAG_FOR_USER | GDT_ACCESS_BYTE_FLAG_FOR_CODE)
#define GDT_ACCESS_BYTE_FOR_USER_DATA            \
        (GDT_ACCESS_BYTE_FLAG_CODE_DATA_COMMON | \
         GDT_ACCESS_BYTE_FLAG_FOR_USER | GDT_ACCESS_BYTE_FLAG_FOR_DATA)

#define GDT_ACCESS_BYTE_FOR_TSS64 \
        (GDT_ACCESS_BYTE_FLAG_P | GDT_ACCESS_BYTE_FLAG_TYPE_TSS64_AVL)

// `limit_b19_to_b16_and_flags` flags for `X86_BaseSegmentDescriptor`

// Is `limit` 1Byte blocks(0) or 4KiB blocks(1)?
#define GDT_FLAG_G (1 << 5)
// 16-bit-segment(0) or 32-bit segment(1)?
// Note that in 64-bit mode, this should be always `0`.
#define GDT_FLAG_DB (1 << 6)
// Long-mode(64-bit mode)?
// NOTE: When this is set, `GDT_FLAG_DB` should NOT be set!
#define GDT_FLAG_L (1 << 7)

static struct X86_BaseSegmentDescriptor make_normal_segment_descriptor(
        uint32_t base,
        uint32_t limit,
        bool is_limit_4k_mul,
        bool is_long_mode,
        uint8_t access_byte
) {
        struct X86_BaseSegmentDescriptor out;
        uint8_t flags = is_long_mode ? GDT_FLAG_L : GDT_FLAG_DB;
        if (is_limit_4k_mul) {
                flags |= GDT_FLAG_G;
        }
        ASSERT((flags & 0x0F) == 0);
        out.limit_b15_to_b0 = limit & 0xFFFF;
        out.base_b15_to_b0 = (base & 0xFFFF);
        out.base_b23_to_b16 = ((base >> 16) & 0xFFFF);
        out.access_byte = access_byte;
        out.limit_b19_to_b16_and_flags = flags | ((limit >> 16) & 0xF);
        out.base_b31_to_b24 = ((base >> 24) & 0xFF);
        return out;
}

static struct X86_BaseSegmentDescriptor
make_segment_descriptor_with_full_addr_range(
        bool is_long_mode, uint8_t access_byte
) {
        return make_normal_segment_descriptor(
                0, 0xFFFFF, true, is_long_mode, access_byte
        );
}

static struct X86_BaseSegmentDescriptor
make_kernel_code_segment_descriptor(bool is_long_mode) {
        return make_segment_descriptor_with_full_addr_range(
                is_long_mode, GDT_ACCESS_BYTE_FOR_KERNEL_CODE
        );
}

static struct X86_BaseSegmentDescriptor
make_kernel_data_segment_descriptor(bool is_long_mode) {
        return make_segment_descriptor_with_full_addr_range(
                is_long_mode, GDT_ACCESS_BYTE_FOR_KERNEL_DATA
        );
}

static struct X86_BaseSegmentDescriptor
make_user_code_segment_descriptor(bool is_long_mode) {
        return make_segment_descriptor_with_full_addr_range(
                is_long_mode, GDT_ACCESS_BYTE_FOR_USER_CODE
        );
}

static struct X86_BaseSegmentDescriptor
make_user_data_segment_descriptor(bool is_long_mode) {
        return make_segment_descriptor_with_full_addr_range(
                is_long_mode, GDT_ACCESS_BYTE_FOR_USER_DATA
        );
}

// NOTE: Two GDT entries will be written: `out[0]` and `out[1]`.
//       Also, `limit` is in bytes.
static void init_system_segment_descriptor(
        union X86_SegmentDescriptor *out,
        uint64_t base,
        uint32_t limit,
        uint8_t access_byte
) {
        out[0].base = make_normal_segment_descriptor(
                base, limit, false, true, access_byte
        );
        out[1].system_descriptor_ext.base_b63_to_b32 = base >> 32;
        out[1].system_descriptor_ext._reserved = 0;
}

#define STACK_SIZE       (32UL * 1024UL)
#define STACK_PAGE_COUNT (STACK_SIZE / PAGE_SIZE)

// TODO: This style of allocation can be seen in other places as well, so move
//       this to somewhere else!
WARN_UNUSED_RESULT static void *alloc_stack(struct Process *process) {
        struct PhysPage_Addr page = physpage_alloc(STACK_PAGE_COUNT);
        if (!page.value) {
                TODO_HANDLE_ERROR();
        }
        void *addr = process_map_pages(
                process,
                page.value,
                STACK_PAGE_COUNT,
                (struct Proc_MapOptions){.executable = false, .writable = true}
        );
        if (!addr) {
                TODO_HANDLE_ERROR();
        }
        return addr;
}

// TODO: This style of allocation can be seen in other places as well, so move
//       this to somewhere else!
static void free_stack(struct Process *process, void *stack_base) {
        uintptr_t physpage = mmu_virt_to_phys(stack_base);
        process_unmap_pages(process, stack_base, STACK_PAGE_COUNT);
        physpage_free((struct PhysPage_Addr){physpage}, STACK_PAGE_COUNT);
}

struct GDTR {
        uint16_t size;
        uint64_t offset;
} PACKED;

static struct Processor_LocalState s_bsp_localstate;
static struct Processor_LocalState *s_ap_localstates;
static size_t s_ap_count;

static void use_thread_ist_and_syscall_stack(
        struct Processor_LocalState *state,
        struct Processor_Thread const *thread
) {
        ASSERT(!interrupts_are_enabled());
        state->x86_tss.ist[0].low = (uintptr_t)thread->x86_ist1_rsp;
        state->x86_tss.ist[0].high = (uintptr_t)thread->x86_ist1_rsp >> 32;
        state->x86_misc_state[X86_MISC_STATE_SYSCALL_KERNEL_STACK_TOP_INDEX] =
                (uintptr_t)thread->x86_syscall_kernel_rsp;
}

// In System V ABI, registers RBX, RBP, R12, R13, R14, R15 are preserved. Others
// are scratch registers, so it is safe to destroy those. In other words, only
// those 6 registers(as well as RSP and RIP) need to be saved, and callers of
// `processor_thread_enter` or `processor_thread_switch_to` knows that others
// may get destroyed.

#define ASM_SAVE_STATE            \
        "pushfq\n"                \
        "push rbx\n"              \
        "push rbp\n"              \
        "push r12\n"              \
        "push r13\n"              \
        "push r14\n"              \
        "push r15\n"              \
        "lea rbx, [rip + 99f] \n" \
        "push rbx\n"              \
        "mov %[save_rsp_to], rsp\n"

// This one is executed after saving previous state(if needed) and before
// loading new state. New threads need to manually unlock the Sched before new
// thread executes(because there's no code to unlock when we jump to
// entry_point).
//
// We have to be careful about *when* we unlock the Sched: We must unlock the
// Sched after the point where we determine that it is safe to allow other
// processors can safely touch the Sched. When starting a thread, kernel does
// the following:
//  1. Pick next thread to run
//  2. Put old thread into Sched's queue(Doesn't happen for initial kernel
//  thread launch)
//  3. Replace Cpu_LocalState's running thread to new one
//  4. Pushes old thread's context to stack(Doesn't happen for initial kernel
//  thread launch)
//  5. Load new RIP, RSP, RFLAGS, and GS.base(if returning to userland)
//  6. Do JMP(Kernel) or SYSRET(Userland) to start executing new code
//
// If you look closely, saving context(Step 4) occurs **after** we put the
// thread into queue(Step 2). And if we unlock the Sched before that point, this
// could happen if we imagine that we have SMP support:
//
// | CPU  | Step                                         |
// |------|----------------------------------------------|
// | cpu0 | <Locks scheduler>                                |
// | cpu0 | 1. Pick next thread to run                   |
// | cpu0 | 2. Put old thread into Sched's queue         |
// | cpu0 | <Unlocks scheduler>                              |
// | cpu1 | <Locks scheduler>                                |
// | cpu1 | 1. Pick next thread to run                   |
// | cpu1 | -> Picks the thread we just pushed           |
// | cpu1 | Perform step 2..3                            |
// | cpu1 | 4. Attempts to load old thread's context     |
// | cpu1 | -> ... Before context was saved              |
// | cpu1 | 5. cpu1 crashes                              |
//
// So we have to unlock the Sched after step 4. So that's what we are doing.

#define ASM_ABOUT_TO_ENTER_NEW_THREAD                \
        "push rax\n"                                 \
        "push rcx\n"                                 \
        "call scheduler_about_to_enter_new_thread\n" \
        "pop rcx\n"                                  \
        "pop rax\n"

// NOTE: This assumes new_rsp_value is RAX register, and reuses the RAX after
//       loading RSP.
#define ASM_LOAD_STATE_COMMON         \
        "mov rsp, %[new_rsp_value]\n" \
        "mov rax, %[ds]\n"            \
        "mov ds, rax\n"               \
        "mov es, rax\n"

#define COMMON_ASM_TAIL "99:\n"

void processor_thread_enter_initial_kernel_thread(
        struct Processor_Thread *new_thread, void (*entry_point)()
) {
        ASSERT(!interrupts_are_enabled());
        struct Processor_LocalState *state = processor_current();
        use_thread_ist_and_syscall_stack(state, new_thread);
        // We are setting RBP to 0 to indicate the end of stack trace.
        // clang-format off
        __asm__ volatile(
                ASM_ABOUT_TO_ENTER_NEW_THREAD
                ASM_LOAD_STATE_COMMON
                "mov rbp, 0\n"
                "push %[rflags]\n"
                "popfq\n"
                "jmp %[entry_point]\n"
                COMMON_ASM_TAIL
                :
                : [new_rsp_value] "a"(new_thread->x86_saved_rsp),
                [ds] "i"(GDT_KERNEL_DS),
                [rflags] "i"(0),
                [entry_point] "c"(entry_point)
        );
        // clang-format on
        panic("unreachable");
}

void processor_thread_enter(
        struct Processor_Thread *from_thread,
        struct Processor_Thread *to_thread,
        bool is_user_thread,
        void (*entry_point)()
) {
        ASSERT(!interrupts_are_enabled());
        ASSERT(to_thread != from_thread);
        struct Processor_LocalState *state = processor_current();
        use_thread_ist_and_syscall_stack(state, to_thread);
        // We are setting RBP to 0 to indicate the end of stack trace.

        if (is_user_thread) {
                // clang-format off
                __asm__ volatile(
                        ASM_SAVE_STATE
                        ASM_ABOUT_TO_ENTER_NEW_THREAD
                        ASM_LOAD_STATE_COMMON
                        "mov rbp, 0\n"
                        "mov r11, %[rflags]\n"
                        "swapgs\n"
                        "sysretq\n"
                        COMMON_ASM_TAIL
                        : [save_rsp_to] "=m"(from_thread->x86_saved_rsp)
                        : [new_rsp_value] "a"(to_thread->x86_saved_rsp),
                        [rflags] "i"(RFLAGS_IF),
                        [ds] "i"(GDT_USER_DS),
                        "c"(entry_point)
                );
                // clang-format on
        } else {
                // clang-format off
                __asm__ volatile(
                        ASM_SAVE_STATE
                        ASM_ABOUT_TO_ENTER_NEW_THREAD
                        ASM_LOAD_STATE_COMMON
                        "mov rbp, 0\n"
                        "push %[rflags]\n"
                        "popfq\n"
                        "jmp %[entry_point]\n"
                        COMMON_ASM_TAIL
                        : [save_rsp_to] "=m"(from_thread->x86_saved_rsp)
                        : [new_rsp_value] "a"(to_thread->x86_saved_rsp),
                        [ds] "i"(GDT_KERNEL_DS),
                        [entry_point]"c"(entry_point),
                        [rflags] "i"(0)
                );
                // clang-format on
        }
}

void processor_thread_context_switch(
        struct Processor_Thread *old_thread,
        struct Processor_Thread *new_thread,
        bool is_user_thread
) {
        ASSERT(!interrupts_are_enabled());
        ASSERT(new_thread != old_thread);
        struct Processor_LocalState *processor = processor_current();
        processor->x86_misc_state[X86_MISC_STATE_SAVED_GS_BASE_UPPER32_INDEX] =
                is_user_thread ? 0 : (uintptr_t)processor;
        use_thread_ist_and_syscall_stack(processor, new_thread);

        // Remember user stack pointers saved by SYSCALL
        // This not only allows syscalls to switch to different task safely, but
        // this is also *very* important on multi-processor systems, where
        // context switch may get saved on one processor and then later restored
        // on the another one.
        old_thread->x86_saved_user_rsp =
                (void *)processor->x86_misc_state
                        [X86_MISC_STATE_SYSCALL_SAVED_USER_RSP_INDEX];
        old_thread->x86_saved_user_rbp =
                (void *)processor->x86_misc_state
                        [X86_MISC_STATE_SYSCALL_SAVED_USER_RBP_INDEX];
        // Load old user stack pointers that may be used by SYSRET
        processor->x86_misc_state[X86_MISC_STATE_SYSCALL_SAVED_USER_RSP_INDEX] =
                (uintptr_t)new_thread->x86_saved_user_rsp;
        processor->x86_misc_state[X86_MISC_STATE_SYSCALL_SAVED_USER_RBP_INDEX] =
                (uintptr_t)new_thread->x86_saved_user_rbp;

        // NOTE: Even if next thread is userland thread, it was last switched
        //       from the ISR, which is kernel-mode code. So we are always
        //       returning to kernel-mode code.
        // clang-format off
        __asm__ volatile(
                ASM_SAVE_STATE
                ASM_LOAD_STATE_COMMON
                "pop rcx\n"
                "pop r15\n"
                "pop r14\n"
                "pop r13\n"
                "pop r12\n"
                "pop rbp\n"
                "pop rbx\n"
                "popfq\n"
                "jmp rcx\n"
                COMMON_ASM_TAIL
                : [save_rsp_to] "=m"(old_thread->x86_saved_rsp)
                : [new_rsp_value] "a"(new_thread->x86_saved_rsp),
                [ds] "i"(GDT_KERNEL_DS)
                : "rcx"
        );
        // clang-format on
}

#undef COMMON_ASM_TAIL
#undef ASM_SAVE_STATE

struct Processor_LocalState *processor_current() {
        ASSERT(!interrupts_are_enabled());
        struct Processor_LocalState *state;
        __asm__("mov %0, gs:%1"
                : "=r"(state)
                : "i"(offsetof(struct Processor_LocalState, x86_self)));
        ASSERT(state);
        return state;
}

struct Thread *processor_running_thread(struct Processor_LocalState const *state
) {
        ASSERT(!interrupts_are_enabled());
        return state->running_thread;
}

void processor_set_running_thread(
        struct Processor_LocalState *state, struct Thread *thread
) {
        ASSERT(!interrupts_are_enabled());
        state->running_thread = thread;
}

bool processor_thread_init(struct Processor_Thread *out, void *stack_top) {
        struct Process *kernel_process = process_kernel();
        out->x86_ist1_stack_base = alloc_stack(kernel_process);
        if (!out->x86_ist1_stack_base) {
                TODO_HANDLE_ERROR();
        }
        out->x86_syscall_kernel_stack_base = alloc_stack(kernel_process);
        if (!out->x86_syscall_kernel_stack_base) {
                TODO_HANDLE_ERROR();
        }
        out->x86_ist1_rsp =
                (void *)((uintptr_t)out->x86_ist1_stack_base + STACK_SIZE);
        out->x86_syscall_kernel_rsp =
                (void *)((uintptr_t)out->x86_syscall_kernel_stack_base +
                         STACK_SIZE);
        out->x86_saved_rsp = stack_top;
        return true;
}

void processor_thread_deinit(
        struct Processor_Thread *thread, struct Process *process
) {
        free_stack(process, thread->x86_syscall_kernel_stack_base);
        free_stack(process, thread->x86_ist1_stack_base);
}

#define CR4_FLAG_SMEP (1 << 20)
#define CR4_FLAG_SMAP (1 << 21)

// SMEP and SMAP flag doesn't seem to be widely available in cpuid.h.
// So we define our own.
#define CPUID_EBX_SMEP (1 << 7)
#define CPUID_EBX_SMAP (1 << 20)

static void enable_smep_smap(void) {
        // Check CPUID to see if SMEP and SMAP is available.
        uint32_t eax;
        uint32_t ebx;
        uint32_t ecx;
        uint32_t edx;
        if (!__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx)) {
                LOGE(LOG_TAG,
                     "Couldn't query CPUID(Leaf 7, Subleaf 0). Not enabling "
                     "SMEP & SMAP.");
                return;
        }
        uint64_t cr4_bit_set_mask = 0;
        if (ebx & CPUID_EBX_SMEP) {
                cr4_bit_set_mask |= CR4_FLAG_SMEP;
        } else {
                LOGE(LOG_TAG,
                     "WARNING: SMEP is not supported. Kernel will be "
                     "able to execute userland code!");
        }
        if (ebx & CPUID_EBX_SMAP) {
                cr4_bit_set_mask |= CR4_FLAG_SMAP;
                processor_current()->flags |=
                        PROCESSOR_LOCALSTATE_FLAG_X86_SMAP_SUPPORTED;
        } else {
                LOGE(LOG_TAG,
                     "WARNING: SMAP is not supported. Kernel will be "
                     "able to access userland memory without explicit "
                     "control!");
        }
        __asm__ volatile(
                "mov rax, cr4\n"
                "or rax, %[cr4_bit_set_mask]\n"
                "mov cr4, rax" ::[cr4_bit_set_mask] "r"(cr4_bit_set_mask)
                : "eax"
        );
}

static void init_gdt(struct Processor_LocalState *state) {
        state->x86_gdt[GDT_KERNEL_CS_INDEX].base =
                make_kernel_code_segment_descriptor(true);
        state->x86_gdt[GDT_KERNEL_DS_INDEX].base =
                make_kernel_data_segment_descriptor(true);
        state->x86_gdt[GDT_USER_CS_INDEX].base =
                make_user_code_segment_descriptor(true);
        state->x86_gdt[GDT_USER_DS_INDEX].base =
                make_user_data_segment_descriptor(true);
        // Below initializes gdt[5] and gdt[6]
        init_system_segment_descriptor(
                &state->x86_gdt[X86_TSS_INDEX],
                (uintptr_t)&state->x86_tss,
                sizeof(state->x86_tss),
                GDT_ACCESS_BYTE_FOR_TSS64
        );
}

static void init_tss(struct Processor_LocalState *state) {
        kmemset(&state->x86_tss, 0, sizeof(state->x86_tss));
        state->x86_tss.iopb = sizeof(state->x86_tss);
}

static void load_gdt(struct Processor_LocalState *state) {
        // Load GDT
        volatile struct GDTR gdtr = {
                sizeof(state->x86_gdt), (uint64_t)state->x86_gdt
        };
        __asm__ volatile("lgdt [%0]" ::"r"(&gdtr));
}

static void load_selectors(void) {
        uint64_t seg_cs = GDT_KERNEL_CS_OFFSET;
        uint64_t seg_ds = GDT_KERNEL_DS_OFFSET;
        uint16_t seg_ts = X86_TSS_OFFSET;

        // Reload segment selectors
        __asm__ volatile("  lea rax, [rip + 1f]\n"
                         "  push %0\n"
                         "  push rax\n"
                         "  retfq\n"
                         "1:\n"
                         "  mov ds, %1\n"
                         "  mov es, %1\n"
                         "  mov fs, %1\n"
                         "  mov gs, %1\n"
                         "  mov ss, %1\n"
                         "  ltr %2\n" ::"r"(seg_cs),
                         "r"(seg_ds),
                         "r"(seg_ts)
                         : "rax" // Used as temporary storage for LEA result
        );
}

static void load_gs_base(struct Processor_LocalState *state) {
        state->x86_self = state;
        state->running_thread = NULL;
        wrmsr(MSR_GS_BASE, (uint64_t)state);
}

#define MAX_MESSAGES_PER_PROCESSOR 100

typedef enum {
        // This will be the default value when s_ipimessage_pool is zeroed
        // during initialization.
        IPIMESSAGE_FREE = 0,
        // Values when message was taken from the pool.
        IPIMESSAGE_UNINITIALIZED,
        IPIMESSAGE_FULL_TLB_FLUSH,
        IPIMESSAGE_PAGE_TLB_FLUSH,
} ipimessage_tag_t;

struct IPIMessage {
        struct List_Node node_head;
        ipimessage_tag_t tag;
        atomic_ulong remaining_response_count;

        union {
                struct {
                        void *vaddr;
                } page_tlb_flush;
        } data;
};

static struct List s_ipimessage_pool;
static struct SpinLock s_ipimessage_pool_lock;

static struct IPIMessage *get_ipimessage_from_pool(void) {
        bool prev_interrupt_state;
        spinlock_lock(&s_ipimessage_pool_lock, &prev_interrupt_state);
        struct IPIMessage *msg = NULL;
        while (1) {
                msg = s_ipimessage_pool.tail;
                if (msg) {
                        break;
                }
                spinlock_unlock(&s_ipimessage_pool_lock, prev_interrupt_state);
                processor_wait_during_spinloop();
                spinlock_lock(&s_ipimessage_pool_lock, &prev_interrupt_state);
        }
        ASSERT(msg);
        list_remove_tail(&s_ipimessage_pool);
        spinlock_unlock(&s_ipimessage_pool_lock, prev_interrupt_state);
        return msg;
}

static void return_ipimessage_to_pool(struct IPIMessage *msg) {
        bool prev_interrupt_state;
        spinlock_lock(&s_ipimessage_pool_lock, &prev_interrupt_state);
        list_insert_head(&s_ipimessage_pool, &msg->node_head);
        spinlock_unlock(&s_ipimessage_pool_lock, prev_interrupt_state);
}

static void enable_wp(void) {
        __asm__ volatile("mov rax, cr0\n"
                         "or rax, 0x10000\n"
                         "mov cr0, rax" ::
                                 : "eax");
}

static void enable_features(void) {
        enable_smep_smap();
        enable_wp();
        x86_msr_set_flag(MSR_IA32_EFER, MSR_IA32_EFER_NXE | MSR_IA32_EFER_SCE);
}

static void init_common(struct Processor_LocalState *state) {
        init_tss(state);
        load_gdt(state);
        load_selectors();
        // Reloading selectors seem to wipe GS.Base, so we load GS.Base after
        // after that.
        load_gs_base(state);
        enable_features();
}

void processor_init_for_bsp(void) {
        struct Processor_LocalState *state = &s_bsp_localstate;
        kmemset(state, 0, sizeof(*state));
        s_bsp_localstate.flags = PROCESSOR_LOCALSTATE_FLAG_BSP;
        init_gdt(state);
        init_common(state);
}

void processor_init_for_ap(unsigned ap_index) {
        struct Processor_LocalState *state = &s_ap_localstates[ap_index];
        init_common(state);
}

bool processor_prepare_aps(unsigned ap_count) {
        // TODO(OIS): Make cmalloc() and replace this with that.
        s_ap_localstates = kmalloc(sizeof(*s_ap_localstates) * ap_count);
        if (!s_ap_localstates) {
                goto oom;
        }
        kmemset(s_ap_localstates, 0, sizeof(*s_ap_localstates) * ap_count);
        size_t message_count =
                ((size_t)ap_count + 1) * MAX_MESSAGES_PER_PROCESSOR;
        while (message_count) {
                struct IPIMessage *msg = kmalloc(sizeof(*msg));
                if (!msg) {
                        goto oom;
                }
                list_insert_tail(&s_ipimessage_pool, &msg->node_head);
                --message_count;
        }
        for (unsigned i = 0; i < ap_count; ++i) {
                struct Processor_LocalState *state = &s_ap_localstates[i];
                init_gdt(state);
        }
        s_ap_count = ap_count;
        return true;
oom:
        TODO_HANDLE_ERROR();
}

static bool am_i_processor(struct Processor_LocalState *state) {
        // TODO(OIS): Remove no-interrupt requirement
        ENTER_NO_INTERRUPT_SECTION();
        struct Processor_LocalState *current = processor_current();
        LEAVE_NO_INTERRUPT_SECTION();
        return current == state;
}

static void dump_message_state() {
        struct Processor_LocalState *state = &s_bsp_localstate;
        if (state->x86_ipimessages.head) {
                LOGE(LOG_TAG, "Remaining messages present on BSP");
        }
        for (size_t i = 0; i < s_ap_count; ++i) {
                struct Processor_LocalState *state = &s_ap_localstates[i];
                if (state->x86_ipimessages.head) {
                        LOGE(LOG_TAG, "Remaining messages present on AP");
                }
        }
}

void processor_process_ipimessages(void) {
        {
                ENTER_NO_INTERRUPT_SECTION();
                struct Processor_LocalState *state = processor_current();
                if (state->flags & PROCESSOR_LOCALSTATE_FLAG_X86_SHOULD_HALT) {
                        LOGI(LOG_TAG, "Halt");
                        while (1) {}
                }
                bool prev_interrupt_state;
                spinlock_lock(
                        &state->x86_ipimessages_lock, &prev_interrupt_state
                );
                while (1) {
                        struct IPIMessage *msg = state->x86_ipimessages.tail;
                        if (!msg) {
                                break;
                        }
                        list_remove_tail(&state->x86_ipimessages);
                        switch (msg->tag) {
                        case IPIMESSAGE_FULL_TLB_FLUSH:
                                mmu_invalidate_local_tlb();
                                break;
                        case IPIMESSAGE_PAGE_TLB_FLUSH:
                                mmu_invalidate_local_tlb_for(
                                        msg->data.page_tlb_flush.vaddr
                                );
                                break;
                        case IPIMESSAGE_FREE:
                        case IPIMESSAGE_UNINITIALIZED:
                                UNREACHABLE();
                        }
                        --msg->remaining_response_count;
                        dump_message_state();
                }
                spinlock_unlock(
                        &state->x86_ipimessages_lock, prev_interrupt_state
                );
                LEAVE_NO_INTERRUPT_SECTION();
        }
}

void processor_broadcast_ipi_to_others(void) {
        lapic_send_ipi(
                0,
                LAPIC_IPI_FLAG_VECTOR(LAPIC_BROADCAST_IPI_VECTOR) |
                        LAPIC_IPI_FLAG_DELIVERY_FIXED |
                        LAPIC_IPI_FLAG_DEST_PHYSICAL |
                        LAPIC_IPI_FLAG_LEVEL_ASSERT |
                        LAPIC_IPI_FLAG_TRIGGER_EDGE |
                        LAPIC_IPI_FLAG_DEST_SHORTHAND_ALL_BUT_SELF
        );
}

static void
queue_message(struct Processor_LocalState *state, struct IPIMessage *msg) {
        bool prev_interrupt_state;
        spinlock_lock(&state->x86_ipimessages_lock, &prev_interrupt_state);
        list_insert_tail(&state->x86_ipimessages, &msg->node_head);
        spinlock_unlock(&state->x86_ipimessages_lock, prev_interrupt_state);
}

static void send_message_and_wait(struct IPIMessage *msg) {
        // AP count + BSP count(1) - 1 = AP count
        msg->remaining_response_count = s_ap_count;
        size_t sent_count = 0;
        if (!am_i_processor(&s_bsp_localstate)) {
                struct Processor_LocalState *state = &s_bsp_localstate;
                queue_message(state, msg);
                sent_count += 1;
        }
        for (size_t i = 0; i < s_ap_count; ++i) {
                struct Processor_LocalState *state = &s_ap_localstates[i];
                if (am_i_processor(state)) {
                        continue;
                }
                queue_message(state, msg);
                sent_count += 1;
        }
        ASSERT(sent_count == s_ap_count);
        processor_broadcast_ipi_to_others();
        // Wait for response
        while (msg->remaining_response_count) {
                processor_wait_during_spinloop();
        }
        return_ipimessage_to_pool(msg);
}

void processor_halt_others(void) {
        if (am_i_processor(&s_bsp_localstate)) {
                s_bsp_localstate.flags |=
                        PROCESSOR_LOCALSTATE_FLAG_X86_SHOULD_HALT;
        }
        for (size_t i = 0; i < s_ap_count; ++i) {
                if (am_i_processor(&s_ap_localstates[i])) {
                        s_ap_localstates[i].flags |=
                                PROCESSOR_LOCALSTATE_FLAG_X86_SHOULD_HALT;
                }
        }
        processor_broadcast_ipi_to_others();
}

void processor_flush_other_processors_tlb(void) {
        struct IPIMessage *msg = get_ipimessage_from_pool();
        msg->tag = IPIMESSAGE_FULL_TLB_FLUSH;
        send_message_and_wait(msg);
}

void processor_flush_other_processors_tlb_for(void *vaddr) {
        struct IPIMessage *msg = get_ipimessage_from_pool();
        msg->tag = IPIMESSAGE_PAGE_TLB_FLUSH;
        msg->data.page_tlb_flush.vaddr = vaddr;
        send_message_and_wait(msg);
}

void processor_wait_during_spinloop(void) {
        __asm__ volatile("pause" ::: "memory");
        processor_process_ipimessages();
}
