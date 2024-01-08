// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause

// This one is only for testing
// #define USE_KMALLOC_AS_BACKING_STORE

#include "heap.h"
#include <kernel/arch/arch.h>
#include <kernel/kernel.h>
#include <kernel/lock/spinlock.h>
#include <kernel/utility/utility.h>
#ifndef USE_KMALLOC_AS_BACKING_STORE
#include <kernel/tasks/tasks.h>
#endif
#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Allocation regions are divided into two categories:
// - Small regions: Backed by only one page, Fixed-size blocks
// - Big regions: Single large block that may span across multiple pages.

struct HeapRegion {
        // NOTE: On big regions, node_head is filled with NULLs as those are not
        //       part of the list.
        struct List_Node node_head;
        size_t page_count;
        size_t used_block_count;
        size_t block_size;
        bitmap_word_t bitmap;
        max_align_t pool[];
};

#define ALIGN_UP(_x) \
        (((_x) + alignof(max_align_t) - 1) & ~(sizeof(max_align_t) - 1))

#define BITMAP_WORD_COUNT            1
#define SMALL_REGION_MAX_BLOCK_COUNT (BITMAP_WORD_COUNT * BITMAP_BITS_PER_WORD)
#define SMALL_REGION_PAGE_COUNT      1
#define SMALL_REGION_SIZE            (SMALL_REGION_PAGE_COUNT * PAGE_SIZE)
#define SMALL_REGION_POOL_SIZE       (SMALL_REGION_SIZE - sizeof(struct HeapRegion))
#define SMALL_REGION_BLOCK_SIZE_UNALIGNED \
        (SMALL_REGION_POOL_SIZE / SMALL_REGION_MAX_BLOCK_COUNT)
#define SMALL_REGION_BLOCK_SIZE ALIGN_UP(SMALL_REGION_BLOCK_SIZE_UNALIGNED)
#define SMALL_REGION_BLOCK_COUNT \
        (SMALL_REGION_POOL_SIZE / SMALL_REGION_BLOCK_SIZE)
#define SMALL_REGION_ALLOC_MAX_SIZE \
        (SMALL_REGION_BLOCK_SIZE * SMALL_REGION_BLOCK_COUNT)
#define WASTED_SIZE (SMALL_REGION_POOL_SIZE - SMALL_REGION_ALLOC_MAX_SIZE)

struct Alloc {
        struct HeapRegion *region;
        size_t block_count;
        max_align_t data[];
};

static struct List s_region_list;
static struct SpinLock s_lock;

static size_t needed_size_for_alloc(size_t desired_size) {
        return align_up(
                alignof(max_align_t), desired_size + sizeof(struct Alloc)
        );
}

static bool should_use_small_region(size_t block_size) {
        return block_size <= SMALL_REGION_ALLOC_MAX_SIZE;
}

static struct HeapRegion *new_region(size_t desired_block_size) {
        size_t page_count;
        size_t block_count;
        size_t block_size;
        size_t needed_size = needed_size_for_alloc(desired_block_size);
        if (needed_size <= SMALL_REGION_ALLOC_MAX_SIZE) {
                page_count = 1;
                block_count = SMALL_REGION_BLOCK_COUNT;
                block_size = SMALL_REGION_BLOCK_SIZE;
        } else {
                page_count = to_block_count(
                        PAGE_SIZE, needed_size + sizeof(struct HeapRegion)
                );
                block_count = 1;
                block_size = needed_size;
        }
#ifdef USE_KMALLOC_AS_BACKING_STORE
        struct HeapRegion *region = kmalloc(page_count * PAGE_SIZE);
#else
        uintptr_t paddr_unused;
        struct HeapRegion *region = process_alloc_pages(
                process_running(),
                &paddr_unused,
                page_count,
                (struct Proc_MapOptions){.writable = true, .executable = false}
        );
#endif
        if (!region) {
                return NULL;
        }
        region->block_size = block_size;
        region->bitmap = 0;
        region->page_count = page_count;
        region->used_block_count = 0;
        bitmap_set_multi(&region->bitmap, 0, block_count);
        return region;
}

