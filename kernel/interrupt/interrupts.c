// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "interrupts.h"
#include "kernel/arch/arch.h"
#include "kernel/kernel.h"
#include "kernel/lock/spinlock.h"
#include "kernel/utility/utility.h"
#include <stdbool.h>

static char const *LOG_TAG = "interrupts";

static struct AVLTree s_handlers;
static struct SpinLock s_lock;

// NOTE: The handler must point to static location(e.g. static variable,
// kmalloc()ed memory)
void interrupts_register_handler(struct Interrupts_HandlerNode *handler) {
        ENTER_NO_INTERRUPT_SECTION();
        bool prev_interrupt_state;
        spinlock_lock(&s_lock, &prev_interrupt_state);
        avltree_insert(
                &s_handlers, &handler->node_head, handler->interrupt_number
        );
        spinlock_unlock(&s_lock, prev_interrupt_state);
        LEAVE_NO_INTERRUPT_SECTION();
}

void interrupts_on_interrupt(interrupt_num_t interrupt_number) {
        ASSERT(!interrupts_are_enabled());
        ENTER_NO_INTERRUPT_SECTION();
        bool prev_interrupt_state;
        spinlock_lock(&s_lock, &prev_interrupt_state);
        struct Interrupts_HandlerNode *handler =
                avltree_search(&s_handlers, interrupt_number);
        if (!handler) {
                LOGE(LOG_TAG,
                     "Got INT %u but no handler assigned to it",
                     interrupt_number);
                goto out;
        }
        ASSERT(handler->handler_fn);
        handler->handler_fn(handler);
out:
        spinlock_unlock(&s_lock, prev_interrupt_state);
        LEAVE_NO_INTERRUPT_SECTION();
}
