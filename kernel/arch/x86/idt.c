// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "_internal.h"
#include "_isr.h"
#include <kernel/utility/utility.h>
#include <stddef.h>
#include <stdint.h>

// #define IDT_TEST

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

static struct GateDescriptor
make_gate_descriptor(void (*entry)(void), uint16_t flags) {
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

static Handler *KERNEL_TRAPS[32] = {
        isr_exc0_entry,  isr_exc1_entry,  isr_exc2_entry,  isr_exc3_entry,
        isr_exc4_entry,  isr_exc5_entry,  isr_exc6_entry,  isr_exc7_entry,
        isr_exc8_entry,  isr_exc9_entry,  isr_exc10_entry, isr_exc11_entry,
        isr_exc12_entry, isr_exc13_entry, isr_exc14_entry, isr_exc15_entry,
        isr_exc16_entry, isr_exc17_entry, isr_exc18_entry, isr_exc19_entry,
        isr_exc20_entry, isr_exc21_entry, isr_exc22_entry, isr_exc23_entry,
        isr_exc24_entry, isr_exc25_entry, isr_exc26_entry, isr_exc27_entry,
        isr_exc28_entry, isr_exc29_entry, isr_exc30_entry, isr_exc31_entry,
};
static Handler *KERNEL_INT_HANDLERS[] = {
        isr_int32_entry,  isr_int33_entry,  isr_int34_entry,  isr_int35_entry,
        isr_int36_entry,  isr_int37_entry,  isr_int38_entry,  isr_int39_entry,
        isr_int40_entry,  isr_int41_entry,  isr_int42_entry,  isr_int43_entry,
        isr_int44_entry,  isr_int45_entry,  isr_int46_entry,  isr_int47_entry,
        isr_int48_entry,  isr_int49_entry,  isr_int50_entry,  isr_int51_entry,
        isr_int52_entry,  isr_int53_entry,  isr_int54_entry,  isr_int55_entry,
        isr_int56_entry,  isr_int57_entry,  isr_int58_entry,  isr_int59_entry,
        isr_int60_entry,  isr_int61_entry,  isr_int62_entry,  isr_int63_entry,
        isr_int64_entry,  isr_int65_entry,  isr_int66_entry,  isr_int67_entry,
        isr_int68_entry,  isr_int69_entry,  isr_int70_entry,  isr_int71_entry,
        isr_int72_entry,  isr_int73_entry,  isr_int74_entry,  isr_int75_entry,
        isr_int76_entry,  isr_int77_entry,  isr_int78_entry,  isr_int79_entry,
        isr_int80_entry,  isr_int81_entry,  isr_int82_entry,  isr_int83_entry,
        isr_int84_entry,  isr_int85_entry,  isr_int86_entry,  isr_int87_entry,
        isr_int88_entry,  isr_int89_entry,  isr_int90_entry,  isr_int91_entry,
        isr_int92_entry,  isr_int93_entry,  isr_int94_entry,  isr_int95_entry,
        isr_int96_entry,  isr_int97_entry,  isr_int98_entry,  isr_int99_entry,
        isr_int100_entry, isr_int101_entry, isr_int102_entry, isr_int103_entry,
        isr_int104_entry, isr_int105_entry, isr_int106_entry, isr_int107_entry,
        isr_int108_entry, isr_int109_entry, isr_int110_entry, isr_int111_entry,
        isr_int112_entry, isr_int113_entry, isr_int114_entry, isr_int115_entry,
        isr_int116_entry, isr_int117_entry, isr_int118_entry, isr_int119_entry,
        isr_int120_entry, isr_int121_entry, isr_int122_entry, isr_int123_entry,
        isr_int124_entry, isr_int125_entry, isr_int126_entry, isr_int127_entry,
        isr_int128_entry, isr_int129_entry, isr_int130_entry, isr_int131_entry,
        isr_int132_entry, isr_int133_entry, isr_int134_entry, isr_int135_entry,
        isr_int136_entry, isr_int137_entry, isr_int138_entry, isr_int139_entry,
        isr_int140_entry, isr_int141_entry, isr_int142_entry, isr_int143_entry,
        isr_int144_entry, isr_int145_entry, isr_int146_entry, isr_int147_entry,
        isr_int148_entry, isr_int149_entry, isr_int150_entry, isr_int151_entry,
        isr_int152_entry, isr_int153_entry, isr_int154_entry, isr_int155_entry,
        isr_int156_entry, isr_int157_entry, isr_int158_entry, isr_int159_entry,
        isr_int160_entry, isr_int161_entry, isr_int162_entry, isr_int163_entry,
        isr_int164_entry, isr_int165_entry, isr_int166_entry, isr_int167_entry,
        isr_int168_entry, isr_int169_entry, isr_int170_entry, isr_int171_entry,
        isr_int172_entry, isr_int173_entry, isr_int174_entry, isr_int175_entry,
        isr_int176_entry, isr_int177_entry, isr_int178_entry, isr_int179_entry,
        isr_int180_entry, isr_int181_entry, isr_int182_entry, isr_int183_entry,
        isr_int184_entry, isr_int185_entry, isr_int186_entry, isr_int187_entry,
        isr_int188_entry, isr_int189_entry, isr_int190_entry, isr_int191_entry,
        isr_int192_entry, isr_int193_entry, isr_int194_entry, isr_int195_entry,
        isr_int196_entry, isr_int197_entry, isr_int198_entry, isr_int199_entry,
        isr_int200_entry, isr_int201_entry, isr_int202_entry, isr_int203_entry,
        isr_int204_entry, isr_int205_entry, isr_int206_entry, isr_int207_entry,
        isr_int208_entry, isr_int209_entry, isr_int210_entry, isr_int211_entry,
        isr_int212_entry, isr_int213_entry, isr_int214_entry, isr_int215_entry,
        isr_int216_entry, isr_int217_entry, isr_int218_entry, isr_int219_entry,
        isr_int220_entry, isr_int221_entry, isr_int222_entry, isr_int223_entry,
        isr_int224_entry, isr_int225_entry, isr_int226_entry, isr_int227_entry,
        isr_int228_entry, isr_int229_entry, isr_int230_entry, isr_int231_entry,
        isr_int232_entry, isr_int233_entry, isr_int234_entry, isr_int235_entry,
        isr_int236_entry, isr_int237_entry, isr_int238_entry, isr_int239_entry,
        isr_int240_entry, isr_int241_entry, isr_int242_entry, isr_int243_entry,
        isr_int244_entry, isr_int245_entry, isr_int246_entry, isr_int247_entry,
        isr_int248_entry, isr_int249_entry, isr_int250_entry, isr_int251_entry,
        isr_int252_entry, isr_int253_entry, isr_int254_entry, isr_int255_entry,
};

static struct GateDescriptor s_idt[256];

#define KERNEL_TRAP_COUNT (sizeof(KERNEL_TRAPS) / sizeof(*KERNEL_TRAPS))
#define KERNEL_INT_HANDLER_COUNT \
        (sizeof(KERNEL_INT_HANDLERS) / sizeof(*KERNEL_INT_HANDLERS))
#define TOTAL_HANDLER_COUNT (sizeof(s_idt) / sizeof(*s_idt))

_Static_assert(
        KERNEL_TRAP_COUNT + KERNEL_INT_HANDLER_COUNT <= TOTAL_HANDLER_COUNT,
        "Too many entrypoints"
);

void idt_init_bsp(void) {
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

void idt_init_ap(void) {
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

void idt_use_ist1(void) {
        for (size_t i = 0; i < TOTAL_HANDLER_COUNT; ++i) {
                s_idt[i].flags &= ~FLAG_NO_IST;
                s_idt[i].flags |= FLAG_IST1;
        }
}
