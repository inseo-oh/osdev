// SPDX-FileCopyrightText: (c) 2024 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "Idt.h"
#include "Isr.h"
extern "C" {
#define NORETURN_WORKAROUND
#include "_internal.h"
#include "kernel/utility/utility.h"
#include <stddef.h>
#include <stdint.h>
}

namespace Kernel::Idt {

// #define IDT_TEST

namespace {
        struct GateDescriptor {
                uint16_t offset_b15_to_b0;
                uint16_t segment_selector;
                uint16_t flags;
                uint16_t offset_b31_to_b16;
                uint32_t offset_b63_to_b32;
                uint32_t reserved;
        };

        #define FLAG_NO_IST      (0 << 0)   // No IST
        #define FLAG_IST1        (1 << 0)   // Use IST1
        #define FLAG_IST2        (2 << 0)   // Use IST2
        #define FLAG_IST3        (3 << 0)   // Use IST3
        #define FLAG_IST4        (4 << 0)   // Use IST4
        #define FLAG_IST5        (5 << 0)   // Use IST5
        #define FLAG_IST6        (6 << 0)   // Use IST6
        #define FLAG_IST7        (7 << 0)   // Use IST7
        #define FLAG_TYPE_INT64  (0xE << 8) // Interrupt gate
        #define FLAG_TYPE_TRAP64 (0xF << 8) // Trap gate
        #define FLAG_DPL0        (0 << 13)  // DPL 0
        #define FLAG_DPL1        (1 << 13)  // DPL 1
        #define FLAG_DPL2        (2 << 13)  // DPL 2
        #define FLAG_DPL3        (3 << 13)  // DPL 3
        #define FLAG_P           (1 << 15)  // Present?

        struct GateDescriptor make_gate_descriptor(void (*entry)(void), uint16_t flags) {
                struct GateDescriptor out;
                uintptr_t offset = (uintptr_t)entry;
                kmemset(&out, 0, sizeof(out));
                out.offset_b15_to_b0 = offset;
                out.segment_selector = GDT_KERNEL_CS;
                out.flags = flags | FLAG_P;
                out.offset_b31_to_b16 = offset >> 16;
                out.offset_b63_to_b32 = offset >> 32;
                return out;
        }

        struct Idtr {
                uint16_t size;
                uint64_t offset;
        } PACKED;

        typedef void(Handler)();

