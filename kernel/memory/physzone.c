// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "_internal.h"
#include <kernel/arch/arch.h>
#include <kernel/heap/heap.h>
#include <kernel/kernel.h>
#include <kernel/utility/utility.h>
#include <stdbool.h>
#include <stdint.h>

// #define VERIFY_REMAINING_PAGE_COUNT

static size_t needed_freelist_len(size_t page_count) {
        size_t len = 0;
        while (page_count) {
                len += page_count;
                page_count /= 2;
        }
        return len;
}

static size_t needed_freelist_byte_count(size_t freelist_len) {
        size_t freelist_bitmap_word_count =
                bitmap_needed_word_count(freelist_len);
        size_t freelist_byte_count = align_up(
                PAGE_SIZE, freelist_bitmap_word_count * sizeof(bitmap_word_t)
        );
        return freelist_byte_count;
}

static uint8_t needed_level_count(size_t pool_size) {
        size_t page_count = pool_size / PAGE_SIZE;
        uint8_t count = 0;
        while (page_count) {
                ++count;
                page_count /= 2;
        }
        return count;
}

// NOTE: Caller must ensure that page_count is 2^n
static uint8_t page_count_to_level(size_t page_count) {
        uint8_t level = 0;
        size_t current_page_count = 1;
        while (current_page_count < page_count) {
                ++level;
                current_page_count *= 2;
        }
        ASSERT(current_page_count == page_count);
        return level;
}

static size_t
offset_in_level_to_block_offset(uint8_t level, size_t offset_in_level) {
        while (level) {
                offset_in_level *= 2;
                --level;
        }
        return offset_in_level;
}

static size_t
block_offset_to_offset_in_level(uint8_t level, size_t block_offset) {
        size_t current_level = 0;
        size_t offset_in_level = block_offset;
        while (current_level < level) {
                offset_in_level /= 2;
                ++current_level;
        }
        return offset_in_level;
}

static size_t freelist_abs_offset_for(
        struct PhysZone const *zone, uint8_t level, size_t page_index_in_level
) {
        ASSERT(level < zone->level_count);
        size_t page_count = zone->pool_size / PAGE_SIZE;
        size_t current_level = 0;
        size_t offset = 0;
        while (current_level < level) {
                ASSERT(page_count);
                ++current_level;
                offset += page_count;
                page_count /= 2;
        }
        offset += page_index_in_level;
        return offset;
}

static size_t
freelist_len_of_level(struct PhysZone const *zone, uint8_t level) {
        size_t len = zone->pool_size / PAGE_SIZE;
        size_t current_level = 0;
        while (current_level < level) {
                ASSERT(len);
                ++current_level;
                len /= 2;
        }
        return len;
}

static bool find_avail_block(
        size_t *out_abs_offset,
        size_t *out_offset_in_level,
        struct PhysZone *zone,
        uint8_t level
) {
        size_t freelist_len = freelist_len_of_level(zone, level);
        size_t begin_offset = freelist_abs_offset_for(zone, level, 0);
        size_t end_offset = begin_offset + freelist_len;
        size_t offset = begin_offset;
        while (offset < end_offset) {
                // Search multiple bits at once
                if (((offset % BITMAP_BITS_PER_WORD) == 0) &&
                    !zone->bitmap[offset / BITMAP_BITS_PER_WORD]) {
                        offset += BITMAP_BITS_PER_WORD;
                        continue;
                }
                if (bitmap_is_set(zone->bitmap, offset)) {
                        *out_abs_offset = offset;
                        *out_offset_in_level = offset - begin_offset;
                        return true;
                }
                ++offset;
        }
        return false;
}

#ifdef VERIFY_REMAINING_PAGE_COUNT
static size_t compute_free_block_count(
        struct PhysZone *zone,
        uint8_t level,
        size_t page_index_in_level,
        size_t page_count
) {
        size_t root_offset =
                freelist_abs_offset_for(zone, level, page_index_in_level);
        if (bitmap_is_set(zone->bitmap, root_offset)) {
                return page_count;
        }
        if (level == 0) {
                return 0;
        }
        size_t left_page_index_in_level = page_index_in_level * 2;
        size_t right_page_index_in_level = left_page_index_in_level + 1;
        size_t left_count = compute_free_block_count(
                zone, level - 1, left_page_index_in_level, page_count / 2
        );
        size_t right_count = compute_free_block_count(
                zone, level - 1, right_page_index_in_level, page_count / 2
        );
        return left_count + right_count;
}

static void verify_remaining_page_count(struct PhysZone *zone) {
        ASSERT(!(zone->pool_size % PAGE_SIZE));
        size_t actual_remaining_page_count = compute_free_block_count(
                zone, zone->level_count - 1, 0, zone->pool_size / PAGE_SIZE
        );
        ASSERT((actual_remaining_page_count * PAGE_SIZE) ==
               zone->remaining_pool_size);
}
#endif

