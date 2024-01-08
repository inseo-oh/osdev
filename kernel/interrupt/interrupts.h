// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#pragma once
#include <kernel/utility/utility.h>

typedef avltree_key_t interrupt_num_t;

struct Interrupts_HandlerNode {
        struct AVLTree_Node node_head;
        void (*handler_fn)(struct Interrupts_HandlerNode *handler);
        interrupt_num_t interrupt_number;
};

// Handler must be static or heap memory.
void interrupts_register_handler(struct Interrupts_HandlerNode *handler);
void interrupts_on_interrupt(interrupt_num_t interrupt_number);