        Handler *KERNEL_TRAPS[32] = {
                Isr::exc0_entry,  Isr::exc1_entry,  Isr::exc2_entry,  Isr::exc3_entry,
                Isr::exc4_entry,  Isr::exc5_entry,  Isr::exc6_entry,  Isr::exc7_entry,
                Isr::exc8_entry,  Isr::exc9_entry,  Isr::exc10_entry, Isr::exc11_entry,
                Isr::exc12_entry, Isr::exc13_entry, Isr::exc14_entry, Isr::exc15_entry,
                Isr::exc16_entry, Isr::exc17_entry, Isr::exc18_entry, Isr::exc19_entry,
                Isr::exc20_entry, Isr::exc21_entry, Isr::exc22_entry, Isr::exc23_entry,
                Isr::exc24_entry, Isr::exc25_entry, Isr::exc26_entry, Isr::exc27_entry,
                Isr::exc28_entry, Isr::exc29_entry, Isr::exc30_entry, Isr::exc31_entry,
        };
        Handler *KERNEL_INT_HANDLERS[] = {
                Isr::int32_entry,  Isr::int33_entry,  Isr::int34_entry,  Isr::int35_entry,
                Isr::int36_entry,  Isr::int37_entry,  Isr::int38_entry,  Isr::int39_entry,
                Isr::int40_entry,  Isr::int41_entry,  Isr::int42_entry,  Isr::int43_entry,
                Isr::int44_entry,  Isr::int45_entry,  Isr::int46_entry,  Isr::int47_entry,
                Isr::int48_entry,  Isr::int49_entry,  Isr::int50_entry,  Isr::int51_entry,
                Isr::int52_entry,  Isr::int53_entry,  Isr::int54_entry,  Isr::int55_entry,
                Isr::int56_entry,  Isr::int57_entry,  Isr::int58_entry,  Isr::int59_entry,
                Isr::int60_entry,  Isr::int61_entry,  Isr::int62_entry,  Isr::int63_entry,
                Isr::int64_entry,  Isr::int65_entry,  Isr::int66_entry,  Isr::int67_entry,
                Isr::int68_entry,  Isr::int69_entry,  Isr::int70_entry,  Isr::int71_entry,
                Isr::int72_entry,  Isr::int73_entry,  Isr::int74_entry,  Isr::int75_entry,
                Isr::int76_entry,  Isr::int77_entry,  Isr::int78_entry,  Isr::int79_entry,
                Isr::int80_entry,  Isr::int81_entry,  Isr::int82_entry,  Isr::int83_entry,
                Isr::int84_entry,  Isr::int85_entry,  Isr::int86_entry,  Isr::int87_entry,
                Isr::int88_entry,  Isr::int89_entry,  Isr::int90_entry,  Isr::int91_entry,
                Isr::int92_entry,  Isr::int93_entry,  Isr::int94_entry,  Isr::int95_entry,
                Isr::int96_entry,  Isr::int97_entry,  Isr::int98_entry,  Isr::int99_entry,
                Isr::int100_entry, Isr::int101_entry, Isr::int102_entry, Isr::int103_entry,
                Isr::int104_entry, Isr::int105_entry, Isr::int106_entry, Isr::int107_entry,
                Isr::int108_entry, Isr::int109_entry, Isr::int110_entry, Isr::int111_entry,
                Isr::int112_entry, Isr::int113_entry, Isr::int114_entry, Isr::int115_entry,
                Isr::int116_entry, Isr::int117_entry, Isr::int118_entry, Isr::int119_entry,
                Isr::int120_entry, Isr::int121_entry, Isr::int122_entry, Isr::int123_entry,
                Isr::int124_entry, Isr::int125_entry, Isr::int126_entry, Isr::int127_entry,
                Isr::int128_entry, Isr::int129_entry, Isr::int130_entry, Isr::int131_entry,
                Isr::int132_entry, Isr::int133_entry, Isr::int134_entry, Isr::int135_entry,
                Isr::int136_entry, Isr::int137_entry, Isr::int138_entry, Isr::int139_entry,
                Isr::int140_entry, Isr::int141_entry, Isr::int142_entry, Isr::int143_entry,
                Isr::int144_entry, Isr::int145_entry, Isr::int146_entry, Isr::int147_entry,
                Isr::int148_entry, Isr::int149_entry, Isr::int150_entry, Isr::int151_entry,
                Isr::int152_entry, Isr::int153_entry, Isr::int154_entry, Isr::int155_entry,
                Isr::int156_entry, Isr::int157_entry, Isr::int158_entry, Isr::int159_entry,
                Isr::int160_entry, Isr::int161_entry, Isr::int162_entry, Isr::int163_entry,
                Isr::int164_entry, Isr::int165_entry, Isr::int166_entry, Isr::int167_entry,
                Isr::int168_entry, Isr::int169_entry, Isr::int170_entry, Isr::int171_entry,
                Isr::int172_entry, Isr::int173_entry, Isr::int174_entry, Isr::int175_entry,
                Isr::int176_entry, Isr::int177_entry, Isr::int178_entry, Isr::int179_entry,
                Isr::int180_entry, Isr::int181_entry, Isr::int182_entry, Isr::int183_entry,
                Isr::int184_entry, Isr::int185_entry, Isr::int186_entry, Isr::int187_entry,
                Isr::int188_entry, Isr::int189_entry, Isr::int190_entry, Isr::int191_entry,
                Isr::int192_entry, Isr::int193_entry, Isr::int194_entry, Isr::int195_entry,
                Isr::int196_entry, Isr::int197_entry, Isr::int198_entry, Isr::int199_entry,
                Isr::int200_entry, Isr::int201_entry, Isr::int202_entry, Isr::int203_entry,
                Isr::int204_entry, Isr::int205_entry, Isr::int206_entry, Isr::int207_entry,
                Isr::int208_entry, Isr::int209_entry, Isr::int210_entry, Isr::int211_entry,
                Isr::int212_entry, Isr::int213_entry, Isr::int214_entry, Isr::int215_entry,
                Isr::int216_entry, Isr::int217_entry, Isr::int218_entry, Isr::int219_entry,
                Isr::int220_entry, Isr::int221_entry, Isr::int222_entry, Isr::int223_entry,
                Isr::int224_entry, Isr::int225_entry, Isr::int226_entry, Isr::int227_entry,
                Isr::int228_entry, Isr::int229_entry, Isr::int230_entry, Isr::int231_entry,
                Isr::int232_entry, Isr::int233_entry, Isr::int234_entry, Isr::int235_entry,
                Isr::int236_entry, Isr::int237_entry, Isr::int238_entry, Isr::int239_entry,
                Isr::int240_entry, Isr::int241_entry, Isr::int242_entry, Isr::int243_entry,
                Isr::int244_entry, Isr::int245_entry, Isr::int246_entry, Isr::int247_entry,
                Isr::int248_entry, Isr::int249_entry, Isr::int250_entry, Isr::int251_entry,
                Isr::int252_entry, Isr::int253_entry, Isr::int254_entry, Isr::int255_entry,
        };

