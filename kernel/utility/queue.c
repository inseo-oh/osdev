// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "utility.h"
#include <kernel/kernel.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct Queue queue_init(void *buf, size_t item_count, size_t item_size) {
        ASSERT(item_count);
        ASSERT(item_size);
        return (struct Queue){
                .buf = buf,
                .item_count = item_count,
                .item_size = item_size,
                .insert_at = 0,
                .remove_at = 0,
        };
}

bool queue_enqueue(struct Queue *queue, void *data) {
        if (queue_is_full(queue)) {
                return false;
        }
        ++queue->insert_at;
        queue->insert_at %= queue->item_count;
        kmemcpy(&((uint8_t *)queue->buf)[queue->item_size * queue->insert_at],
                data,
                queue->item_size);
        return true;
}

void *queue_peek(struct Queue const *queue, void *buf) {
        if (queue_is_empty(queue)) {
                return NULL;
        }
        unsigned index = (queue->remove_at + 1) % queue->item_count;
        kmemcpy(buf,
                &((uint8_t *)queue->buf)[queue->item_size * index],
                queue->item_size);
        return buf;
}

void *queue_dequeue(struct Queue *queue, void *buf) {
        if (queue_is_empty(queue)) {
                return NULL;
        }
        ++queue->remove_at;
        queue->remove_at %= queue->item_count;
        kmemcpy(buf,
                &((uint8_t *)queue->buf)[queue->item_size * queue->remove_at],
                queue->item_size);
        return buf;
}

void queue_empty(struct Queue *queue) {
        queue->insert_at = 0;
        queue->remove_at = 0;
}

bool queue_is_empty(struct Queue const *queue) {
        return queue->insert_at == queue->remove_at;
}

bool queue_is_full(struct Queue const *queue) {
        return ((queue->insert_at + 1) % queue->item_count) == queue->remove_at;
}
