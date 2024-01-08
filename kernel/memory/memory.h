// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#pragma once
#include <kernel/arch/arch.h>
#include <kernel/utility/utility.h>

////////////////////////////////////////////////////////////////////////////////
// Physical page management
////////////////////////////////////////////////////////////////////////////////

// Distinct integer type for physical address. Physical address is distinct type
// to avoid accidentally using the address as virtual address.
struct PhysPage_Addr {
        uintptr_t value;
};

// `NULL` for `PhysPage_Addr`.
static const struct PhysPage_Addr PHYSPAGE_NULL = {0};

struct PhysPage_Descriptor {
        uintptr_t base;
        size_t page_count;
};

WARN_UNUSED_RESULT struct PhysPage_Addr physpage_alloc(size_t count);
void physpage_free(struct PhysPage_Addr addr, size_t count);
void physpage_register(struct PhysPage_Descriptor const *descriptor);

////////////////////////////////////////////////////////////////////////////////
// Virtual address management
////////////////////////////////////////////////////////////////////////////////

struct VirtZone {
        struct AVLTree free_page_list_for_size_tree;
};

// Returns NULL if allocation fails.
WARN_UNUSED_RESULT void *
virtzone_alloc_region(struct VirtZone *zone, size_t page_count);
// Returns NULL if allocation fails.
WARN_UNUSED_RESULT bool virtzone_alloc_region_at(
        struct VirtZone *zone, void *virtbase, size_t page_count
);
void virtzone_free_region(struct VirtZone *zone, void *base, size_t page_count);
void virtzone_init(
        struct VirtZone *out, uintptr_t begin_addr, uintptr_t end_addr
);
void virtzone_deinit(struct VirtZone *zone);