        struct GateDescriptor s_idt[256];
}

#define KERNEL_TRAP_COUNT (sizeof(KERNEL_TRAPS) / sizeof(*KERNEL_TRAPS))
#define KERNEL_INT_HANDLER_COUNT \
        (sizeof(KERNEL_INT_HANDLERS) / sizeof(*KERNEL_INT_HANDLERS))
#define TOTAL_HANDLER_COUNT (sizeof(s_idt) / sizeof(*s_idt))

static_assert(
        KERNEL_TRAP_COUNT + KERNEL_INT_HANDLER_COUNT <= TOTAL_HANDLER_COUNT,
        "Too many entrypoints"
);

void init_bsp(void) {
        kmemset(s_idt, 0, sizeof(s_idt));

        // Note that IST is disabled right now, until idt_use_ist1() is
        // called. We don't have IST stack ready yet.
        // If an exception occurs before IST is ready, it will just use existing
        // kernel stack.
        for (size_t i = 0; i < KERNEL_TRAP_COUNT; ++i) {
                s_idt[i] = make_gate_descriptor(
                        KERNEL_TRAPS[i],
                        FLAG_NO_IST | FLAG_TYPE_TRAP64 | FLAG_DPL0
                );
        }
        for (size_t i = 0; i < KERNEL_INT_HANDLER_COUNT; ++i) {
                unsigned vector = 32 + i;
                s_idt[vector] = make_gate_descriptor(
                        KERNEL_INT_HANDLERS[i],
                        FLAG_NO_IST | FLAG_TYPE_INT64 | FLAG_DPL0
                );
        }

        // Load IDT
        volatile struct Idtr idtr = {
                .size = sizeof(s_idt), .offset = (uintptr_t)s_idt
        };
        __asm__ volatile("lidt [%0]" ::"r"(&idtr));

#ifdef IDT_TEST
        console_printf("Testing IDT by triggering DIVIDE BY ZERO\n");
        __asm__ volatile("mov rax, 0\n"
                         "idiv rax\n");
#endif
}

void init_ap(void) {
        // Load IDT
        volatile struct Idtr idtr = {
                .size = sizeof(s_idt), .offset = (uintptr_t)s_idt
        };
        __asm__ volatile("lidt [%0]" ::"r"(&idtr));

#ifdef IDT_TEST
        console_printf("Testing IDT by triggering DIVIDE BY ZERO\n");
        __asm__ volatile("mov rax, 0\n"
                         "idiv rax\n");
#endif
}

void use_ist1(void) {
        for (size_t i = 0; i < TOTAL_HANDLER_COUNT; ++i) {
                s_idt[i].flags &= ~FLAG_NO_IST;
                s_idt[i].flags |= FLAG_IST1;
        }
}

}