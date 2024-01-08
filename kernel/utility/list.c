// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "utility.h"
#include <kernel/kernel.h>
#include <stddef.h>

#ifdef ISOS_ULTRA_PARANOID_MODE

static void check_node_integrity(struct List_Node *node) {
        if (node->prev && ((struct List_Node *)node->prev)->next != node) {
                panic("struct List_Node integrity check failed: "
                      "node->prev->next != node");
        }
        if (node->next && ((struct List_Node *)node->next)->prev != node) {
                panic("struct List_Node integrity check failed: "
                      "node->next->prev != node");
        }
}

static void check_list_integrity_forward(struct List list) {
        struct List_Node *expected_prev = NULL;
        for (struct List_Node *node = list.head; node;
             expected_prev = node, node = node->next) {
                if (expected_prev != node->prev) {
                        panic("struct List integrity check failed: prev "
                              "pointer mismatch");
                }
                check_node_integrity(node);
        }
}

static void check_list_integrity_backward(struct List list) {
        struct List_Node *expected_next = NULL;
        for (struct List_Node *node = list.tail; node;
             expected_next = node, node = node->prev) {
                if (expected_next != node->next) {
                        panic("struct List integrity check failed: next "
                              "pointer mismatch");
                }
                check_node_integrity(node);
        }
}

static void check_list_integrity(struct List list) {
        check_list_integrity_forward(list);
        check_list_integrity_backward(list);
}
#endif

void list_insert_before(
        struct List *list, struct List_Node *node, struct List_Node *before
) {
        ASSERT(list);
        ASSERT(node);
#ifdef ISOS_ULTRA_PARANOID_MODE
        check_list_integrity(*list);
#endif
        node->next = before;
        if (before) {
                node->prev = before->prev;
        } else {
                node->prev = NULL;
        }
        if (node->prev) {
                ((struct List_Node *)node->prev)->next = node;
        }
        if (node->next) {
                ((struct List_Node *)node->next)->prev = node;
        }
        if (before == list->head) {
                list->head = node;
        }
        if (!list->tail) {
                list->tail = node;
        }
#ifdef ISOS_ULTRA_PARANOID_MODE
        check_list_integrity(*list);
#endif
}

void list_insert_after(
        struct List *list, struct List_Node *node, struct List_Node *after
) {
        ASSERT(list);
        ASSERT(node);
#ifdef ISOS_ULTRA_PARANOID_MODE
        check_list_integrity(*list);
#endif
        if (after) {
                node->next = after->next;
        } else {
                node->next = NULL;
        }
        node->prev = after;
        if (node->prev) {
                ((struct List_Node *)node->prev)->next = node;
        }
        if (node->next) {
                ((struct List_Node *)node->next)->prev = node;
        }
        if (after == list->tail) {
                list->tail = node;
        }
        if (!list->head) {
                list->head = node;
        }
#ifdef ISOS_ULTRA_PARANOID_MODE
        check_list_integrity(*list);
#endif
}

void list_remove(struct List *list, struct List_Node *node) {
        ASSERT(list);
        ASSERT(node);
#ifdef ISOS_ULTRA_PARANOID_MODE
        check_list_integrity(*list);
#endif
        if (node->prev) {
                ((struct List_Node *)node->prev)->next = node->next;
        }
        if (node->next) {
                ((struct List_Node *)node->next)->prev = node->prev;
        }
        if (node == list->head) {
                list->head = node->next;
        }
        if (node == list->tail) {
                list->tail = node->prev;
        }
#ifdef ISOS_ULTRA_PARANOID_MODE
        check_list_integrity(*list);
#endif
}
