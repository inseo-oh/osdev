// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#pragma once
#error Use utility.h instead
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint64_t bitmap_word_t;
typedef uint64_t bitmap_bit_index_t;

#define BITMAP_BIT_INDEX_INVALID ((bitmap_bit_index_t)~0)
#define BITMAP_BITS_PER_WORD     (sizeof(bitmap_word_t) * 8)

static inline size_t bitmap_needed_word_count(size_t bit_cnt) {
        return (bit_cnt % BITMAP_BITS_PER_WORD)
                       ? ((bit_cnt / BITMAP_BITS_PER_WORD) + 1UL)
                       : bit_cnt / BITMAP_BITS_PER_WORD;
}

static inline void bitmap_set(bitmap_word_t *bitmap, size_t idx) {
        bitmap[idx / BITMAP_BITS_PER_WORD] |=
                ((bitmap_word_t)1 << (idx % BITMAP_BITS_PER_WORD));
}

static inline void bitmap_clear(bitmap_word_t *bitmap, size_t idx) {
        bitmap[idx / BITMAP_BITS_PER_WORD] &=
                ~((bitmap_word_t)1 << (idx % BITMAP_BITS_PER_WORD));
}

static inline bool bitmap_is_set(bitmap_word_t const *bitmap, size_t idx) {
        return bitmap[idx / BITMAP_BITS_PER_WORD] &
               ((bitmap_word_t)1 << (idx % BITMAP_BITS_PER_WORD));
}

// Returns BITMAP_BIT_INDEX_INVALID if bitmap is empty.
bitmap_bit_index_t bitmap_find_set_bit(
        bitmap_word_t const *bitmap,
        bitmap_bit_index_t start_pos,
        size_t words_len
);
// Returns BITMAP_BIT_INDEX_INVALID if an active bit doesn't exist.
bitmap_bit_index_t bitmap_find_last_continuous_set_bit(
        bitmap_word_t const *bitmap,
        bitmap_bit_index_t start_pos,
        size_t words_len
);
bool bitmap_are_set(
        bitmap_word_t const *bitmap,
        bitmap_bit_index_t offset,
        bitmap_bit_index_t len
);
void bitmap_set_multi(
        bitmap_word_t *bitmap, bitmap_bit_index_t offset, bitmap_bit_index_t len
);
void bitmap_clear_multi(
        bitmap_word_t *bitmap, bitmap_bit_index_t offset, bitmap_bit_index_t len
);
bitmap_bit_index_t bitmap_find_set_bits(
        bitmap_word_t const *bitmap,
        bitmap_bit_index_t start_pos,
        bitmap_bit_index_t len,
        size_t words_len
);
