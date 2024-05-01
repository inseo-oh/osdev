// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "memory.h"
#include "kernel/arch/arch.h"
#include "kernel/heap/heap.h"
#include "kernel/kernel.h"
#include "kernel/utility/utility.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static char const *LOG_TAG = "virtzone";

struct FreeRegion {
        struct List_Node node_head;
        uintptr_t begin_addr;
        uintptr_t end_addr;
};

struct FreeRegionsForSize {
        struct AVLTree_Node node_head;
        struct List free_region_list;
};

// Finds and returns first region that is larger than given `page_count`.
//
// Returns NULL if free region cannot be found.
static struct FreeRegion *
find_and_take_free_region(struct VirtZone *zone, size_t page_count) {
        ASSERT(page_count != 0);
        if (!zone->free_page_list_for_size_tree.root) {
                return NULL;
        }
        for (struct FreeRegionsForSize *current = avltree_min_node(
                     (struct AVLTree_Node *)
                             zone->free_page_list_for_size_tree.root
             );
             current;
             current = avltree_successor_of(&current->node_head)) {
                size_t current_page_count = current->node_head.key;
                if (current_page_count < page_count) {
                        continue;
                }
                struct FreeRegion *region =
                        (struct FreeRegion *)current->free_region_list.head;
                list_remove_head(&current->free_region_list);
                if (region) {
                        if (!current->free_region_list.head) {
                                avltree_remove(
                                        &zone->free_page_list_for_size_tree,
                                        &current->node_head
                                );
                                kfree(current);
                        }
                        return region;
                }
        }
        return NULL;
}

// Finds any first region that includes given range.
//
// Returns NULL if region cannot be found or it is not large enough.
static struct FreeRegion *take_free_region_including(
        struct VirtZone *zone, uintptr_t addr, size_t page_count
) {
        ASSERT(page_count != 0);
        ASSERT(zone->free_page_list_for_size_tree.root);
        for (struct FreeRegionsForSize *current = avltree_min_node(
                     (struct AVLTree_Node *)
                             zone->free_page_list_for_size_tree.root
             );
             current;
             current = avltree_successor_of(&current->node_head)) {
                size_t current_page_count = current->node_head.key;
                if (current_page_count < page_count) {
                        continue;
                }
                struct FreeRegion *result_region;
                for (result_region = (struct FreeRegion *)
                                             current->free_region_list.head;
                     result_region;
                     result_region = result_region->node_head.next) {
                        if (addr < result_region->begin_addr ||
                            result_region->end_addr <= addr) {
                                continue;
                        }
                        if (result_region->end_addr <
                            addr + page_count * PAGE_SIZE) {
                                // The region includes given address, but there
                                // are not enough pages to map.
                                return NULL;
                        }
                        break;
                }
                if (result_region) {
                        list_remove(
                                &current->free_region_list,
                                &result_region->node_head
                        );
                        if (!current->free_region_list.head) {
                                avltree_remove(
                                        &zone->free_page_list_for_size_tree,
                                        &current->node_head
                                );
                                kfree(current);
                        }
                        return result_region;
                }
        }
        return NULL;
}

static struct FreeRegion *
make_free_region(uintptr_t begin_addr, uintptr_t end_addr) {
        struct FreeRegion *region = kmalloc(sizeof(*region));
        if (!region) {
                LOGE(LOG_TAG, "Not enough kmalloc memory for FreeRegion");
                return NULL;
        }
        region->begin_addr = begin_addr;
        region->end_addr = end_addr;
        return region;
}

struct AVLTree;
void avltree_insert2(
        struct AVLTree *tree, struct AVLTree_Node *node, avltree_key_t key
);

static void add_free_region(struct VirtZone *zone, struct FreeRegion *region) {
        ASSERT(is_aligned(PAGE_SIZE, region->end_addr - region->begin_addr));
        size_t page_count = (region->end_addr - region->begin_addr) / PAGE_SIZE;
        struct FreeRegionsForSize *regions =
                avltree_search(&zone->free_page_list_for_size_tree, page_count);
        if (!regions) {
                regions = kmalloc(sizeof(*regions));
                if (!regions) {
                        panic("Not enough kmalloc memory to create new free "
                              "region");
                }
                kmemset(regions, 0, sizeof(*regions));
                avltree_insert(
                        &zone->free_page_list_for_size_tree,
                        &regions->node_head,
                        page_count
                );
        }
        list_insert_tail(&regions->free_region_list, &region->node_head);
}

