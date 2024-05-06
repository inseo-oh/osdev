// SPDX-FileCopyrightText: (c) 2024 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause

#include "Isr.hh"

extern "C" {
#define NORETURN_WORKAROUND
#include "_internal.h"
#include "arch.h"
#include "kernel/arch/arch.h"
#include "kernel/interrupt/interrupts.h"
#include "kernel/kernel.h"
#include <stdbool.h>
#include <stdint.h>
#include "kernel/utility/utility.h"
}

namespace Kernel::Isr {

struct TrapFrame {
        uint64_t r15;
        uint64_t r14;
        uint64_t r13;
        uint64_t r12;
        uint64_t r11;
        uint64_t r10;
        uint64_t r9;
        uint64_t r8;
        uint64_t rdi;
        uint64_t rsi;
        uint64_t rdx;
        uint64_t rcx;
        uint64_t rbx;
        uint64_t gs;
        uint64_t fs;
        uint64_t es;
        uint64_t ds;
        uint64_t rax;
        uint64_t rbp;
        uint64_t err_code;
        uint64_t rip, cs, rflags, rsp, ss;
};


namespace {
        void print_regs(struct TrapFrame const *frm) {
                uint64_t cr0;
                uint64_t cr2;
                uint64_t cr3;
                uint64_t cr4;
                uint64_t cr8;
                __asm__ volatile("mov %0, cr0\n" : "=r"(cr0));
                __asm__ volatile("mov %0, cr2\n" : "=r"(cr2));
                __asm__ volatile("mov %0, cr3\n" : "=r"(cr3));
                __asm__ volatile("mov %0, cr4\n" : "=r"(cr4));
                __asm__ volatile("mov %0, cr8\n" : "=r"(cr8));

                console_alert("---------- CPU REGISTER DUMP ----------");
                console_alert(
                        "RAX=%lx RBX=%lx RCX=%lx RDX=%lx",
                        frm->rax,
                        frm->rbx,
                        frm->rcx,
                        frm->rdx
                );
                console_alert(
                        "RSI=%lx RDI=%lx RBP=%lx RSP=%lx",
                        frm->rsi,
                        frm->rdi,
                        frm->rbp,
                        frm->rsp
                );
                console_alert(
                        "R8 =%lx R9 =%lx R10=%lx R11=%lx",
                        frm->r8,
                        frm->r9,
                        frm->r10,
                        frm->r11
                );
                console_alert(
                        "R12=%lx R13=%lx R14=%lx R15=%lx",
                        frm->r12,
                        frm->r13,
                        frm->r14,
                        frm->r15
                );
                console_alert(
                        "CS=%lx DS=%lx ES=%lx FS=%lx",
                        frm->cs,
                        frm->ds,
                        frm->es,
                        frm->fs
                );
                console_alert(
                        "GS=%lx SS=%lx RIP=%lx RFLAGS=%lx",
                        frm->gs,
                        frm->ss,
                        frm->rip,
                        frm->rflags
                );
                console_alert("---------- CURRENT CONTROL REGISTERS and MSRS ----------"
                );
                console_alert("CR0=%lx CR2=%lx CR3=%lx CR4=%lx", cr0, cr2, cr3, cr4);
                console_alert("CR8=%lx", cr8);
                console_alert(
                        "IA32_EFER=%lx FS.base=%lx GS.base=%lx",
                        rdmsr(MSR_IA32_EFER),
                        rdmsr(MSR_FS_BASE),
                        rdmsr(MSR_GS_BASE)
                );
                console_alert("KernelGSBase=%lx", rdmsr(MSR_KERNEL_GS_BASE));
        }

        void print_stacktrace(struct TrapFrame const *frm) {
                console_alert("EXCEPTION OCCURED AT: %p", (void *)frm->rip);
                console_alert("EXCEPTION STACK TRACE:");
                stacktrace_show_using_rbp((void *)frm->rbp);
        }

        [[noreturn]] void generic_exc_no_code(struct TrapFrame const *frm, uint8_t exc_num) {
                console_alert("EXCEPTION %u", exc_num);
                print_regs(frm);
                print_stacktrace(frm);
                panic("CPU exception");
        }

