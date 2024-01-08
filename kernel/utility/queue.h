// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#pragma once
#error Use utility.h instead
#include <stdbool.h>
#include <stddef.h>
#include <kernel/utility/utility.h>

struct Queue {
        void *buf;
        size_t item_count;
        size_t item_size;
        size_t insert_at;
        size_t remove_at;
};

// WANING: buf must be able to hold `(item_count * item_size)` bytes!
struct Queue queue_init(void *buf, size_t item_count, size_t item_size);
WARN_UNUSED_RESULT bool queue_enqueue(struct Queue *queue, void *data);
void *queue_peek(struct Queue const *queue, void *buf);
void *queue_dequeue(struct Queue *queue, void *buf);
void queue_empty(struct Queue *queue);
bool queue_is_empty(struct Queue const *queue);
bool queue_is_full(struct Queue const *queue);
