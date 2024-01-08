// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#pragma once
#include <kernel/utility/utility.h>
#include <stdint.h>

////////////////////////////////////////////////////////////////////////////////
// Physical Memory Zone (physzone)
////////////////////////////////////////////////////////////////////////////////

struct PhysZone {
        uintptr_t pool_begin;
        bitmap_word_t *bitmap;
        size_t pool_size;
        size_t remaining_pool_size;
        // Even managing thousands of petabytes require less than 60 levels, so
        // 8-bit is enough.
        uint8_t level_count;
};

uintptr_t physzone_alloc(struct PhysZone *zone, size_t page_count);
void physzone_free(struct PhysZone *zone, uintptr_t base, size_t page_count);
struct PhysZone physzone_init(uintptr_t base, size_t size);
