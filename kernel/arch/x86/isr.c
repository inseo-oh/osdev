// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "_internal.h"
#include "arch.h"
#include <kernel/arch/arch.h>
#include <kernel/interrupt/interrupts.h>
#include <kernel/kernel.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdnoreturn.h>
#include <kernel/utility/utility.h>

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

static void print_regs(struct TrapFrame const *frm) {
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

static void print_stacktrace(struct TrapFrame const *frm) {
        console_alert("EXCEPTION OCCURED AT: %p", (void *)frm->rip);
        console_alert("EXCEPTION STACK TRACE:");
        stacktrace_show_using_rbp((void *)frm->rbp);
}

noreturn static void
generic_exc_no_code(struct TrapFrame const *frm, uint8_t exc_num) {
        console_alert("EXCEPTION %u", exc_num);
        print_regs(frm);
        print_stacktrace(frm);
        panic("CPU exception");
}

noreturn static void generic_exc(struct TrapFrame const *frm, uint8_t exc_num) {
        ASSERT(((uintptr_t)frm & 0xf) == 0);
        console_alert("EXCEPTION %u (Error code %x)", exc_num, frm->err_code);
        print_regs(frm);
        print_stacktrace(frm);
        panic("CPU exception");
}

void isr_exc0_handler(struct TrapFrame *frm) { generic_exc_no_code(frm, 0); }

void isr_exc1_handler(struct TrapFrame *frm) { generic_exc_no_code(frm, 1); }

void isr_exc2_handler(struct TrapFrame *frm) { generic_exc_no_code(frm, 2); }

void isr_exc3_handler(struct TrapFrame *frm) { generic_exc_no_code(frm, 3); }

void isr_exc4_handler(struct TrapFrame *frm) { generic_exc_no_code(frm, 4); }

void isr_exc5_handler(struct TrapFrame *frm) { generic_exc_no_code(frm, 5); }

void isr_exc6_handler(struct TrapFrame *frm) { generic_exc_no_code(frm, 6); }

void isr_exc7_handler(struct TrapFrame *frm) { generic_exc_no_code(frm, 7); }

void isr_exc8_handler(struct TrapFrame *frm) { generic_exc(frm, 8); }

void isr_exc9_handler(struct TrapFrame *frm) { generic_exc_no_code(frm, 9); }

void isr_exc10_handler(struct TrapFrame *frm) { generic_exc(frm, 10); }

void isr_exc11_handler(struct TrapFrame *frm) { generic_exc(frm, 11); }

void isr_exc12_handler(struct TrapFrame *frm) { generic_exc(frm, 12); }

void isr_exc13_handler(struct TrapFrame *frm) {
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

void isr_exc14_handler(struct TrapFrame *frm) {
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
        bool is_user_page;
        if (is_present) {
                is_user_page = mmu_is_accessible(virtaddr, MMU_PROT_USER);
        } else {
                is_user_page = false;
        }
        console_alert(
                "EXCEPTION 14(#PF) at %p(User=%u) [R=%u PK=%u SS=%u W=%u I=%u "
                "U=%u P=%u]",
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

void isr_exc15_handler(struct TrapFrame *frm) { generic_exc_no_code(frm, 15); }

void isr_exc16_handler(struct TrapFrame *frm) { generic_exc_no_code(frm, 16); }

void isr_exc17_handler(struct TrapFrame *frm) { generic_exc(frm, 17); }

void isr_exc18_handler(struct TrapFrame *frm) { generic_exc_no_code(frm, 18); }

void isr_exc19_handler(struct TrapFrame *frm) { generic_exc_no_code(frm, 19); }

void isr_exc20_handler(struct TrapFrame *frm) { generic_exc_no_code(frm, 20); }

void isr_exc21_handler(struct TrapFrame *frm) { generic_exc(frm, 21); }

void isr_exc22_handler(struct TrapFrame *frm) { generic_exc_no_code(frm, 22); }

void isr_exc23_handler(struct TrapFrame *frm) { generic_exc_no_code(frm, 23); }

void isr_exc24_handler(struct TrapFrame *frm) { generic_exc_no_code(frm, 24); }

void isr_exc25_handler(struct TrapFrame *frm) { generic_exc_no_code(frm, 25); }

void isr_exc26_handler(struct TrapFrame *frm) { generic_exc_no_code(frm, 26); }

void isr_exc27_handler(struct TrapFrame *frm) { generic_exc_no_code(frm, 27); }

void isr_exc28_handler(struct TrapFrame *frm) { generic_exc_no_code(frm, 28); }

void isr_exc29_handler(struct TrapFrame *frm) { generic_exc(frm, 29); }

void isr_exc30_handler(struct TrapFrame *frm) { generic_exc(frm, 30); }

void isr_exc31_handler(struct TrapFrame *frm) { generic_exc_no_code(frm, 31); }


void isr_handle_interrupt(struct TrapFrame *frm, uint64_t int_num) {
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
  "mov ecx, " STRINGIFY(MSR_GS_BASE) "\n"                                  \
  "rdmsr\n"                                                                    \
  "cmp edx, 0\n"                                                               \
  "jnz 2f\n"                                                                   \
  "1:\n"                                                                       \
  "swapgs\n"                                                                   \
  "2:\n"                                                                       \
  "mov dword ptr [gs:" STRINGIFY(SAVED_GS_BASE_UPPER32_OFFSET) "], edx\n"       \
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

NAKED void isr_exc0_entry() { IMPL_ENTRY_EXC_NOCODE(0); }

NAKED void isr_exc1_entry() { IMPL_ENTRY_EXC_WITH_EXPENSIVE_GS_BASE_CHECK(1); }

NAKED void isr_exc2_entry() { IMPL_ENTRY_EXC_WITH_EXPENSIVE_GS_BASE_CHECK(2); }

NAKED void isr_exc3_entry() { IMPL_ENTRY_EXC_NOCODE(3); }

NAKED void isr_exc4_entry() { IMPL_ENTRY_EXC_NOCODE(4); }

NAKED void isr_exc5_entry() { IMPL_ENTRY_EXC_NOCODE(5); }

NAKED void isr_exc6_entry() { IMPL_ENTRY_EXC_NOCODE(6); }

NAKED void isr_exc7_entry() { IMPL_ENTRY_EXC_NOCODE(7); }

NAKED void isr_exc8_entry() { IMPL_ENTRY_EXC(8); }

NAKED void isr_exc9_entry() { IMPL_ENTRY_EXC_NOCODE(9); }

NAKED void isr_exc10_entry() { IMPL_ENTRY_EXC(10); }

NAKED void isr_exc11_entry() { IMPL_ENTRY_EXC(11); }

NAKED void isr_exc12_entry() { IMPL_ENTRY_EXC(12); }

NAKED void isr_exc13_entry() { IMPL_ENTRY_EXC(13); }

NAKED void isr_exc14_entry() { IMPL_ENTRY_EXC(14); }

NAKED void isr_exc15_entry() { IMPL_ENTRY_EXC_NOCODE(15); }

NAKED void isr_exc16_entry() { IMPL_ENTRY_EXC_NOCODE(16); }

NAKED void isr_exc17_entry() { IMPL_ENTRY_EXC(17); }

NAKED void isr_exc18_entry() {
        IMPL_ENTRY_EXC_WITH_EXPENSIVE_GS_BASE_CHECK(18);
}

NAKED void isr_exc19_entry() { IMPL_ENTRY_EXC_NOCODE(19); }

NAKED void isr_exc20_entry() { IMPL_ENTRY_EXC_NOCODE(20); }

NAKED void isr_exc21_entry() { IMPL_ENTRY_EXC(21); }

NAKED void isr_exc22_entry() { IMPL_ENTRY_EXC_NOCODE(22); }

NAKED void isr_exc23_entry() { IMPL_ENTRY_EXC_NOCODE(23); }

NAKED void isr_exc24_entry() { IMPL_ENTRY_EXC_NOCODE(24); }

NAKED void isr_exc25_entry() { IMPL_ENTRY_EXC_NOCODE(25); }

NAKED void isr_exc26_entry() { IMPL_ENTRY_EXC_NOCODE(26); }

NAKED void isr_exc27_entry() { IMPL_ENTRY_EXC_NOCODE(27); }

NAKED void isr_exc28_entry() { IMPL_ENTRY_EXC_NOCODE(28); }

NAKED void isr_exc29_entry() { IMPL_ENTRY_EXC(29); }

NAKED void isr_exc30_entry() { IMPL_ENTRY_EXC(30); }

NAKED void isr_exc31_entry() { IMPL_ENTRY_EXC_NOCODE(31); }

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

NAKED void isr_int32_entry() { IMPL_ENTRY_INT(32); }

NAKED void isr_int33_entry() { IMPL_ENTRY_INT(33); }

NAKED void isr_int34_entry() { IMPL_ENTRY_INT(34); }

NAKED void isr_int35_entry() { IMPL_ENTRY_INT(35); }

NAKED void isr_int36_entry() { IMPL_ENTRY_INT(36); }

NAKED void isr_int37_entry() { IMPL_ENTRY_INT(37); }

NAKED void isr_int38_entry() { IMPL_ENTRY_INT(38); }

NAKED void isr_int39_entry() { IMPL_ENTRY_INT(39); }

NAKED void isr_int40_entry() { IMPL_ENTRY_INT(40); }

NAKED void isr_int41_entry() { IMPL_ENTRY_INT(41); }

NAKED void isr_int42_entry() { IMPL_ENTRY_INT(42); }

NAKED void isr_int43_entry() { IMPL_ENTRY_INT(43); }

NAKED void isr_int44_entry() { IMPL_ENTRY_INT(44); }

NAKED void isr_int45_entry() { IMPL_ENTRY_INT(45); }

NAKED void isr_int46_entry() { IMPL_ENTRY_INT(46); }

NAKED void isr_int47_entry() { IMPL_ENTRY_INT(47); }

NAKED void isr_int48_entry() { IMPL_ENTRY_INT(48); }

NAKED void isr_int49_entry() { IMPL_ENTRY_INT(49); }

NAKED void isr_int50_entry() { IMPL_ENTRY_INT(50); }

NAKED void isr_int51_entry() { IMPL_ENTRY_INT(51); }

NAKED void isr_int52_entry() { IMPL_ENTRY_INT(52); }

NAKED void isr_int53_entry() { IMPL_ENTRY_INT(53); }

NAKED void isr_int54_entry() { IMPL_ENTRY_INT(54); }

NAKED void isr_int55_entry() { IMPL_ENTRY_INT(55); }

NAKED void isr_int56_entry() { IMPL_ENTRY_INT(56); }

NAKED void isr_int57_entry() { IMPL_ENTRY_INT(57); }

NAKED void isr_int58_entry() { IMPL_ENTRY_INT(58); }

NAKED void isr_int59_entry() { IMPL_ENTRY_INT(59); }

NAKED void isr_int60_entry() { IMPL_ENTRY_INT(60); }

NAKED void isr_int61_entry() { IMPL_ENTRY_INT(61); }

NAKED void isr_int62_entry() { IMPL_ENTRY_INT(62); }

NAKED void isr_int63_entry() { IMPL_ENTRY_INT(63); }

NAKED void isr_int64_entry() { IMPL_ENTRY_INT(64); }

NAKED void isr_int65_entry() { IMPL_ENTRY_INT(65); }

NAKED void isr_int66_entry() { IMPL_ENTRY_INT(66); }

NAKED void isr_int67_entry() { IMPL_ENTRY_INT(67); }

NAKED void isr_int68_entry() { IMPL_ENTRY_INT(68); }

NAKED void isr_int69_entry() { IMPL_ENTRY_INT(69); }

NAKED void isr_int70_entry() { IMPL_ENTRY_INT(70); }

NAKED void isr_int71_entry() { IMPL_ENTRY_INT(71); }

NAKED void isr_int72_entry() { IMPL_ENTRY_INT(72); }

NAKED void isr_int73_entry() { IMPL_ENTRY_INT(73); }

NAKED void isr_int74_entry() { IMPL_ENTRY_INT(74); }

NAKED void isr_int75_entry() { IMPL_ENTRY_INT(75); }

NAKED void isr_int76_entry() { IMPL_ENTRY_INT(76); }

NAKED void isr_int77_entry() { IMPL_ENTRY_INT(77); }

NAKED void isr_int78_entry() { IMPL_ENTRY_INT(78); }

NAKED void isr_int79_entry() { IMPL_ENTRY_INT(79); }

NAKED void isr_int80_entry() { IMPL_ENTRY_INT(80); }

NAKED void isr_int81_entry() { IMPL_ENTRY_INT(81); }

NAKED void isr_int82_entry() { IMPL_ENTRY_INT(82); }

NAKED void isr_int83_entry() { IMPL_ENTRY_INT(83); }

NAKED void isr_int84_entry() { IMPL_ENTRY_INT(84); }

NAKED void isr_int85_entry() { IMPL_ENTRY_INT(85); }

NAKED void isr_int86_entry() { IMPL_ENTRY_INT(86); }

NAKED void isr_int87_entry() { IMPL_ENTRY_INT(87); }

NAKED void isr_int88_entry() { IMPL_ENTRY_INT(88); }

NAKED void isr_int89_entry() { IMPL_ENTRY_INT(89); }

NAKED void isr_int90_entry() { IMPL_ENTRY_INT(90); }

NAKED void isr_int91_entry() { IMPL_ENTRY_INT(91); }

NAKED void isr_int92_entry() { IMPL_ENTRY_INT(92); }

NAKED void isr_int93_entry() { IMPL_ENTRY_INT(93); }

NAKED void isr_int94_entry() { IMPL_ENTRY_INT(94); }

NAKED void isr_int95_entry() { IMPL_ENTRY_INT(95); }

NAKED void isr_int96_entry() { IMPL_ENTRY_INT(96); }

NAKED void isr_int97_entry() { IMPL_ENTRY_INT(97); }

NAKED void isr_int98_entry() { IMPL_ENTRY_INT(98); }

NAKED void isr_int99_entry() { IMPL_ENTRY_INT(99); }

NAKED void isr_int100_entry() { IMPL_ENTRY_INT(100); }

NAKED void isr_int101_entry() { IMPL_ENTRY_INT(101); }

NAKED void isr_int102_entry() { IMPL_ENTRY_INT(102); }

NAKED void isr_int103_entry() { IMPL_ENTRY_INT(103); }

NAKED void isr_int104_entry() { IMPL_ENTRY_INT(104); }

NAKED void isr_int105_entry() { IMPL_ENTRY_INT(105); }

NAKED void isr_int106_entry() { IMPL_ENTRY_INT(106); }

NAKED void isr_int107_entry() { IMPL_ENTRY_INT(107); }

NAKED void isr_int108_entry() { IMPL_ENTRY_INT(108); }

NAKED void isr_int109_entry() { IMPL_ENTRY_INT(109); }

NAKED void isr_int110_entry() { IMPL_ENTRY_INT(110); }

NAKED void isr_int111_entry() { IMPL_ENTRY_INT(111); }

NAKED void isr_int112_entry() { IMPL_ENTRY_INT(112); }

NAKED void isr_int113_entry() { IMPL_ENTRY_INT(113); }

NAKED void isr_int114_entry() { IMPL_ENTRY_INT(114); }

NAKED void isr_int115_entry() { IMPL_ENTRY_INT(115); }

NAKED void isr_int116_entry() { IMPL_ENTRY_INT(116); }

NAKED void isr_int117_entry() { IMPL_ENTRY_INT(117); }

NAKED void isr_int118_entry() { IMPL_ENTRY_INT(118); }

NAKED void isr_int119_entry() { IMPL_ENTRY_INT(119); }

NAKED void isr_int120_entry() { IMPL_ENTRY_INT(120); }

NAKED void isr_int121_entry() { IMPL_ENTRY_INT(121); }

NAKED void isr_int122_entry() { IMPL_ENTRY_INT(122); }

NAKED void isr_int123_entry() { IMPL_ENTRY_INT(123); }

NAKED void isr_int124_entry() { IMPL_ENTRY_INT(124); }

NAKED void isr_int125_entry() { IMPL_ENTRY_INT(125); }

NAKED void isr_int126_entry() { IMPL_ENTRY_INT(126); }

NAKED void isr_int127_entry() { IMPL_ENTRY_INT(127); }

NAKED void isr_int128_entry() { IMPL_ENTRY_INT(128); }

NAKED void isr_int129_entry() { IMPL_ENTRY_INT(129); }

NAKED void isr_int130_entry() { IMPL_ENTRY_INT(130); }

NAKED void isr_int131_entry() { IMPL_ENTRY_INT(131); }

NAKED void isr_int132_entry() { IMPL_ENTRY_INT(132); }

NAKED void isr_int133_entry() { IMPL_ENTRY_INT(133); }

NAKED void isr_int134_entry() { IMPL_ENTRY_INT(134); }

NAKED void isr_int135_entry() { IMPL_ENTRY_INT(135); }

NAKED void isr_int136_entry() { IMPL_ENTRY_INT(136); }

NAKED void isr_int137_entry() { IMPL_ENTRY_INT(137); }

NAKED void isr_int138_entry() { IMPL_ENTRY_INT(138); }

NAKED void isr_int139_entry() { IMPL_ENTRY_INT(139); }

NAKED void isr_int140_entry() { IMPL_ENTRY_INT(140); }

NAKED void isr_int141_entry() { IMPL_ENTRY_INT(141); }

NAKED void isr_int142_entry() { IMPL_ENTRY_INT(142); }

NAKED void isr_int143_entry() { IMPL_ENTRY_INT(143); }

NAKED void isr_int144_entry() { IMPL_ENTRY_INT(144); }

NAKED void isr_int145_entry() { IMPL_ENTRY_INT(145); }

NAKED void isr_int146_entry() { IMPL_ENTRY_INT(146); }

NAKED void isr_int147_entry() { IMPL_ENTRY_INT(147); }

NAKED void isr_int148_entry() { IMPL_ENTRY_INT(148); }

NAKED void isr_int149_entry() { IMPL_ENTRY_INT(149); }

NAKED void isr_int150_entry() { IMPL_ENTRY_INT(150); }

NAKED void isr_int151_entry() { IMPL_ENTRY_INT(151); }

NAKED void isr_int152_entry() { IMPL_ENTRY_INT(152); }

NAKED void isr_int153_entry() { IMPL_ENTRY_INT(153); }

NAKED void isr_int154_entry() { IMPL_ENTRY_INT(154); }

NAKED void isr_int155_entry() { IMPL_ENTRY_INT(155); }

NAKED void isr_int156_entry() { IMPL_ENTRY_INT(156); }

NAKED void isr_int157_entry() { IMPL_ENTRY_INT(157); }

NAKED void isr_int158_entry() { IMPL_ENTRY_INT(158); }

NAKED void isr_int159_entry() { IMPL_ENTRY_INT(159); }

NAKED void isr_int160_entry() { IMPL_ENTRY_INT(160); }

NAKED void isr_int161_entry() { IMPL_ENTRY_INT(161); }

NAKED void isr_int162_entry() { IMPL_ENTRY_INT(162); }

NAKED void isr_int163_entry() { IMPL_ENTRY_INT(163); }

NAKED void isr_int164_entry() { IMPL_ENTRY_INT(164); }

NAKED void isr_int165_entry() { IMPL_ENTRY_INT(165); }

NAKED void isr_int166_entry() { IMPL_ENTRY_INT(166); }

NAKED void isr_int167_entry() { IMPL_ENTRY_INT(167); }

NAKED void isr_int168_entry() { IMPL_ENTRY_INT(168); }

NAKED void isr_int169_entry() { IMPL_ENTRY_INT(169); }

NAKED void isr_int170_entry() { IMPL_ENTRY_INT(170); }

NAKED void isr_int171_entry() { IMPL_ENTRY_INT(171); }

NAKED void isr_int172_entry() { IMPL_ENTRY_INT(172); }

NAKED void isr_int173_entry() { IMPL_ENTRY_INT(173); }

NAKED void isr_int174_entry() { IMPL_ENTRY_INT(174); }

NAKED void isr_int175_entry() { IMPL_ENTRY_INT(175); }

NAKED void isr_int176_entry() { IMPL_ENTRY_INT(176); }

NAKED void isr_int177_entry() { IMPL_ENTRY_INT(177); }

NAKED void isr_int178_entry() { IMPL_ENTRY_INT(178); }

NAKED void isr_int179_entry() { IMPL_ENTRY_INT(179); }

NAKED void isr_int180_entry() { IMPL_ENTRY_INT(180); }

NAKED void isr_int181_entry() { IMPL_ENTRY_INT(181); }

NAKED void isr_int182_entry() { IMPL_ENTRY_INT(182); }

NAKED void isr_int183_entry() { IMPL_ENTRY_INT(183); }

NAKED void isr_int184_entry() { IMPL_ENTRY_INT(184); }

NAKED void isr_int185_entry() { IMPL_ENTRY_INT(185); }

NAKED void isr_int186_entry() { IMPL_ENTRY_INT(186); }

NAKED void isr_int187_entry() { IMPL_ENTRY_INT(187); }

NAKED void isr_int188_entry() { IMPL_ENTRY_INT(188); }

NAKED void isr_int189_entry() { IMPL_ENTRY_INT(189); }

NAKED void isr_int190_entry() { IMPL_ENTRY_INT(190); }

NAKED void isr_int191_entry() { IMPL_ENTRY_INT(191); }

NAKED void isr_int192_entry() { IMPL_ENTRY_INT(192); }

NAKED void isr_int193_entry() { IMPL_ENTRY_INT(193); }

NAKED void isr_int194_entry() { IMPL_ENTRY_INT(194); }

NAKED void isr_int195_entry() { IMPL_ENTRY_INT(195); }

NAKED void isr_int196_entry() { IMPL_ENTRY_INT(196); }

NAKED void isr_int197_entry() { IMPL_ENTRY_INT(197); }

NAKED void isr_int198_entry() { IMPL_ENTRY_INT(198); }

NAKED void isr_int199_entry() { IMPL_ENTRY_INT(199); }

NAKED void isr_int200_entry() { IMPL_ENTRY_INT(200); }

NAKED void isr_int201_entry() { IMPL_ENTRY_INT(201); }

NAKED void isr_int202_entry() { IMPL_ENTRY_INT(202); }

NAKED void isr_int203_entry() { IMPL_ENTRY_INT(203); }

NAKED void isr_int204_entry() { IMPL_ENTRY_INT(204); }

NAKED void isr_int205_entry() { IMPL_ENTRY_INT(205); }

NAKED void isr_int206_entry() { IMPL_ENTRY_INT(206); }

NAKED void isr_int207_entry() { IMPL_ENTRY_INT(207); }

NAKED void isr_int208_entry() { IMPL_ENTRY_INT(208); }

NAKED void isr_int209_entry() { IMPL_ENTRY_INT(209); }

NAKED void isr_int210_entry() { IMPL_ENTRY_INT(210); }

NAKED void isr_int211_entry() { IMPL_ENTRY_INT(211); }

NAKED void isr_int212_entry() { IMPL_ENTRY_INT(212); }

NAKED void isr_int213_entry() { IMPL_ENTRY_INT(213); }

NAKED void isr_int214_entry() { IMPL_ENTRY_INT(214); }

NAKED void isr_int215_entry() { IMPL_ENTRY_INT(215); }

NAKED void isr_int216_entry() { IMPL_ENTRY_INT(216); }

NAKED void isr_int217_entry() { IMPL_ENTRY_INT(217); }

NAKED void isr_int218_entry() { IMPL_ENTRY_INT(218); }

NAKED void isr_int219_entry() { IMPL_ENTRY_INT(219); }

NAKED void isr_int220_entry() { IMPL_ENTRY_INT(220); }

NAKED void isr_int221_entry() { IMPL_ENTRY_INT(221); }

NAKED void isr_int222_entry() { IMPL_ENTRY_INT(222); }

NAKED void isr_int223_entry() { IMPL_ENTRY_INT(223); }

NAKED void isr_int224_entry() { IMPL_ENTRY_INT(224); }

NAKED void isr_int225_entry() { IMPL_ENTRY_INT(225); }

NAKED void isr_int226_entry() { IMPL_ENTRY_INT(226); }

NAKED void isr_int227_entry() { IMPL_ENTRY_INT(227); }

NAKED void isr_int228_entry() { IMPL_ENTRY_INT(228); }

NAKED void isr_int229_entry() { IMPL_ENTRY_INT(229); }

NAKED void isr_int230_entry() { IMPL_ENTRY_INT(230); }

NAKED void isr_int231_entry() { IMPL_ENTRY_INT(231); }

NAKED void isr_int232_entry() { IMPL_ENTRY_INT(232); }

NAKED void isr_int233_entry() { IMPL_ENTRY_INT(233); }

NAKED void isr_int234_entry() { IMPL_ENTRY_INT(234); }

NAKED void isr_int235_entry() { IMPL_ENTRY_INT(235); }

NAKED void isr_int236_entry() { IMPL_ENTRY_INT(236); }

NAKED void isr_int237_entry() { IMPL_ENTRY_INT(237); }

NAKED void isr_int238_entry() { IMPL_ENTRY_INT(238); }

NAKED void isr_int239_entry() { IMPL_ENTRY_INT(239); }

NAKED void isr_int240_entry() { IMPL_ENTRY_INT(240); }

NAKED void isr_int241_entry() { IMPL_ENTRY_INT(241); }

NAKED void isr_int242_entry() { IMPL_ENTRY_INT(242); }

NAKED void isr_int243_entry() { IMPL_ENTRY_INT(243); }

NAKED void isr_int244_entry() { IMPL_ENTRY_INT(244); }

NAKED void isr_int245_entry() { IMPL_ENTRY_INT(245); }

NAKED void isr_int246_entry() { IMPL_ENTRY_INT(246); }

NAKED void isr_int247_entry() { IMPL_ENTRY_INT(247); }

NAKED void isr_int248_entry() { IMPL_ENTRY_INT(248); }

NAKED void isr_int249_entry() { IMPL_ENTRY_INT(249); }

NAKED void isr_int250_entry() { IMPL_ENTRY_INT(250); }

NAKED void isr_int251_entry() { IMPL_ENTRY_INT(251); }

NAKED void isr_int252_entry() { IMPL_ENTRY_INT(252); }

NAKED void isr_int253_entry() { IMPL_ENTRY_INT(253); }

NAKED void isr_int254_entry() { IMPL_ENTRY_INT(254); }

NAKED void isr_int255_entry() { IMPL_ENTRY_INT(255); }
