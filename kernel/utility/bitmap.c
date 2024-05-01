// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "utility.h"
#include "kernel/kernel.h"
#include <stdbool.h>
#include <stddef.h>

static bitmap_word_t make_bitmask(unsigned offset, unsigned len) {
        return (((bitmap_word_t)~0) >> (BITMAP_BITS_PER_WORD - len)) << offset;
}

// Returns BITMAP_BIT_INDEX_INVALID if bitmap is empty.
static bitmap_bit_index_t
word_first_active(bitmap_word_t word, bitmap_bit_index_t start_pos) {
        ASSERT(start_pos != BITMAP_BIT_INDEX_INVALID);
        if (!word || !(word >> start_pos)) {
                return BITMAP_BIT_INDEX_INVALID;
        }
        unsigned index = start_pos;
        word >>= start_pos;
        for (; !(word & 1); word >>= 1, ++index) {}
        return index;
}

// Returns BITMAP_BIT_INDEX_INVALID if an active bit doesn't exist.
static bitmap_bit_index_t word_find_last_continuous_set_bit(
        bitmap_word_t word, bitmap_bit_index_t start_pos
) {
        ASSERT(start_pos != BITMAP_BIT_INDEX_INVALID);
        if (!word || !(word >> start_pos)) {
                return BITMAP_BIT_INDEX_INVALID;
        }
        unsigned index = start_pos;
        word >>= start_pos;
        for (; (word & 1); word >>= 1, ++index) {}
        if (index == start_pos) {
                return BITMAP_BIT_INDEX_INVALID;
        }
        return index - 1;
}

// Returns BITMAP_BIT_INDEX_INVALID if bitmap is empty.
bitmap_bit_index_t bitmap_find_set_bit(
        bitmap_word_t const *bitmap,
        bitmap_bit_index_t start_pos,
        size_t words_len
) {
        ASSERT(start_pos != BITMAP_BIT_INDEX_INVALID);
        bitmap_bit_index_t word_bit_min_index =
                start_pos % BITMAP_BITS_PER_WORD;
        for (size_t word_index = start_pos / BITMAP_BITS_PER_WORD;
             word_index < words_len;
             ++word_index, word_bit_min_index = 0) {
                bitmap_bit_index_t bit_index = word_first_active(
                        bitmap[word_index], word_bit_min_index
                );
                if (bit_index != BITMAP_BIT_INDEX_INVALID) {
                        return (word_index * BITMAP_BITS_PER_WORD) + bit_index;
                }
        }
        return BITMAP_BIT_INDEX_INVALID;
}

// Returns BITMAP_BIT_INDEX_INVALID if an active bit doesn't exist.
bitmap_bit_index_t bitmap_find_last_continuous_set_bit(
        bitmap_word_t const *bitmap,
        bitmap_bit_index_t start_pos,
        size_t words_len
) {
        ASSERT(start_pos != BITMAP_BIT_INDEX_INVALID);
        bitmap_bit_index_t word_bit_min_index =
                start_pos % BITMAP_BITS_PER_WORD;
        bitmap_bit_index_t last_bit_index = BITMAP_BIT_INDEX_INVALID;
        for (size_t word_index = start_pos / BITMAP_BITS_PER_WORD;
             word_index < words_len;
             ++word_index, word_bit_min_index = 0) {
                bitmap_bit_index_t bit_index =
                        word_find_last_continuous_set_bit(
                                bitmap[word_index], word_bit_min_index
                        );
                if (bit_index == BITMAP_BIT_INDEX_INVALID) {
                        break;
                }
                last_bit_index =
                        (word_index * BITMAP_BITS_PER_WORD) + bit_index;
                if (bit_index != (BITMAP_BITS_PER_WORD - 1)) {
                        break;
                }
        }
        return last_bit_index;
}