        [[noreturn]] void generic_exc(struct TrapFrame const *frm, uint8_t exc_num) {
                ASSERT(((uintptr_t)frm & 0xf) == 0);
                console_alert("EXCEPTION %u (Error code %x)", exc_num, frm->err_code);
                print_regs(frm);
                print_stacktrace(frm);
                panic("CPU exception");
        }
}

extern "C" {
[[gnu::used]] void isr_exc0_handler(struct TrapFrame *frm) { generic_exc_no_code(frm, 0); }
[[gnu::used]] void isr_exc1_handler(struct TrapFrame *frm) { generic_exc_no_code(frm, 1); }
[[gnu::used]] void isr_exc2_handler(struct TrapFrame *frm) { generic_exc_no_code(frm, 2); }
[[gnu::used]] void isr_exc3_handler(struct TrapFrame *frm) { generic_exc_no_code(frm, 3); }
[[gnu::used]] void isr_exc4_handler(struct TrapFrame *frm) { generic_exc_no_code(frm, 4); }
[[gnu::used]] void isr_exc5_handler(struct TrapFrame *frm) { generic_exc_no_code(frm, 5); }
[[gnu::used]] void isr_exc6_handler(struct TrapFrame *frm) { generic_exc_no_code(frm, 6); }
[[gnu::used]] void isr_exc7_handler(struct TrapFrame *frm) { generic_exc_no_code(frm, 7); }
[[gnu::used]] void isr_exc8_handler(struct TrapFrame *frm) { generic_exc(frm, 8); }
[[gnu::used]] void isr_exc9_handler(struct TrapFrame *frm) { generic_exc_no_code(frm, 9); }
[[gnu::used]] void isr_exc10_handler(struct TrapFrame *frm) { generic_exc(frm, 10); }
[[gnu::used]] void isr_exc11_handler(struct TrapFrame *frm) { generic_exc(frm, 11); }
[[gnu::used]] void isr_exc12_handler(struct TrapFrame *frm) { generic_exc(frm, 12); }
[[gnu::used]] void isr_exc13_handler(struct TrapFrame *frm) {
        enum {
                FLAG_E = 1 << 0,
                FLAG_TABLE_MASK = 0x3 << 1,
                FLAG_TABLE_GDT = 0x0 << 1,
                // Looking at OSDev wiki
                // (https://wiki.osdev.org/Exception#Selector_Error_Code),
                // FLAG_TBL_IDT_A and FLAG_TBL_IDT_B seems identical.
                // Intel's documentation doesn't say much about the
                // exact format.
                FLAG_TABLE_IDT_A = 0x1 << 1,
                FLAG_TABLE_LDT = 0x2 << 1,
                FLAG_TABLE_IDT_B = 0x3 << 1,
                FLAG_SELECTOR_INDEX_MASK = 0xfff8,
                FLAG_SELECTOR_INDEX_OFFSET = 3,
        };

        uint64_t err = frm->err_code;
        if (err == 0) {
                console_alert("EXCEPTION 13(#GP) -- Unknown reason");
        } else {
                char const *src_name;
                switch ((err & FLAG_TABLE_MASK)) {
                case FLAG_TABLE_GDT:
                        src_name = "GDT";
                        break;
                case FLAG_TABLE_IDT_A:
                case FLAG_TABLE_IDT_B:
                        src_name = "IDT";
                        break;
                case FLAG_TABLE_LDT:
                        src_name = "LDT";
                        break;
                default:
                        UNREACHABLE();
                }
                uint16_t index = (err & FLAG_SELECTOR_INDEX_MASK) >>
                                 FLAG_SELECTOR_INDEX_OFFSET;
                bool origin_external = (err & FLAG_E) == FLAG_E;
                console_alert(
                        "EXCEPTION 13(#GP) -- Related segment: %s[%u] %s",
                        src_name,
                        index,
                        origin_external ? " <Origin is external>" : ""
                );
        }
        console_alert("(Raw error code is %x)", err);
        print_regs(frm);
        print_stacktrace(frm);
        panic("CPU exception");
}
[[gnu::used]] void isr_exc14_handler(struct TrapFrame *frm) {
        enum {
                FLAG_P = 1,
                FLAG_W = 1 << 1,
                FLAG_U = 1 << 2,
                FLAG_R = 1 << 3,
                FLAG_I = 1 << 4,
                FLAG_PK = 1 << 5,
                FLAG_SS = 1 << 6,
        };

        uint64_t err = frm->err_code;
        void *virtaddr;
        __asm__ volatile("mov %0, cr2" : "=r"(virtaddr));
        bool is_present = (err & FLAG_P) == FLAG_P;
        bool is_write = (err & FLAG_W) == FLAG_W;
        bool is_exec = (err & FLAG_I) == FLAG_I;
        bool is_user_page;
        bool do_prot_test = false;
        uint32_t prot_to_test = 0;
        if (!is_present) {
                do_prot_test = true;
        } else {
                if (is_write) {
                        do_prot_test = true;
                        prot_to_test |= MMU_PROT_WRITE;
                }
                if (is_exec) {
                        do_prot_test = true;
                        prot_to_test |= MMU_PROT_EXEC;
                }
        }

        if (do_prot_test && mmu_is_accessible(virtaddr, prot_to_test)) {
                // This was likely just TLB cache issue.
                mmu_invalidate_local_tlb_for(virtaddr);
                return;
        }
        if (is_present) {
                is_user_page = mmu_is_accessible(virtaddr, MMU_PROT_USER);
        } else {
                is_user_page = false;
        }
        console_alert(
                "EXCEPTION 14(#PF) at %p(User=%u) [R=%u PK=%u SS=%u W=%u I=%u U=%u P=%u]",
                virtaddr,
                is_user_page,
                (err & FLAG_R) == FLAG_R,
                (err & FLAG_PK) == FLAG_PK,
                (err & FLAG_SS) == FLAG_SS,
                (err & FLAG_W) == FLAG_W,
                (err & FLAG_I) == FLAG_I,
                (err & FLAG_U) == FLAG_U,
                (err & FLAG_P) == FLAG_P
        );
        console_alert("(Raw error code is %x)", err);
        print_regs(frm);
        print_stacktrace(frm);
        panic("CPU exception");
}
[[gnu::used]] void isr_exc15_handler(struct TrapFrame *frm) { generic_exc_no_code(frm, 15); }
[[gnu::used]] void isr_exc16_handler(struct TrapFrame *frm) { generic_exc_no_code(frm, 16); }
[[gnu::used]] void isr_exc17_handler(struct TrapFrame *frm) { generic_exc(frm, 17); }
[[gnu::used]] void isr_exc18_handler(struct TrapFrame *frm) { generic_exc_no_code(frm, 18); }
[[gnu::used]] void isr_exc19_handler(struct TrapFrame *frm) { generic_exc_no_code(frm, 19); }
[[gnu::used]] void isr_exc20_handler(struct TrapFrame *frm) { generic_exc_no_code(frm, 20); }
[[gnu::used]] void isr_exc21_handler(struct TrapFrame *frm) { generic_exc(frm, 21); }
[[gnu::used]] void isr_exc22_handler(struct TrapFrame *frm) { generic_exc_no_code(frm, 22); }
[[gnu::used]] void isr_exc23_handler(struct TrapFrame *frm) { generic_exc_no_code(frm, 23); }
[[gnu::used]] void isr_exc24_handler(struct TrapFrame *frm) { generic_exc_no_code(frm, 24); }
[[gnu::used]] void isr_exc25_handler(struct TrapFrame *frm) { generic_exc_no_code(frm, 25); }
[[gnu::used]] void isr_exc26_handler(struct TrapFrame *frm) { generic_exc_no_code(frm, 26); }
[[gnu::used]] void isr_exc27_handler(struct TrapFrame *frm) { generic_exc_no_code(frm, 27); }
[[gnu::used]] void isr_exc28_handler(struct TrapFrame *frm) { generic_exc_no_code(frm, 28); }
[[gnu::used]] void isr_exc29_handler(struct TrapFrame *frm) { generic_exc(frm, 29); }
[[gnu::used]] void isr_exc30_handler(struct TrapFrame *frm) { generic_exc(frm, 30); }
[[gnu::used]] void isr_exc31_handler(struct TrapFrame *frm) { generic_exc_no_code(frm, 31); }
}

extern "C" [[gnu::used]] void isr_handle_interrupt(struct TrapFrame *frm, uint64_t int_num) {
        (void)frm;
        ASSERT(!interrupts_are_enabled());
        ASSERT(32 <= int_num); // 0~31 are for exceptions

        switch (int_num) {
        case LAPIC_SPURIOUS_VECTOR:
                TODO();
                break;
        case LAPIC_THERMAL_SENSOR_VECTOR:
                TODO();
                break;
        case LAPIC_PERFORMENCE_COUNTER_VECTOR:
                TODO();
                break;
        case LAPIC_ERROR_VECTOR:
                TODO();
                break;
        case LAPIC_LINT1_VECTOR:
                TODO();
                break;
        case LAPIC_LINT0_VECTOR:
                TODO();
                break;
        case LAPIC_CMCI_VECTOR:
                TODO();
                break;
        case LAPIC_BROADCAST_IPI_VECTOR:
                processor_process_ipimessages();
                lapic_send_eoi();
                break;
        case LAPIC_TIMER_VECTOR:
                // EOI must be sent first, because we might end up switching to
                // different task without sending EOI if we sent EOI after timer
                // handler.
                lapic_send_eoi();
                ticktime_increment_tick();
                break;
        default:
                // Interrupt request
                if ((PIC_VECTOR_MIN <= int_num) &&
                    (int_num <= PIC_VECTOR_MAX)) {
                        // Legacy PIC
                        interrupts_on_interrupt(int_num - PIC_VECTOR_MIN);
                        if (int_num == 0) {
                                ticktime_increment_tick();
                        }
                        i8259pic_send_eoi(int_num);
                } else if (IOAPIC_IRQ_VECTOR_BASE <= int_num) {
                        // I/O APIC
                        uint32_t gsi = int_num - IOAPIC_IRQ_VECTOR_BASE;
                        uint32_t i8259_timer_gsi;
                        if (ioapic_legacy_irq_to_gsi(&i8259_timer_gsi, 0) &&
                            i8259_timer_gsi == gsi) {
                                ticktime_increment_tick();
                        } else {
                                interrupts_on_interrupt(gsi);
                        }
                        lapic_send_eoi();
                } else {
                        panic("Unexpected interrupt #%u", int_num);
                }
                break;
        }
        ASSERT(!interrupts_are_enabled());
}

////////////////////////////////////////////////////////////////////////////////
//  Interrupt / Exception handler entry
////////////////////////////////////////////////////////////////////////////////

#define ASM_ISR_HEADER_EXC "cli\n"
#define ASM_PUSH_FAKE_CODE "push 0\n"

// Determine whether we are coming from kerner or userland by looking at saved
// CS value, and perform SWAPGS if it's userland.
#define ASM_SWAPGS                         \
        "cmp qword ptr [rsp + 16], 0x08\n" \
        "je 1f\n"                          \
        "swapgs\n"                         \
        "1:\n"

#define SAVED_GS_BASE_UPPER32_OFFSET X86_MISC_STATE_SAVED_GS_BASE_UPPER32_OFFSET

// Similar to ASM_SWAPGS, but used in cases where the exception can occur after
// entring the entry but before SWAPGS happens. #DB, NMI, #MC are examples of
// this. If that happens, we would see kernel CS if we used normal ASM_SWAPGS,
// since we are technically coming from another handler and not perform SWAPGS,
// which is bad.
//
// So this version doesn't rely on the CS value, but the actual GS.Base MSR's
// value to determine the source. But be warned that unlike ASM_SWAPGS, there
// are two macros in this case, one for the entry and another for the exit.
//
// This is because bahavior between two is very different:
// - On entry: We check GS using current GS.Base MSR, and save it to memory
//             location.
// - On exit: We check GS using GS.Base we saved on entry. MSR is not used.

// clang-format off
#define ASM_SWAPGS_EXPENSIVE_ENTRY                                             \
  "push rax\n"                                                                 \
  "push rcx\n"                                                                 \
  "push rdx\n"                                                                 \
  "mov ecx, " STRINGIFY(MSR_GS_BASE) "\n"                                      \
  "rdmsr\n"                                                                    \
  "cmp edx, 0\n"                                                               \
  "jnz 2f\n"                                                                   \
  "1:\n"                                                                       \
  "swapgs\n"                                                                   \
  "2:\n"                                                                       \
  "mov dword ptr [gs:" STRINGIFY(SAVED_GS_BASE_UPPER32_OFFSET) "], edx\n"      \
  "pop rdx\n"                                                                  \
  "pop rcx\n"                                                                  \
  "pop rax\n"
// clang-format on

// clang-format off
#define ASM_SWAPGS_EXPENSIVE_EXIT                                              \
        "cmp dword ptr [gs:" STRINGIFY(SAVED_GS_BASE_UPPER32_OFFSET ) "], 0 \n" \
        "jne 2f\n"                                                             \
        "1:\n"                                                                 \
        "swapgs\n"                                                             \
        "2:\n"
// clang-format on

// NOTE: RSP, SS, RIP, CS, RFLAGS and error code was already saved by processor.
//       We just need to save the rest.
#define ASM_SAVE_REGS   \
        "push rbp\n"    \
        "push rax\n"    \
        "mov rax, ds\n" \
        "push rax\n"    \
        "mov rax, es\n" \
        "push rax\n"    \
        "mov rax, fs\n" \
        "push rax\n"    \
        "mov rax, gs\n" \
        "push rax\n"    \
        "push rbx\n"    \
        "push rcx\n"    \
        "push rdx\n"    \
        "push rsi\n"    \
        "push rdi\n"    \
        "push r8\n"     \
        "push r9\n"     \
        "push r10\n"    \
        "push r11\n"    \
        "push r12\n"    \
        "push r13\n"    \
        "push r14\n"    \
        "push r15\n"

// NOTE: We don't restore FS and GS selector value, because it really doesn't
//       matter at all. (And I think reloading them actually resets MSR values)

#define ASM_RESTORE_REGS                 \
        "pop r15\n"                      \
        "pop r14\n"                      \
        "pop r13\n"                      \
        "pop r12\n"                      \
        "pop r11\n"                      \
        "pop r10\n"                      \
        "pop r9\n"                       \
        "pop r8\n"                       \
        "pop rdi\n"                      \
        "pop rsi\n"                      \
        "pop rdx\n"                      \
        "pop rcx\n"                      \
        "pop rbx\n" /* Skip FS and GS */ \
        "add rsp, 16\n"                  \
        "pop rax\n"                      \
        "mov es, rax\n"                  \
        "pop rax\n"                      \
        "mov ds, rax\n"                  \
        "pop rax\n"                      \
        "pop rbp\n"

#define ASM_ISR_FOOTER \
        "add rsp, 8\n" \
        "iretq\n"

// clang-format off
#define ASM_CALL_EXC_HANDLER(__handler_fn)                          \
        "mov rdi, rsp\n" /* Provide stack pointer as argument*/ \
        "sub rsp, 8\n"   /* Align stack to 16-byte boundary */  \
        "call " #__handler_fn "\n"                              \
        "add rsp, 8\n"   /* Undo sub rsp, 8 above */

#define IMPL_ENTRY_EXC(__n) \
        __asm__ volatile( \
                ASM_ISR_HEADER_EXC \
                ASM_SWAPGS \
                ASM_SAVE_REGS \
                ASM_CALL_EXC_HANDLER(isr_exc##__n##_handler) \
                ASM_RESTORE_REGS \
                ASM_SWAPGS \
                ASM_ISR_FOOTER \
        )

#define IMPL_ENTRY_EXC_NOCODE(__n) \
        __asm__ volatile( \
                ASM_ISR_HEADER_EXC \
                ASM_PUSH_FAKE_CODE /* Stack will be aligned to 16-byte boundary after this */ \
                ASM_SWAPGS \
                ASM_SAVE_REGS \
                ASM_CALL_EXC_HANDLER(isr_exc##__n##_handler) \
                ASM_RESTORE_REGS \
                ASM_SWAPGS \
                ASM_ISR_FOOTER \
        )

#define IMPL_ENTRY_EXC_WITH_EXPENSIVE_GS_BASE_CHECK(__n) \
        __asm__ volatile( \
                ASM_ISR_HEADER_EXC \
                ASM_PUSH_FAKE_CODE /* Stack will be aligned to 16-byte boundary after this */ \
                ASM_SWAPGS_EXPENSIVE_ENTRY \
                ASM_SAVE_REGS \
                ASM_CALL_EXC_HANDLER(isr_exc##__n##_handler) \
                ASM_RESTORE_REGS \
                ASM_SWAPGS_EXPENSIVE_EXIT \
                ASM_ISR_FOOTER \
        )

// clang-format on

[[gnu::naked]] void exc0_entry() { IMPL_ENTRY_EXC_NOCODE(0); }
[[gnu::naked]] void exc1_entry() { IMPL_ENTRY_EXC_WITH_EXPENSIVE_GS_BASE_CHECK(1); }
[[gnu::naked]] void exc2_entry() { IMPL_ENTRY_EXC_WITH_EXPENSIVE_GS_BASE_CHECK(2); }
[[gnu::naked]] void exc3_entry() { IMPL_ENTRY_EXC_NOCODE(3); }
[[gnu::naked]] void exc4_entry() { IMPL_ENTRY_EXC_NOCODE(4); }
[[gnu::naked]] void exc5_entry() { IMPL_ENTRY_EXC_NOCODE(5); }
[[gnu::naked]] void exc6_entry() { IMPL_ENTRY_EXC_NOCODE(6); }
[[gnu::naked]] void exc7_entry() { IMPL_ENTRY_EXC_NOCODE(7); }
[[gnu::naked]] void exc8_entry() { IMPL_ENTRY_EXC(8); }
[[gnu::naked]] void exc9_entry() { IMPL_ENTRY_EXC_NOCODE(9); }
[[gnu::naked]] void exc10_entry() { IMPL_ENTRY_EXC(10); }
[[gnu::naked]] void exc11_entry() { IMPL_ENTRY_EXC(11); }
[[gnu::naked]] void exc12_entry() { IMPL_ENTRY_EXC(12); }
[[gnu::naked]] void exc13_entry() { IMPL_ENTRY_EXC(13); }
[[gnu::naked]] void exc14_entry() { IMPL_ENTRY_EXC(14); }
[[gnu::naked]] void exc15_entry() { IMPL_ENTRY_EXC_NOCODE(15); }
[[gnu::naked]] void exc16_entry() { IMPL_ENTRY_EXC_NOCODE(16); }
[[gnu::naked]] void exc17_entry() { IMPL_ENTRY_EXC(17); }
[[gnu::naked]] void exc18_entry() { IMPL_ENTRY_EXC_WITH_EXPENSIVE_GS_BASE_CHECK(18); }
[[gnu::naked]] void exc19_entry() { IMPL_ENTRY_EXC_NOCODE(19); }
[[gnu::naked]] void exc20_entry() { IMPL_ENTRY_EXC_NOCODE(20); }
[[gnu::naked]] void exc21_entry() { IMPL_ENTRY_EXC(21); }
[[gnu::naked]] void exc22_entry() { IMPL_ENTRY_EXC_NOCODE(22); }
[[gnu::naked]] void exc23_entry() { IMPL_ENTRY_EXC_NOCODE(23); }
[[gnu::naked]] void exc24_entry() { IMPL_ENTRY_EXC_NOCODE(24); }
[[gnu::naked]] void exc25_entry() { IMPL_ENTRY_EXC_NOCODE(25); }
[[gnu::naked]] void exc26_entry() { IMPL_ENTRY_EXC_NOCODE(26); }
[[gnu::naked]] void exc27_entry() { IMPL_ENTRY_EXC_NOCODE(27); }
[[gnu::naked]] void exc28_entry() { IMPL_ENTRY_EXC_NOCODE(28); }
[[gnu::naked]] void exc29_entry() { IMPL_ENTRY_EXC(29); }
[[gnu::naked]] void exc30_entry() { IMPL_ENTRY_EXC(30); }
[[gnu::naked]] void exc31_entry() { IMPL_ENTRY_EXC_NOCODE(31); }

// clang-format off
#define ASM_CALL_INTERRUPT_HANDLER(__n) \
        "mov rdi, rsp\n"      /* Provide stack pointer as argument*/ \
        "mov rsi, " #__n "\n" /* Provide interrupt number as argument*/ \
        "sub rsp, 8\n"        /* Align stack to 16-byte boundary */ \
        "call isr_handle_interrupt\n" \
        "add rsp, 8\n"        /* Undo <sub rsp, 8> above */

#define IMPL_ENTRY_INT(__n) \
        __asm__ volatile( \
                ASM_PUSH_FAKE_CODE /* Stack will be aligned to 16-byte boundary after this */ \
                ASM_SWAPGS \
                ASM_SAVE_REGS \
                ASM_CALL_INTERRUPT_HANDLER(__n) \
                ASM_RESTORE_REGS \
                ASM_SWAPGS \
                ASM_ISR_FOOTER \
        );

// clang-format on

[[gnu::naked]] void int32_entry() { IMPL_ENTRY_INT(32); }
[[gnu::naked]] void int33_entry() { IMPL_ENTRY_INT(33); }
[[gnu::naked]] void int34_entry() { IMPL_ENTRY_INT(34); }
[[gnu::naked]] void int35_entry() { IMPL_ENTRY_INT(35); }
[[gnu::naked]] void int36_entry() { IMPL_ENTRY_INT(36); }
[[gnu::naked]] void int37_entry() { IMPL_ENTRY_INT(37); }
[[gnu::naked]] void int38_entry() { IMPL_ENTRY_INT(38); }
[[gnu::naked]] void int39_entry() { IMPL_ENTRY_INT(39); }
[[gnu::naked]] void int40_entry() { IMPL_ENTRY_INT(40); }
[[gnu::naked]] void int41_entry() { IMPL_ENTRY_INT(41); }
[[gnu::naked]] void int42_entry() { IMPL_ENTRY_INT(42); }
[[gnu::naked]] void int43_entry() { IMPL_ENTRY_INT(43); }
[[gnu::naked]] void int44_entry() { IMPL_ENTRY_INT(44); }
[[gnu::naked]] void int45_entry() { IMPL_ENTRY_INT(45); }
[[gnu::naked]] void int46_entry() { IMPL_ENTRY_INT(46); }
[[gnu::naked]] void int47_entry() { IMPL_ENTRY_INT(47); }
[[gnu::naked]] void int48_entry() { IMPL_ENTRY_INT(48); }
[[gnu::naked]] void int49_entry() { IMPL_ENTRY_INT(49); }
[[gnu::naked]] void int50_entry() { IMPL_ENTRY_INT(50); }
[[gnu::naked]] void int51_entry() { IMPL_ENTRY_INT(51); }
[[gnu::naked]] void int52_entry() { IMPL_ENTRY_INT(52); }
[[gnu::naked]] void int53_entry() { IMPL_ENTRY_INT(53); }
[[gnu::naked]] void int54_entry() { IMPL_ENTRY_INT(54); }
[[gnu::naked]] void int55_entry() { IMPL_ENTRY_INT(55); }
[[gnu::naked]] void int56_entry() { IMPL_ENTRY_INT(56); }
[[gnu::naked]] void int57_entry() { IMPL_ENTRY_INT(57); }
[[gnu::naked]] void int58_entry() { IMPL_ENTRY_INT(58); }
[[gnu::naked]] void int59_entry() { IMPL_ENTRY_INT(59); }
[[gnu::naked]] void int60_entry() { IMPL_ENTRY_INT(60); }
[[gnu::naked]] void int61_entry() { IMPL_ENTRY_INT(61); }
[[gnu::naked]] void int62_entry() { IMPL_ENTRY_INT(62); }
[[gnu::naked]] void int63_entry() { IMPL_ENTRY_INT(63); }
[[gnu::naked]] void int64_entry() { IMPL_ENTRY_INT(64); }
[[gnu::naked]] void int65_entry() { IMPL_ENTRY_INT(65); }
[[gnu::naked]] void int66_entry() { IMPL_ENTRY_INT(66); }
[[gnu::naked]] void int67_entry() { IMPL_ENTRY_INT(67); }
[[gnu::naked]] void int68_entry() { IMPL_ENTRY_INT(68); }
[[gnu::naked]] void int69_entry() { IMPL_ENTRY_INT(69); }
[[gnu::naked]] void int70_entry() { IMPL_ENTRY_INT(70); }
[[gnu::naked]] void int71_entry() { IMPL_ENTRY_INT(71); }
[[gnu::naked]] void int72_entry() { IMPL_ENTRY_INT(72); }
[[gnu::naked]] void int73_entry() { IMPL_ENTRY_INT(73); }
[[gnu::naked]] void int74_entry() { IMPL_ENTRY_INT(74); }
[[gnu::naked]] void int75_entry() { IMPL_ENTRY_INT(75); }
[[gnu::naked]] void int76_entry() { IMPL_ENTRY_INT(76); }
[[gnu::naked]] void int77_entry() { IMPL_ENTRY_INT(77); }
[[gnu::naked]] void int78_entry() { IMPL_ENTRY_INT(78); }
[[gnu::naked]] void int79_entry() { IMPL_ENTRY_INT(79); }
[[gnu::naked]] void int80_entry() { IMPL_ENTRY_INT(80); }
[[gnu::naked]] void int81_entry() { IMPL_ENTRY_INT(81); }
[[gnu::naked]] void int82_entry() { IMPL_ENTRY_INT(82); }
[[gnu::naked]] void int83_entry() { IMPL_ENTRY_INT(83); }
[[gnu::naked]] void int84_entry() { IMPL_ENTRY_INT(84); }
[[gnu::naked]] void int85_entry() { IMPL_ENTRY_INT(85); }
[[gnu::naked]] void int86_entry() { IMPL_ENTRY_INT(86); }
[[gnu::naked]] void int87_entry() { IMPL_ENTRY_INT(87); }
[[gnu::naked]] void int88_entry() { IMPL_ENTRY_INT(88); }
[[gnu::naked]] void int89_entry() { IMPL_ENTRY_INT(89); }
[[gnu::naked]] void int90_entry() { IMPL_ENTRY_INT(90); }
[[gnu::naked]] void int91_entry() { IMPL_ENTRY_INT(91); }
[[gnu::naked]] void int92_entry() { IMPL_ENTRY_INT(92); }
[[gnu::naked]] void int93_entry() { IMPL_ENTRY_INT(93); }
[[gnu::naked]] void int94_entry() { IMPL_ENTRY_INT(94); }
[[gnu::naked]] void int95_entry() { IMPL_ENTRY_INT(95); }
[[gnu::naked]] void int96_entry() { IMPL_ENTRY_INT(96); }
[[gnu::naked]] void int97_entry() { IMPL_ENTRY_INT(97); }
[[gnu::naked]] void int98_entry() { IMPL_ENTRY_INT(98); }
[[gnu::naked]] void int99_entry() { IMPL_ENTRY_INT(99); }
[[gnu::naked]] void int100_entry() { IMPL_ENTRY_INT(100); }
[[gnu::naked]] void int101_entry() { IMPL_ENTRY_INT(101); }
[[gnu::naked]] void int102_entry() { IMPL_ENTRY_INT(102); }
[[gnu::naked]] void int103_entry() { IMPL_ENTRY_INT(103); }
[[gnu::naked]] void int104_entry() { IMPL_ENTRY_INT(104); }
[[gnu::naked]] void int105_entry() { IMPL_ENTRY_INT(105); }
[[gnu::naked]] void int106_entry() { IMPL_ENTRY_INT(106); }
[[gnu::naked]] void int107_entry() { IMPL_ENTRY_INT(107); }
[[gnu::naked]] void int108_entry() { IMPL_ENTRY_INT(108); }
[[gnu::naked]] void int109_entry() { IMPL_ENTRY_INT(109); }
[[gnu::naked]] void int110_entry() { IMPL_ENTRY_INT(110); }
[[gnu::naked]] void int111_entry() { IMPL_ENTRY_INT(111); }
[[gnu::naked]] void int112_entry() { IMPL_ENTRY_INT(112); }
[[gnu::naked]] void int113_entry() { IMPL_ENTRY_INT(113); }
[[gnu::naked]] void int114_entry() { IMPL_ENTRY_INT(114); }
[[gnu::naked]] void int115_entry() { IMPL_ENTRY_INT(115); }
[[gnu::naked]] void int116_entry() { IMPL_ENTRY_INT(116); }
[[gnu::naked]] void int117_entry() { IMPL_ENTRY_INT(117); }
[[gnu::naked]] void int118_entry() { IMPL_ENTRY_INT(118); }
[[gnu::naked]] void int119_entry() { IMPL_ENTRY_INT(119); }
[[gnu::naked]] void int120_entry() { IMPL_ENTRY_INT(120); }
[[gnu::naked]] void int121_entry() { IMPL_ENTRY_INT(121); }
[[gnu::naked]] void int122_entry() { IMPL_ENTRY_INT(122); }
[[gnu::naked]] void int123_entry() { IMPL_ENTRY_INT(123); }
[[gnu::naked]] void int124_entry() { IMPL_ENTRY_INT(124); }
[[gnu::naked]] void int125_entry() { IMPL_ENTRY_INT(125); }
[[gnu::naked]] void int126_entry() { IMPL_ENTRY_INT(126); }
[[gnu::naked]] void int127_entry() { IMPL_ENTRY_INT(127); }
[[gnu::naked]] void int128_entry() { IMPL_ENTRY_INT(128); }
[[gnu::naked]] void int129_entry() { IMPL_ENTRY_INT(129); }
[[gnu::naked]] void int130_entry() { IMPL_ENTRY_INT(130); }
[[gnu::naked]] void int131_entry() { IMPL_ENTRY_INT(131); }
[[gnu::naked]] void int132_entry() { IMPL_ENTRY_INT(132); }
[[gnu::naked]] void int133_entry() { IMPL_ENTRY_INT(133); }
[[gnu::naked]] void int134_entry() { IMPL_ENTRY_INT(134); }
[[gnu::naked]] void int135_entry() { IMPL_ENTRY_INT(135); }
[[gnu::naked]] void int136_entry() { IMPL_ENTRY_INT(136); }
[[gnu::naked]] void int137_entry() { IMPL_ENTRY_INT(137); }
[[gnu::naked]] void int138_entry() { IMPL_ENTRY_INT(138); }
[[gnu::naked]] void int139_entry() { IMPL_ENTRY_INT(139); }
[[gnu::naked]] void int140_entry() { IMPL_ENTRY_INT(140); }
[[gnu::naked]] void int141_entry() { IMPL_ENTRY_INT(141); }
[[gnu::naked]] void int142_entry() { IMPL_ENTRY_INT(142); }
[[gnu::naked]] void int143_entry() { IMPL_ENTRY_INT(143); }
[[gnu::naked]] void int144_entry() { IMPL_ENTRY_INT(144); }
[[gnu::naked]] void int145_entry() { IMPL_ENTRY_INT(145); }
[[gnu::naked]] void int146_entry() { IMPL_ENTRY_INT(146); }
[[gnu::naked]] void int147_entry() { IMPL_ENTRY_INT(147); }
[[gnu::naked]] void int148_entry() { IMPL_ENTRY_INT(148); }
[[gnu::naked]] void int149_entry() { IMPL_ENTRY_INT(149); }
[[gnu::naked]] void int150_entry() { IMPL_ENTRY_INT(150); }
[[gnu::naked]] void int151_entry() { IMPL_ENTRY_INT(151); }
[[gnu::naked]] void int152_entry() { IMPL_ENTRY_INT(152); }
[[gnu::naked]] void int153_entry() { IMPL_ENTRY_INT(153); }
[[gnu::naked]] void int154_entry() { IMPL_ENTRY_INT(154); }
[[gnu::naked]] void int155_entry() { IMPL_ENTRY_INT(155); }
[[gnu::naked]] void int156_entry() { IMPL_ENTRY_INT(156); }
[[gnu::naked]] void int157_entry() { IMPL_ENTRY_INT(157); }
[[gnu::naked]] void int158_entry() { IMPL_ENTRY_INT(158); }
[[gnu::naked]] void int159_entry() { IMPL_ENTRY_INT(159); }
[[gnu::naked]] void int160_entry() { IMPL_ENTRY_INT(160); }
[[gnu::naked]] void int161_entry() { IMPL_ENTRY_INT(161); }
[[gnu::naked]] void int162_entry() { IMPL_ENTRY_INT(162); }
[[gnu::naked]] void int163_entry() { IMPL_ENTRY_INT(163); }
[[gnu::naked]] void int164_entry() { IMPL_ENTRY_INT(164); }
[[gnu::naked]] void int165_entry() { IMPL_ENTRY_INT(165); }
[[gnu::naked]] void int166_entry() { IMPL_ENTRY_INT(166); }
[[gnu::naked]] void int167_entry() { IMPL_ENTRY_INT(167); }
[[gnu::naked]] void int168_entry() { IMPL_ENTRY_INT(168); }
[[gnu::naked]] void int169_entry() { IMPL_ENTRY_INT(169); }
[[gnu::naked]] void int170_entry() { IMPL_ENTRY_INT(170); }
[[gnu::naked]] void int171_entry() { IMPL_ENTRY_INT(171); }
[[gnu::naked]] void int172_entry() { IMPL_ENTRY_INT(172); }
[[gnu::naked]] void int173_entry() { IMPL_ENTRY_INT(173); }
[[gnu::naked]] void int174_entry() { IMPL_ENTRY_INT(174); }
[[gnu::naked]] void int175_entry() { IMPL_ENTRY_INT(175); }
[[gnu::naked]] void int176_entry() { IMPL_ENTRY_INT(176); }
[[gnu::naked]] void int177_entry() { IMPL_ENTRY_INT(177); }
[[gnu::naked]] void int178_entry() { IMPL_ENTRY_INT(178); }
[[gnu::naked]] void int179_entry() { IMPL_ENTRY_INT(179); }
[[gnu::naked]] void int180_entry() { IMPL_ENTRY_INT(180); }
[[gnu::naked]] void int181_entry() { IMPL_ENTRY_INT(181); }
[[gnu::naked]] void int182_entry() { IMPL_ENTRY_INT(182); }
[[gnu::naked]] void int183_entry() { IMPL_ENTRY_INT(183); }
[[gnu::naked]] void int184_entry() { IMPL_ENTRY_INT(184); }
[[gnu::naked]] void int185_entry() { IMPL_ENTRY_INT(185); }
[[gnu::naked]] void int186_entry() { IMPL_ENTRY_INT(186); }
[[gnu::naked]] void int187_entry() { IMPL_ENTRY_INT(187); }
[[gnu::naked]] void int188_entry() { IMPL_ENTRY_INT(188); }
[[gnu::naked]] void int189_entry() { IMPL_ENTRY_INT(189); }
[[gnu::naked]] void int190_entry() { IMPL_ENTRY_INT(190); }
[[gnu::naked]] void int191_entry() { IMPL_ENTRY_INT(191); }
[[gnu::naked]] void int192_entry() { IMPL_ENTRY_INT(192); }
[[gnu::naked]] void int193_entry() { IMPL_ENTRY_INT(193); }
[[gnu::naked]] void int194_entry() { IMPL_ENTRY_INT(194); }
[[gnu::naked]] void int195_entry() { IMPL_ENTRY_INT(195); }
[[gnu::naked]] void int196_entry() { IMPL_ENTRY_INT(196); }
[[gnu::naked]] void int197_entry() { IMPL_ENTRY_INT(197); }
[[gnu::naked]] void int198_entry() { IMPL_ENTRY_INT(198); }
[[gnu::naked]] void int199_entry() { IMPL_ENTRY_INT(199); }
[[gnu::naked]] void int200_entry() { IMPL_ENTRY_INT(200); }
[[gnu::naked]] void int201_entry() { IMPL_ENTRY_INT(201); }
[[gnu::naked]] void int202_entry() { IMPL_ENTRY_INT(202); }
[[gnu::naked]] void int203_entry() { IMPL_ENTRY_INT(203); }
[[gnu::naked]] void int204_entry() { IMPL_ENTRY_INT(204); }
[[gnu::naked]] void int205_entry() { IMPL_ENTRY_INT(205); }
[[gnu::naked]] void int206_entry() { IMPL_ENTRY_INT(206); }
[[gnu::naked]] void int207_entry() { IMPL_ENTRY_INT(207); }
[[gnu::naked]] void int208_entry() { IMPL_ENTRY_INT(208); }
[[gnu::naked]] void int209_entry() { IMPL_ENTRY_INT(209); }
[[gnu::naked]] void int210_entry() { IMPL_ENTRY_INT(210); }
[[gnu::naked]] void int211_entry() { IMPL_ENTRY_INT(211); }
[[gnu::naked]] void int212_entry() { IMPL_ENTRY_INT(212); }
[[gnu::naked]] void int213_entry() { IMPL_ENTRY_INT(213); }
[[gnu::naked]] void int214_entry() { IMPL_ENTRY_INT(214); }
[[gnu::naked]] void int215_entry() { IMPL_ENTRY_INT(215); }
[[gnu::naked]] void int216_entry() { IMPL_ENTRY_INT(216); }
[[gnu::naked]] void int217_entry() { IMPL_ENTRY_INT(217); }
[[gnu::naked]] void int218_entry() { IMPL_ENTRY_INT(218); }
[[gnu::naked]] void int219_entry() { IMPL_ENTRY_INT(219); }
[[gnu::naked]] void int220_entry() { IMPL_ENTRY_INT(220); }
[[gnu::naked]] void int221_entry() { IMPL_ENTRY_INT(221); }
[[gnu::naked]] void int222_entry() { IMPL_ENTRY_INT(222); }
[[gnu::naked]] void int223_entry() { IMPL_ENTRY_INT(223); }
[[gnu::naked]] void int224_entry() { IMPL_ENTRY_INT(224); }
[[gnu::naked]] void int225_entry() { IMPL_ENTRY_INT(225); }
[[gnu::naked]] void int226_entry() { IMPL_ENTRY_INT(226); }
[[gnu::naked]] void int227_entry() { IMPL_ENTRY_INT(227); }
[[gnu::naked]] void int228_entry() { IMPL_ENTRY_INT(228); }
[[gnu::naked]] void int229_entry() { IMPL_ENTRY_INT(229); }
[[gnu::naked]] void int230_entry() { IMPL_ENTRY_INT(230); }
[[gnu::naked]] void int232_entry() { IMPL_ENTRY_INT(232); }
[[gnu::naked]] void int231_entry() { IMPL_ENTRY_INT(231); }
[[gnu::naked]] void int233_entry() { IMPL_ENTRY_INT(233); }
[[gnu::naked]] void int234_entry() { IMPL_ENTRY_INT(234); }
[[gnu::naked]] void int235_entry() { IMPL_ENTRY_INT(235); }
[[gnu::naked]] void int236_entry() { IMPL_ENTRY_INT(236); }
[[gnu::naked]] void int237_entry() { IMPL_ENTRY_INT(237); }
[[gnu::naked]] void int238_entry() { IMPL_ENTRY_INT(238); }
[[gnu::naked]] void int239_entry() { IMPL_ENTRY_INT(239); }
[[gnu::naked]] void int240_entry() { IMPL_ENTRY_INT(240); }
[[gnu::naked]] void int241_entry() { IMPL_ENTRY_INT(241); }
[[gnu::naked]] void int242_entry() { IMPL_ENTRY_INT(242); }
[[gnu::naked]] void int243_entry() { IMPL_ENTRY_INT(243); }
[[gnu::naked]] void int244_entry() { IMPL_ENTRY_INT(244); }
[[gnu::naked]] void int245_entry() { IMPL_ENTRY_INT(245); }
[[gnu::naked]] void int246_entry() { IMPL_ENTRY_INT(246); }
[[gnu::naked]] void int247_entry() { IMPL_ENTRY_INT(247); }
[[gnu::naked]] void int248_entry() { IMPL_ENTRY_INT(248); }
[[gnu::naked]] void int249_entry() { IMPL_ENTRY_INT(249); }
[[gnu::naked]] void int250_entry() { IMPL_ENTRY_INT(250); }
[[gnu::naked]] void int251_entry() { IMPL_ENTRY_INT(251); }
[[gnu::naked]] void int252_entry() { IMPL_ENTRY_INT(252); }
[[gnu::naked]] void int253_entry() { IMPL_ENTRY_INT(253); }
[[gnu::naked]] void int254_entry() { IMPL_ENTRY_INT(254); }
[[gnu::naked]] void int255_entry() { IMPL_ENTRY_INT(255); }

}