static void free_regions_tree(struct FreeRegionsForSize *regions_root) {
        for (int i = 0; i < 2; ++i) {
                struct FreeRegionsForSize *child =
                        regions_root->node_head.children[i];
                if (child) {
                        free_regions_tree(child);
                }
        }
        if (regions_root->free_region_list.head) {
                list_remove_head(&regions_root->free_region_list);
                kfree(regions_root->free_region_list.head);
        }
        kfree(regions_root);
}

void *virtzone_alloc_region(struct VirtZone *zone, size_t page_count) {
        ASSERT(page_count);
        // `region` contains region that includes region we are about to use and
        // remaining region follows.
        struct FreeRegion *region = find_and_take_free_region(zone, page_count);
        if (!region) {
                LOGE(LOG_TAG,
                     "No free region found (page_count: %u)",
                     page_count);
                return NULL;
        }
        uint8_t *virtbase = (uint8_t *)region->begin_addr;
        ASSERT(is_aligned(PAGE_SIZE, (uintptr_t)virtbase));
        // Shrink the region by advancing its beginning address
        region->begin_addr += page_count * PAGE_SIZE;
        bool is_range_valid = region->begin_addr < region->end_addr;
        if (is_range_valid) {
                // Add the shrunk region back to free regions.
                add_free_region(zone, region);
        } else {
                // We've used all free regions
                kfree(region);
        }
        return (void *)virtbase;
}

// Returns false if allocation fails.
bool virtzone_alloc_region_at(
        struct VirtZone *zone, void *virtbase, size_t page_count
) {
        ASSERT(virtbase);
        ASSERT(is_aligned(PAGE_SIZE, (uintptr_t)virtbase));
        ASSERT(page_count);
        // "Left" region intially contains before and after the desired region.
        // Its end point will be set to `virtbase` to leave room for our
        // allocated addresses.
        struct FreeRegion *left_region = take_free_region_including(
                zone, (uintptr_t)virtbase, page_count
        );
        struct FreeRegion *right_region = NULL;
        if (!left_region) {
                goto fail;
        }
        // Because "Left" region's end point was set to `virtbase`, we lose
        // address ranges after the addresses we allocated. To fix that, we
        // create "Right" region that covers those addresses.
        right_region = make_free_region(
                (uintptr_t)virtbase + page_count * PAGE_SIZE,
                left_region->end_addr
        );
        if (!right_region) {
                goto fail;
        }
        left_region->end_addr = (uintptr_t)virtbase;
        bool is_range_valid = left_region->begin_addr < left_region->end_addr;
        if (is_range_valid) {
                add_free_region(zone, left_region);
        } else {
                // We've used all of the left_region
                kfree(left_region);
        }
        left_region = NULL;
        is_range_valid = right_region->begin_addr < right_region->end_addr;
        if (is_range_valid) {
                add_free_region(zone, right_region);
        } else {
                // We've used all of the right_region
                kfree(right_region);
        }
        right_region = NULL;

        return true;
fail:
        if (right_region) {
                kfree(right_region);
        }
        if (left_region) {
                kfree(left_region);
        }
        return false;
}

void virtzone_free_region(
        struct VirtZone *zone, void *base, size_t page_count
) {
        ASSERT(base != 0);
        ASSERT(page_count != 0);
        struct FreeRegion *region = make_free_region(
                (uintptr_t)base, (uintptr_t)base + page_count * PAGE_SIZE
        );
        if (!region) {
                panic("Not enough kmalloc memory for new free region");
        }
        add_free_region(zone, region);
}

void virtzone_init(
        struct VirtZone *out, uintptr_t begin_addr, uintptr_t end_addr
) {
        kmemset(out, 0, sizeof(*out));
        struct FreeRegion *region = make_free_region(begin_addr, end_addr);
        if (!region) {
                panic("Not enough kmalloc memory for initial free region");
        }
        add_free_region(out, region);
}

void virtzone_deinit(struct VirtZone *zone) {
        free_regions_tree((struct FreeRegionsForSize *)
                                  zone->free_page_list_for_size_tree.root);
}
