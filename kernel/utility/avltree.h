// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#pragma once
#include <stdint.h>
#error Use utility.h instead

typedef uintptr_t avltree_key_t;

struct AVLTree_Node {
        void *parent, *children[2];
        avltree_key_t key;
};

struct AVLTree {
        void *root;
};

void *avltree_max_node(struct AVLTree_Node *node);
void *avltree_min_node(struct AVLTree_Node *node);
void *avltree_successor_of(struct AVLTree_Node *node);
void *avltree_predecessor_of(struct AVLTree_Node *node);
void *avltree_search(struct AVLTree *tree, avltree_key_t key);
void avltree_insert(struct AVLTree *tree, void *node, avltree_key_t key);
void avltree_remove(struct AVLTree *tree, struct AVLTree_Node *node);
