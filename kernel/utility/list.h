// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#pragma once
#error Use utility.h instead

struct List {
        void *head;
        void *tail;
};

struct List_Node {
        void *next;
        void *prev;
};

void list_insert_before(
        struct List *list, struct List_Node *node, struct List_Node *before
);
void list_insert_after(
        struct List *list, struct List_Node *node, struct List_Node *after
);
void list_remove(struct List *list, struct List_Node *node);

static inline void list_insert_head(struct List *list, struct List_Node *node) {
        list_insert_before(list, node, list->head);
}

static inline void list_insert_tail(struct List *list, struct List_Node *node) {
        list_insert_after(list, node, list->tail);
}

static inline void list_remove_head(struct List *list) {
        list_remove(list, list->head);
}

static inline void list_remove_tail(struct List *list) {
        list_remove(list, list->tail);
}