// Returns entry offset in the level, or (size_t) if no blocks are available.
static size_t alloc_block(struct PhysZone *zone, uint8_t level) {
        uint8_t current_level;
        size_t abs_offset = 0;
        size_t offset_in_level = 0;
        for (current_level = level; current_level < zone->level_count;
             ++current_level) {
                if (find_avail_block(
                            &abs_offset, &offset_in_level, zone, current_level
                    )) {
                        break;
                }
        }
        if (current_level == zone->level_count) {
                return (size_t)-1;
        }
        // If we found at upper level, divide upper level blocks.
        while (level < current_level) {
                ASSERT(bitmap_is_set(zone->bitmap, abs_offset));
                // Mark current entry as used
                bitmap_clear(zone->bitmap, abs_offset);
                // Move down one level and mark two entries as available.
                --current_level;
                offset_in_level *= 2;
                abs_offset = freelist_abs_offset_for(
                        zone, current_level, offset_in_level
                );
                bitmap_set(zone->bitmap, abs_offset);
                bitmap_set(zone->bitmap, abs_offset + 1);
        }
        // Mark current entry as unavailable
        bitmap_clear(zone->bitmap, abs_offset);
        return offset_in_level;
}

static void
free_block(struct PhysZone *zone, size_t offset_in_level, uint8_t level) {
        while (level < zone->level_count) {
                size_t abs_offset =
                        freelist_abs_offset_for(zone, level, offset_in_level);
                // Mark current entry as available
                bitmap_set(zone->bitmap, abs_offset);
                // If buddy is also free, both can be combined into one upper
                // block.
                size_t buddy_offset = ((offset_in_level % 2) == 0)
                                              ? (abs_offset + 1)
                                              : (abs_offset - 1);
                if (!bitmap_is_set(zone->bitmap, buddy_offset)) {
                        // If buddy is not free, stop here.
                        break;
                }
                // Mark both entries as non-available.
                bitmap_clear(zone->bitmap, abs_offset);
                bitmap_clear(zone->bitmap, buddy_offset);
                // Move up one level
                ++level;
                offset_in_level /= 2;
        }
}

uintptr_t physzone_alloc(struct PhysZone *zone, size_t page_count) {
        // FIXME: 2^n requirement is also handled from caller side. We don't
        //        have to do this here.
        size_t new_page_count = 1;
        while (new_page_count < page_count) {
                new_page_count *= 2;
        }
        if ((zone->pool_size / PAGE_SIZE) < new_page_count) {
                return 0;
        }
        if (zone->remaining_pool_size < (new_page_count * PAGE_SIZE)) {
                return 0;
        }
        uint8_t level = page_count_to_level(new_page_count);
        size_t offset_in_level = alloc_block(zone, level);
        if (offset_in_level == (size_t)-1) {
                // Allocation could still fail due to fragmentation.
                return 0;
        }
        uintptr_t block_offset =
                offset_in_level_to_block_offset(level, offset_in_level);
        zone->remaining_pool_size -= new_page_count * PAGE_SIZE;
#ifdef VERIFY_REMAINING_PAGE_COUNT
        verify_remaining_page_count(zone);
#endif

        return zone->pool_begin + (block_offset * PAGE_SIZE);
}

void physzone_free(struct PhysZone *zone, uintptr_t base, size_t page_count) {
        // FIXME: 2^n requirement is also handled from caller side. We don't
        //        have to do this here.
        size_t new_page_count = 1;
        while (new_page_count < page_count) {
                new_page_count *= 2;
        }
        uint8_t level = page_count_to_level(new_page_count);
        uintptr_t block_offset = (base - zone->pool_begin) / PAGE_SIZE;
        size_t offset_in_level =
                block_offset_to_offset_in_level(level, block_offset);
        free_block(zone, offset_in_level, level);
        zone->remaining_pool_size += new_page_count * PAGE_SIZE;

        ASSERT(!(zone->pool_size % PAGE_SIZE));
#ifdef VERIFY_REMAINING_PAGE_COUNT
        verify_remaining_page_count(zone);
#endif
}

struct PhysZone physzone_init(uintptr_t base, size_t size) {
        ASSERT(is_aligned(PAGE_SIZE, base));
        size_t pool_size = size;
        // Pool size must be 2^n
        {
                size_t new_pool_size = 1;
                while ((new_pool_size * 2) <= pool_size) {
                        new_pool_size *= 2;
                }
                pool_size = new_pool_size;
        }

        pool_size &= ~(PAGE_SIZE - 1); // Align to page boundary
        if (!pool_size) {
                ASSERT(!"Memory region is too small!");
        }
        size_t freelist_len = needed_freelist_len(pool_size / PAGE_SIZE);
        size_t freelist_byte_count = needed_freelist_byte_count(freelist_len);
        struct PhysZone zone;
        zone.bitmap = kmalloc(freelist_byte_count);
        if (!zone.bitmap) {
                TODO_HANDLE_ERROR();
        }
        zone.pool_size = pool_size;
        zone.remaining_pool_size = pool_size;
        zone.pool_begin = base;
        zone.level_count = needed_level_count(pool_size);

        // Clear our new bitmap(= All unavailable)
        kmemset(zone.bitmap, 0, freelist_byte_count);
        // Make very top level's block available
        bitmap_set(zone.bitmap, freelist_len - 1);
        return zone;
}
