// SPDX-FileCopyrightText: 2023, Inseo Oh (dhdlstjtr@gmail.com)
//
// SPDX-License-Identifier: BSD-2-Clause
#include "kernel.h"
#include <kernel/arch/arch.h>
#include <kernel/lock/spinlock.h>
#include <kernel/tasks/tasks.h>
#include <kernel/utility/utility.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FONT_WIDTH  8
#define FONT_HEIGHT 16

// Fonts are series of 8-bit x 16-line glyphs, starting from ASCII
// character 32.
static uint8_t *s_font;
static uint32_t *s_video_buf;
static uint32_t *s_doublebuf_buf;
static uint32_t *s_active_buf;
static size_t s_buf_size;
// Colors are stored as native color for the framebuffer.
static uint32_t s_bg_encoded_color;
static uint32_t s_fg_encoded_color;
static uint16_t s_width, s_height;
static uint16_t s_pixels_per_line;
static uint16_t s_next_cursor_x;
static struct SpinLock s_lock;

static void copy_active_to_video_buf(void) {
        if (s_active_buf == s_video_buf) {
                return;
        }
        kmemcpy(s_video_buf, s_active_buf, s_buf_size);
}

static void draw_pixel(unsigned xpos, unsigned ypos, uint32_t encoded_color) {
        s_active_buf[ypos * s_pixels_per_line + xpos] = encoded_color;
}

static void fill_rect(unsigned ypos, unsigned height, uint32_t encoded_color) {
        unsigned bottom = ypos + height;
        for (; ypos < bottom; ++ypos) {
                uint32_t *dest =
                        &s_active_buf[(uintptr_t)ypos * s_pixels_per_line];
                for (int xpos = 0; xpos < s_width; ++xpos, ++dest) {
                        *dest = encoded_color;
                }
        }
}

static void scroll_lines(void) {
        size_t ppl = s_pixels_per_line;
        size_t copy_byte_count = ppl * sizeof(*s_active_buf);
        uint32_t *dest = &s_active_buf[0];
        uint32_t *src = &s_active_buf[(uintptr_t)FONT_HEIGHT * ppl];
        for (unsigned i = FONT_HEIGHT; i < s_height; ++i) {
                kmemcpy(dest, src, copy_byte_count);
                dest += ppl;
                src += ppl;
        }
        fill_rect(s_height - FONT_HEIGHT, FONT_HEIGHT, s_bg_encoded_color);
        s_next_cursor_x = 0;
}

static void draw_next_char(char chr) {
        if ((chr == '\n') || (chr == '\r')) {
                s_next_cursor_x = 0;
                if (chr == '\n') {
                        scroll_lines();
                }
                return;
        }
        if (s_width < (s_next_cursor_x + FONT_WIDTH)) {
                scroll_lines();
        }
        if ((chr < ' ') || ('~' < chr)) {
                chr = '?';
        }
        uint8_t const *next_line =
                &s_font[(uintptr_t)(chr - ' ') * FONT_HEIGHT];
        unsigned left = s_next_cursor_x;
        unsigned pen_y = s_height - FONT_HEIGHT;
        for (unsigned line_index = 0; line_index < FONT_HEIGHT;
             ++line_index, pen_y += 1, ++next_line) {
                unsigned pen_x = left;
                uint8_t const pixels = *next_line;
                uint8_t mask = 1 << 7;
                for (unsigned pixel_index = 0; pixel_index < 8;
                     ++pixel_index, pen_x += 1, mask >>= 1) {
                        bool active = !!(pixels & mask);
                        if (!active) {
                                continue;
                        }
                        draw_pixel(pen_x, pen_y, s_fg_encoded_color);
                }
        }
        s_next_cursor_x += FONT_WIDTH;
}

struct ParsedBitMask {
        uint32_t input_value_mask;
        uint8_t left_shift_count;
};