bool bitmap_are_set(
        bitmap_word_t const *bitmap,
        bitmap_bit_index_t offset,
        bitmap_bit_index_t len
) {
        ASSERT(offset != BITMAP_BIT_INDEX_INVALID);
        ASSERT(len != BITMAP_BIT_INDEX_INVALID);
        if (!len) {
                return true;
        }
        while (len) {
                bitmap_word_t word = bitmap[offset / BITMAP_BITS_PER_WORD];
                if (word == 0) {
                        return false;
                }
                size_t starting_bit = offset % BITMAP_BITS_PER_WORD;
                size_t current_len = len;
                if (BITMAP_BITS_PER_WORD < (starting_bit + current_len)) {
                        current_len = BITMAP_BITS_PER_WORD - starting_bit;
                }
                ASSERT((starting_bit + current_len) <= BITMAP_BITS_PER_WORD);
                bitmap_word_t mask = make_bitmask(starting_bit, len);
                if ((word & mask) != mask) {
                        return false;
                }
                len -= current_len;
                offset += current_len;
        }
        return true;
}

void bitmap_set_multi(
        bitmap_word_t *bitmap, bitmap_bit_index_t offset, bitmap_bit_index_t len
) {
        ASSERT(offset != BITMAP_BIT_INDEX_INVALID);
        ASSERT(len != BITMAP_BIT_INDEX_INVALID);
        if (!len) {
                return;
        }
        while (len) {
                size_t starting_bit = offset % BITMAP_BITS_PER_WORD;
                size_t current_len = len;
                if (BITMAP_BITS_PER_WORD < (starting_bit + current_len)) {
                        current_len = BITMAP_BITS_PER_WORD - starting_bit;
                }
                ASSERT((starting_bit + current_len) <= BITMAP_BITS_PER_WORD);
                bitmap_word_t mask = make_bitmask(starting_bit, current_len);
                bitmap[offset / BITMAP_BITS_PER_WORD] |= mask;
                len -= current_len;
                offset += current_len;
        }
}

void bitmap_clear_multi(
        bitmap_word_t *bitmap, bitmap_bit_index_t offset, bitmap_bit_index_t len
) {
        ASSERT(offset != BITMAP_BIT_INDEX_INVALID);
        ASSERT(len != BITMAP_BIT_INDEX_INVALID);
        if (!len) {
                return;
        }
        while (len) {
                size_t starting_bit = offset % BITMAP_BITS_PER_WORD;
                size_t current_len = len;
                if (BITMAP_BITS_PER_WORD < (starting_bit + current_len)) {
                        current_len = BITMAP_BITS_PER_WORD - starting_bit;
                }
                ASSERT((starting_bit + current_len) <= BITMAP_BITS_PER_WORD);
                bitmap_word_t mask = make_bitmask(starting_bit, current_len);
                bitmap[offset / BITMAP_BITS_PER_WORD] &= ~mask;
                len -= current_len;
                offset += current_len;
        }
}

bitmap_bit_index_t bitmap_find_set_bits(
        bitmap_word_t const *bitmap,
        bitmap_bit_index_t start_pos,
        bitmap_bit_index_t len,
        size_t words_len
) {
        ASSERT(start_pos != BITMAP_BIT_INDEX_INVALID);
        ASSERT(len != BITMAP_BIT_INDEX_INVALID);
        while (1) {
                bitmap_bit_index_t first_bit =
                        bitmap_find_set_bit(bitmap, start_pos, words_len);
                if (first_bit == BITMAP_BIT_INDEX_INVALID) {
                        return BITMAP_BIT_INDEX_INVALID;
                }
                bitmap_bit_index_t last_bit =
                        bitmap_find_last_continuous_set_bit(
                                bitmap, first_bit, words_len
                        );
                ASSERT(last_bit != BITMAP_BIT_INDEX_INVALID);
                bitmap_bit_index_t found_len = last_bit - first_bit + 1;
                if (len <= found_len) {
                        return first_bit;
                }
                start_pos = last_bit + 1;
        }
}
