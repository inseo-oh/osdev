// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "_internal.h"
#include "memory.h"
#include <kernel/arch/arch.h>
#include <kernel/heap/heap.h>
#include <kernel/kernel.h>
#include <kernel/lock/spinlock.h>
#include <kernel/utility/utility.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct PageGroup {
        struct List_Node node_head;
        struct PhysPage_Descriptor descriptor;
        struct PhysZone physzone;
};

static struct List s_group_list;
static struct SpinLock s_lock;

// TODO: Get rid of `what` parameter.
struct PhysPage_Addr physpage_alloc(size_t count) {
        bool prev_interrupt_state;
        spinlock_lock(&s_lock, &prev_interrupt_state);
        struct PhysPage_Addr out = PHYSPAGE_NULL;
        ASSERT(count != 0);
        // Actual page count must be 2^n
        size_t actual_page_count = 1;
        for (; actual_page_count < count; actual_page_count *= 2) {}
        for (struct PageGroup *group = s_group_list.head; group;
             group = group->node_head.next) {
                if (group->descriptor.page_count < count) {
                        continue;
                }
                out.value = physzone_alloc(&group->physzone, actual_page_count);
                if (out.value) {
                        break;
                }
        }
        spinlock_unlock(&s_lock, prev_interrupt_state);
        return out;
}

void physpage_free(struct PhysPage_Addr addr, size_t count) {
        uintptr_t base = addr.value;
        ASSERT(is_aligned(PAGE_SIZE, base));
        ASSERT(count != 0);
        bool prev_interrupt_state;
        spinlock_lock(&s_lock, &prev_interrupt_state);

        // Actual page count must be 2^n
        size_t allocated_page_count = 1;
        for (; allocated_page_count < count; allocated_page_count *= 2) {}
        bool free_ok = false;
        for (struct PageGroup *group = s_group_list.head; group;
             group = group->node_head.next) {
                bool is_out_of_range =
                        (base < group->descriptor.base) ||
                        ((group->descriptor.base +
                          (group->descriptor.page_count * PAGE_SIZE)) <= base);
                if (is_out_of_range) {
                        continue;
                }
                ASSERT(!free_ok);
                physzone_free(&group->physzone, base, allocated_page_count);
                free_ok = true;
        }
        if (!free_ok) {
                panic("physpage_free(): %p does not look like allocated "
                      "page\n",
                      base);
        }
        spinlock_unlock(&s_lock, prev_interrupt_state);
}

void physpage_register(struct PhysPage_Descriptor const *descriptor) {
        ASSERT(is_aligned(PAGE_SIZE, descriptor->base));
        ASSERT(descriptor->base != 0);
        ASSERT(descriptor->page_count != 0);
        bool prev_interrupt_state;
        spinlock_lock(&s_lock, &prev_interrupt_state);
        // Given page count is not likely going to be 2^n sized, which is
        // required for buddy allocation algorithm. The solution is to split
        // into multiple 2^n sized groups.
        size_t remaining_page_count = descriptor->page_count;
        uintptr_t next_base = descriptor->base;
        while (remaining_page_count != 0) {
                size_t group_page_count = 1;
                for (; group_page_count * 2 <= remaining_page_count;
                     group_page_count *= 2) {}
                struct PageGroup *ppg = kmalloc(sizeof(*ppg));
                if (!ppg) {
                        panic("Not enough kmalloc memory for PageGroup");
                }
                ppg->descriptor.base = next_base;
                ppg->descriptor.page_count = group_page_count;
                ppg->physzone = physzone_init(
                        ppg->descriptor.base,
                        ppg->descriptor.page_count * PAGE_SIZE
                );
                list_insert_tail(&s_group_list, &ppg->node_head);
                next_base += group_page_count * PAGE_SIZE;
                remaining_page_count -= group_page_count;
        }
        spinlock_unlock(&s_lock, prev_interrupt_state);
}