static struct ParsedBitMask parse_bitmask(uint32_t bitmask) {
        int left_shift_count = -1;
        uint32_t input_value_mask = 0;
        uint32_t mask = 1;
        for (unsigned i = 0; i < 32; ++i, mask <<= 1) {
                if ((bitmask & mask)) {
                        if (left_shift_count < 0) {
                                left_shift_count = (int)i;
                        }
                        input_value_mask <<= 1;
                        input_value_mask |= 1;
                } else {
                        if (0 < left_shift_count) {
                                break;
                        }
                }
        }
        // NOTE: left_shift_count can never be -1 unless `bitmask` is 0.
        ASSERT(left_shift_count != -1);
        return (struct ParsedBitMask
        ){.left_shift_count = left_shift_count,
          .input_value_mask = input_value_mask};
}

static uint32_t encode_color(
        struct ParsedBitMask const *red_mask,
        struct ParsedBitMask const *green_mask,
        struct ParsedBitMask const *blue_mask,
        uint8_t red,
        uint8_t green,
        uint8_t blue
) {
        uint32_t encoded_color = (((uint32_t)red) & red_mask->input_value_mask)
                                 << red_mask->left_shift_count;
        encoded_color |= (((uint32_t)green) & green_mask->input_value_mask)
                         << green_mask->left_shift_count;
        encoded_color |= (((uint32_t)blue) & blue_mask->input_value_mask)
                         << blue_mask->left_shift_count;
        return encoded_color;
}

static void put_char(struct Console_Driver *driver, char chr) {
        (void)driver;
        bool prev_interrupt_state;
        spinlock_lock(&s_lock, &prev_interrupt_state);
        draw_next_char(chr);
        spinlock_unlock(&s_lock, prev_interrupt_state);
}

static int get_char(struct Console_Driver *driver) {
        (void)driver;
        return -1;
}

static void flush(struct Console_Driver *driver) {
        (void)driver;
        bool prev_interrupt_state;
        spinlock_lock(&s_lock, &prev_interrupt_state);
        copy_active_to_video_buf();
        spinlock_unlock(&s_lock, prev_interrupt_state);
}

void videoconsole_init(
        void *buf,
        unsigned width,
        unsigned height,
        unsigned pixels_per_line,
        uint32_t red_bitmask,
        uint32_t green_bitmask,
        uint32_t blue_bitmask,
        uint8_t *font,
        bool vm_available
) {
        static struct Console_Driver console_driver = {
                .put_char_fn = put_char,
                .get_char_fn = get_char,
                .flush_fn = flush,
        };

        s_font = font;
        s_video_buf = buf;
        s_width = width;
        s_height = height;
        s_pixels_per_line = pixels_per_line;
        struct ParsedBitMask red_mask = parse_bitmask(red_bitmask);
        struct ParsedBitMask green_mask = parse_bitmask(green_bitmask);
        struct ParsedBitMask blue_mask = parse_bitmask(blue_bitmask);
        s_next_cursor_x = 0;
        s_bg_encoded_color =
                encode_color(&red_mask, &green_mask, &blue_mask, 24, 24, 32);
        s_fg_encoded_color =
                encode_color(&red_mask, &green_mask, &blue_mask, 252, 220, 236);
        s_buf_size =
                (size_t)pixels_per_line * (size_t)height * sizeof(*s_video_buf);
        s_active_buf = s_video_buf;
        if (vm_available) {
                uintptr_t paddr_unused;
                s_doublebuf_buf = process_alloc_pages(
                        process_running(),
                        &paddr_unused,
                        to_block_count(PAGE_SIZE, s_buf_size),
                        (struct Proc_MapOptions){
                                .writable = true,
                                .executable = false,
                        }
                );
                if (!s_doublebuf_buf) {
                        console_printf("Not enough memory to initialize "
                                       "double-buffered video console\n");
                } else {
                        s_active_buf = s_doublebuf_buf;
                }
        }
        fill_rect(0, s_height, s_bg_encoded_color);
        console_register_driver(&console_driver);

        console_printf(
                "Video console: %ux%u, ppl %u, bitmask: %x/%x/%x\n",
                width,
                height,
                pixels_per_line,
                red_bitmask,
                green_bitmask,
                blue_bitmask
        );
}
