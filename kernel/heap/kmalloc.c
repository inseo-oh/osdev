// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "heap.h"
#include <kernel/kernel.h>
#include <kernel/lock/spinlock.h>
#include <kernel/utility/utility.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct HeapRegion {
        void *pool_start;
        size_t bitmap_word_count;
        size_t block_count;
        bitmap_word_t bitmap[];
};

struct Alloc {
        struct HeapRegion *region;
        size_t block_count;
        max_align_t data[];
};

#define BLOCK_SIZE                           64UL
#define INITIAL_REGION_BITMAP_WORD_COUNT_1MB 256UL
#define INITIAL_REGION_BITMAP_WORD_COUNT \
        (INITIAL_REGION_BITMAP_WORD_COUNT_1MB * 2)
#define INITIAL_REGION_BLOCK_COUNT \
        (INITIAL_REGION_BITMAP_WORD_COUNT * BITMAP_BITS_PER_WORD)
#define INITIAL_REGION_POOL_SIZE (INITIAL_REGION_BLOCK_COUNT * BLOCK_SIZE)
#define INITIAL_REGION_SIZE \
        (INITIAL_REGION_POOL_SIZE + sizeof(struct HeapRegion))

_Static_assert(alignof(max_align_t) <= BLOCK_SIZE, "Block size is too small");

// Currently there's only one region called initial region, but we still have
// "Region" concept so that it is easier when we decide to expand the kmalloc
// beyond the initial fixed-size pool.
static uint8_t s_initial_region_pool[INITIAL_REGION_SIZE];
static struct HeapRegion *s_initial_region;
static struct SpinLock s_lock;

static struct HeapRegion *init_region(void *base, size_t block_count) {
        struct HeapRegion *region = base;
        size_t total_byte_count = block_count * BLOCK_SIZE;
        // Initially we don't know how much is needed to manage blocks, so let's
        // assume we manage all the given blocks.
        region->bitmap_word_count = bitmap_needed_word_count(block_count);
        size_t bitmap_size =
                region->bitmap_word_count * sizeof(*region->bitmap);
        // Pool's start address needs to be aligned, so increase the bitmap size
        // accordingly(even though it is not actually be taken by bitmap).
        bitmap_size +=
                (alignof(max_align_t) - (bitmap_size % alignof(max_align_t)));
        // Now calculate real pool block count
        size_t pool_byte_count = total_byte_count - bitmap_size;
        size_t pool_block_count = pool_byte_count / BLOCK_SIZE;
        region->pool_start = (void *)((uintptr_t)base + bitmap_size);
        region->block_count = pool_block_count;
        kmemset(region->bitmap,
                0,
                region->bitmap_word_count * sizeof(*region->bitmap));
        bitmap_set_multi(region->bitmap, 0, pool_block_count);
        return region;
}

static void *alloc_from_region(struct HeapRegion *region, size_t size) {
        size_t needed_size = size + sizeof(struct Alloc);
        size_t block_count = to_block_count(BLOCK_SIZE, needed_size);
        bitmap_bit_index_t block_index = bitmap_find_set_bits(
                region->bitmap, 0, block_count, region->bitmap_word_count
        );
        if (block_index == BITMAP_BIT_INDEX_INVALID) {
                return NULL;
        }
        bitmap_clear_multi(region->bitmap, block_index, block_count);
        uintptr_t offset_in_pool = block_index * BLOCK_SIZE;
        ASSERT(is_aligned(alignof(max_align_t), offset_in_pool));
        struct Alloc *alloc = (struct Alloc *)((uintptr_t)region->pool_start +
                                               offset_in_pool);
        alloc->region = region;
        alloc->block_count = block_count;
        return alloc->data;
}

void kmalloc_init() {
        s_initial_region = init_region(
                s_initial_region_pool,
                to_block_count(BLOCK_SIZE, sizeof(s_initial_region_pool))
        );
}

void *kmalloc(size_t size) {
        bool prev_interrupt_state;
        spinlock_lock(&s_lock, &prev_interrupt_state);
        void *result = alloc_from_region(s_initial_region, size);
        spinlock_unlock(&s_lock, prev_interrupt_state);
        return result;
}

void kfree(void *ptr) {
        bool prev_interrupt_state;
        spinlock_lock(&s_lock, &prev_interrupt_state);
        struct Alloc *alloc =
                (struct Alloc *)((uintptr_t)ptr - offsetof(struct Alloc, data));
        uintptr_t offset_in_pool =
                (uintptr_t)ptr - (uintptr_t)alloc->region->pool_start;
        bitmap_bit_index_t block_index = offset_in_pool / BLOCK_SIZE;
        bitmap_set_multi(
                alloc->region->bitmap, block_index, alloc->block_count
        );
        spinlock_unlock(&s_lock, prev_interrupt_state);
}
