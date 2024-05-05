// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#pragma once
#include <stdint.h>

namespace Kernel::Isr {

struct TrapFrame;

typedef void(isr_exception_handler_t)(struct TrapFrame *);
typedef void(isr_entry_t)(void);

extern "C" {
        isr_exception_handler_t isr_exc0_handler, isr_exc1_handler, isr_exc2_handler,
                isr_exc3_handler, isr_exc4_handler, isr_exc5_handler, isr_exc6_handler,
                isr_exc7_handler, isr_exc8_handler, isr_exc9_handler, isr_exc10_handler,
                isr_exc11_handler, isr_exc12_handler, isr_exc13_handler,
                isr_exc14_handler, isr_exc15_handler, isr_exc16_handler,
                isr_exc17_handler, isr_exc18_handler, isr_exc19_handler,
                isr_exc20_handler, isr_exc21_handler, isr_exc22_handler,
                isr_exc23_handler, isr_exc24_handler, isr_exc25_handler,
                isr_exc26_handler, isr_exc27_handler, isr_exc28_handler,
                isr_exc29_handler, isr_exc30_handler, isr_exc31_handler;

        void isr_handle_interrupt(struct TrapFrame *frm, uint64_t int_num);

isr_entry_t exc0_entry, exc1_entry, exc2_entry, exc3_entry,
        exc4_entry, exc5_entry, exc6_entry, exc7_entry,
        exc8_entry, exc9_entry, exc10_entry, exc11_entry,
        exc12_entry, exc13_entry, exc14_entry, exc15_entry,
        exc16_entry, exc17_entry, exc18_entry, exc19_entry,
        exc20_entry, exc21_entry, exc22_entry, exc23_entry,
        exc24_entry, exc25_entry, exc26_entry, exc27_entry,
        exc28_entry, exc29_entry, exc30_entry, exc31_entry,
        int32_entry, int33_entry, int34_entry, int35_entry,
        int36_entry, int37_entry, int38_entry, int39_entry,
        int40_entry, int41_entry, int42_entry, int43_entry,
        int44_entry, int45_entry, int46_entry, int47_entry,
        int48_entry, int49_entry, int50_entry, int51_entry,
        int52_entry, int53_entry, int54_entry, int55_entry,
        int56_entry, int57_entry, int58_entry, int59_entry,
        int60_entry, int61_entry, int62_entry, int63_entry,
        int64_entry, int65_entry, int66_entry, int67_entry,
        int68_entry, int69_entry, int70_entry, int71_entry,
        int72_entry, int73_entry, int74_entry, int75_entry,
        int76_entry, int77_entry, int78_entry, int79_entry,
        int80_entry, int81_entry, int82_entry, int83_entry,
        int84_entry, int85_entry, int86_entry, int87_entry,
        int88_entry, int89_entry, int90_entry, int91_entry,
        int92_entry, int93_entry, int94_entry, int95_entry,
        int96_entry, int97_entry, int98_entry, int99_entry,
        int100_entry, int101_entry, int102_entry, int103_entry,
        int104_entry, int105_entry, int106_entry, int107_entry,
        int108_entry, int109_entry, int110_entry, int111_entry,
        int112_entry, int113_entry, int114_entry, int115_entry,
        int116_entry, int117_entry, int118_entry, int119_entry,
        int120_entry, int121_entry, int122_entry, int123_entry,
        int124_entry, int125_entry, int126_entry, int127_entry,
        int128_entry, int129_entry, int130_entry, int131_entry,
        int132_entry, int133_entry, int134_entry, int135_entry,
        int136_entry, int137_entry, int138_entry, int139_entry,
        int140_entry, int141_entry, int142_entry, int143_entry,
        int144_entry, int145_entry, int146_entry, int147_entry,
        int148_entry, int149_entry, int150_entry, int151_entry,
        int152_entry, int153_entry, int154_entry, int155_entry,
        int156_entry, int157_entry, int158_entry, int159_entry,
        int160_entry, int161_entry, int162_entry, int163_entry,
        int164_entry, int165_entry, int166_entry, int167_entry,
        int168_entry, int169_entry, int170_entry, int171_entry,
        int172_entry, int173_entry, int174_entry, int175_entry,
        int176_entry, int177_entry, int178_entry, int179_entry,
        int180_entry, int181_entry, int182_entry, int183_entry,
        int184_entry, int185_entry, int186_entry, int187_entry,
        int188_entry, int189_entry, int190_entry, int191_entry,
        int192_entry, int193_entry, int194_entry, int195_entry,
        int196_entry, int197_entry, int198_entry, int199_entry,
        int200_entry, int201_entry, int202_entry, int203_entry,
        int204_entry, int205_entry, int206_entry, int207_entry,
        int208_entry, int209_entry, int210_entry, int211_entry,
        int212_entry, int213_entry, int214_entry, int215_entry,
        int216_entry, int217_entry, int218_entry, int219_entry,
        int220_entry, int221_entry, int222_entry, int223_entry,
        int224_entry, int225_entry, int226_entry, int227_entry,
        int228_entry, int229_entry, int230_entry, int231_entry,
        int232_entry, int233_entry, int234_entry, int235_entry,
        int236_entry, int237_entry, int238_entry, int239_entry,
        int240_entry, int241_entry, int242_entry, int243_entry,
        int244_entry, int245_entry, int246_entry, int247_entry,
        int248_entry, int249_entry, int250_entry, int251_entry,
        int252_entry, int253_entry, int254_entry, int255_entry;
}

}