static void *alloc_from_region(struct HeapRegion *region, size_t size) {
        ASSERT(alignof(max_align_t) <= region->block_size);
        size_t block_count =
                to_block_count(region->block_size, needed_size_for_alloc(size));
        bitmap_bit_index_t block_index =
                bitmap_find_set_bits(&region->bitmap, 0, block_count, 1);
        if (block_index == BITMAP_BIT_INDEX_INVALID) {
                return NULL;
        }
        bitmap_clear_multi(&region->bitmap, block_index, block_count);
        uintptr_t offset_in_pool = block_index * region->block_size;
        ASSERT(is_aligned(alignof(max_align_t), offset_in_pool));
        struct Alloc *alloc =
                (struct Alloc *)((uintptr_t)region->pool + offset_in_pool);
        ++region->used_block_count;
        alloc->region = region;
        alloc->block_count = block_count;
        return alloc->data;
}

static void *alloc_within_regions(size_t size) {
        if (SMALL_REGION_ALLOC_MAX_SIZE < needed_size_for_alloc(size)) {
                return NULL;
        }
        for (struct HeapRegion *region = s_region_list.head; region;
             region = region->node_head.next) {
                if (!region->bitmap) {
                        continue;
                }
                void *result = alloc_from_region(region, size);
                if (result) {
                        return result;
                }
        }
        return NULL;
}

void *vmmalloc(size_t size) {
        void *result = NULL;
        bool prev_interrupt_state;
        spinlock_lock(&s_lock, &prev_interrupt_state);
        result = alloc_within_regions(size);
        if (result) {
                goto out;
        }
        bool is_small_region =
                should_use_small_region(needed_size_for_alloc(size));
        struct HeapRegion *region = new_region(size);
        if (!region) {
                goto out;
        }
        if (is_small_region) {
                list_insert_tail(&s_region_list, &region->node_head);
        } else {
                // Big regions are not part of the list.
                region->node_head.next = NULL;
                region->node_head.prev = NULL;
        }
        result = alloc_from_region(region, size);
        ASSERT(result);
out:
        spinlock_unlock(&s_lock, prev_interrupt_state);
        return result;
}

void vmfree(void *ptr) {
        bool prev_interrupt_state;
        spinlock_lock(&s_lock, &prev_interrupt_state);
        struct Alloc *alloc =
                (struct Alloc *)((uintptr_t)ptr - offsetof(struct Alloc, data));
        uintptr_t offset_in_pool =
                (uintptr_t)ptr - (uintptr_t)alloc->region->pool;
        bitmap_bit_index_t block_index =
                offset_in_pool / alloc->region->block_size;
        bitmap_set_multi(
                &alloc->region->bitmap, block_index, alloc->block_count
        );
        --alloc->region->used_block_count;
        // If The region is empty -> Free the region
        if (!alloc->region->used_block_count) {
                // If the region is big region, prev is NULL just like first
                // entry of the list. But since big regions are not part of
                // the list, list head won't match with the region pointer.
                // We can use this to distinguish between first entry and the
                // big region.
                if (!(!alloc->region->node_head.prev &&
                      (s_region_list.head != alloc->region))) {
                        list_remove(&s_region_list, &alloc->region->node_head);
                }
#ifdef USE_KMALLOC_AS_BACKING_STORE
                kfree(alloc->region);
#else
                process_free_pages(
                        process_running(),
                        alloc->region,
                        alloc->region->page_count
                );
#endif
        }
        spinlock_unlock(&s_lock, prev_interrupt_state);
}

void *vmrealloc(void *ptr, size_t new_size) {
        bool prev_interrupt_state;
        spinlock_lock(&s_lock, &prev_interrupt_state);
        struct Alloc *alloc =
                (struct Alloc *)((uintptr_t)ptr - offsetof(struct Alloc, data));
        size_t old_size = alloc->block_count * alloc->region->block_size;
        spinlock_unlock(&s_lock, prev_interrupt_state);
        
        // TODO: Consider implementing in-place reallocation.
        void *new_ptr = vmmalloc(new_size);
        if (!new_ptr) {
                return NULL;
        }
        kmemcpy(new_ptr, ptr, old_size);
        return new_ptr;
}