// SPDX-FileCopyrightText: (c) 2023, 2024 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "utility.h"
#include "kernel/kernel.h"
#include <stddef.h>
#include <time.h>

typedef enum { DIR_LEFT = 0, DIR_RIGHT = 1 } dir_t;

static dir_t child_dir_in_parent(struct AVLTree_Node const *node) {
        struct AVLTree_Node *parent_node = node->parent;
        ASSERT(parent_node);
        for (int i = 0; i < 2; ++i) {
                if (parent_node->children[i] == node) {
                        return (dir_t)i;
                }
        }
        ASSERT(!"`parent` does not have `node` as child?");
}

#ifdef YJK_ULTRA_PARANOID_MODE
static void check_integrity_inner(
        struct AVLTree tree,
        struct AVLTree_Node const *root,
        struct AVLTree_Node const *expected_parent
) {
        if (!root) {
                return;
        }
        struct AVLTree_Node *parent = root->parent;
        ASSERT(parent == expected_parent);
        if (parent) {
                ASSERT(parent->children[child_dir_in_parent(root)] == root);
        }
        ASSERT(root != root->children[DIR_LEFT]);
        ASSERT(root != root->children[DIR_RIGHT]);
        check_integrity_inner(tree, root->children[DIR_LEFT], root);
        check_integrity_inner(tree, root->children[DIR_RIGHT], root);
}

static void check_integrity(struct AVLTree tree) {
        check_integrity_inner(tree, tree.root, NULL);
}
#endif

static void
rotate(struct AVLTree *tree, struct AVLTree_Node *rotation_root, dir_t dir) {
        ASSERT(rotation_root);
        /*
                NOTE: Example tree assumes dir is left.
                      [P]
                       |
                      [Z]
                      / \
                  [...] [Y]
                        / \
                      [X] [...]
        */
        struct AVLTree_Node *node_z = rotation_root;
        struct AVLTree_Node *node_p = node_z->parent;
        struct AVLTree_Node *node_y = node_z->children[1 - dir];
        ASSERT(node_y);
        struct AVLTree_Node *node_x = node_y->children[dir];
#ifdef YJK_ULTRA_PARANOID_MODE
        check_integrity(*tree);
#endif
        /*
                       [P]       [X]
                        |
                       [Y]
                      /  \
                    [Z]  [...]
                    /
                [...]
        */
        if (node_p) {
                node_p->children[child_dir_in_parent(node_z)] = node_y;
                node_y->parent = node_p;
        } else {
                node_y->parent = NULL;
                tree->root = node_y;
        }
        node_y->children[dir] = node_z;
        node_z->parent = node_y;
        /*
                       [P]
                        |
                       [Y]
                      /  \
                    [Z]  [...]
                    / \
                [...]  [X]
        */
        node_z->children[1 - dir] = node_x;
        if (node_x) {
                node_x->parent = node_z;
        }
#ifdef YJK_ULTRA_PARANOID_MODE
        check_integrity(*tree);
#endif
}

static size_t height_of_subtree(struct AVLTree_Node const *node) {
        struct AVLTree_Node const *subtree_root = node;
        size_t lheight = 0;
        size_t rheight = 0;
        if (subtree_root->children[DIR_LEFT]) {
                lheight =
                        height_of_subtree(subtree_root->children[DIR_LEFT]) + 1;
        }
        if (subtree_root->children[DIR_RIGHT]) {
                rheight = height_of_subtree(subtree_root->children[DIR_RIGHT]) +
                          1;
        }
        return (lheight < rheight) ? rheight : lheight;
}

static int bf_of(struct AVLTree_Node const *node) {
        struct AVLTree_Node *lchild = node->children[DIR_LEFT];
        struct AVLTree_Node *rchild = node->children[DIR_RIGHT];
        size_t lheight = lchild ? (height_of_subtree(lchild) + 1) : 0;
        size_t rheight = rchild ? (height_of_subtree(rchild) + 1) : 0;
        return (int)lheight - (int)rheight;
}

static void
rebalance_subtree(struct AVLTree *tree, struct AVLTree_Node *subtree_root) {
        ASSERT(subtree_root);
        int bfactor[2] = {bf_of(subtree_root)};
        if (bfactor[0] > 1) {
                // Left heavy
                struct AVLTree_Node *child = subtree_root->children[DIR_LEFT];
                ASSERT(child);
                bfactor[1] = bf_of(child);
                if (bfactor[1] < 0) {
                        // Left-right heavy
                        rotate(tree, child, DIR_LEFT);
                }
                rotate(tree, subtree_root, DIR_RIGHT);
        }
        if (bfactor[0] < -1) {
                // Right heavy
                struct AVLTree_Node *child = subtree_root->children[DIR_RIGHT];
                ASSERT(child);
                bfactor[1] = bf_of(child);
                if (bfactor[1] > 0) {
                        // Right-left heavy
                        rotate(tree, child, DIR_RIGHT);
                }
                rotate(tree, subtree_root, DIR_LEFT);
        }
}

static void check_and_rebalance_tree(
        struct AVLTree *tree, struct AVLTree_Node *start_node
) {
        struct AVLTree_Node *current = start_node;
        while (current) {
                struct AVLTree_Node *old_parent = current->parent;
                rebalance_subtree(tree, current);
                current = old_parent;
        }
}

void *avltree_max_node(struct AVLTree_Node *node) {
        struct AVLTree_Node *current = node;
        while (current->children[DIR_RIGHT]) {
                current = current->children[DIR_RIGHT];
        }
        return current;
}

void *avltree_min_node(struct AVLTree_Node *node) {
        struct AVLTree_Node *current_node = node;
        while (current_node->children[DIR_LEFT]) {
                current_node = current_node->children[DIR_LEFT];
        }
        return current_node;
}

void *avltree_successor_of(struct AVLTree_Node *node) {
        if (node->children[DIR_RIGHT]) {
                return avltree_min_node(node->children[DIR_RIGHT]);
        }
        struct AVLTree_Node *parent = node->parent;
        struct AVLTree_Node *current = node;
        while (parent && child_dir_in_parent(current) == DIR_RIGHT) {
                current = parent;
                parent = current->parent;
        }
        return parent;
}

void *avltree_predecessor_of(struct AVLTree_Node *node) {
        if (node->children[DIR_LEFT]) {
                return avltree_max_node(node->children[DIR_LEFT]);
        }
        if (node->parent && child_dir_in_parent(node) == DIR_RIGHT) {
                return node->parent;
        }
        struct AVLTree_Node *parent = node->parent;
        struct AVLTree_Node *current = node;
        while (parent && child_dir_in_parent(current) == DIR_LEFT) {
                ASSERT(current != parent);
                current = parent;
                parent = current->parent;
        }

        return parent;
}

void *avltree_search(struct AVLTree *tree, avltree_key_t key) {
        struct AVLTree_Node *current = tree->root;
        while (current) {
                if (current->key == key) {
                        return current;
                }
                dir_t child_dir = (key < current->key) ? DIR_LEFT : DIR_RIGHT;
                current = current->children[child_dir];
        }
        return NULL;
}

void avltree_insert(struct AVLTree *tree, void *node, avltree_key_t key) {
#ifdef YJK_ULTRA_PARANOID_MODE
        check_integrity(*tree);
#endif
        ((struct AVLTree_Node *)node)->key = key;
        if (!tree->root) {
                tree->root = node;
                ((struct AVLTree_Node *)node)->parent = NULL;
                return;
        }

        struct AVLTree_Node *result_parent = tree->root;
        dir_t child_dir;
        for (struct AVLTree_Node *current = tree->root; current;
             child_dir = (key < current->key) ? DIR_LEFT : DIR_RIGHT,
                                 current = current->children[child_dir]) {
                result_parent = current;
        }
        ((struct AVLTree_Node *)node)->parent = result_parent;
        ASSERT(!result_parent->children[child_dir]);
        result_parent->children[child_dir] = node;
#ifdef YJK_ULTRA_PARANOID_MODE
        check_integrity(*tree);
#endif
        check_and_rebalance_tree(tree, ((struct AVLTree_Node *)node)->parent);

#ifdef YJK_ULTRA_PARANOID_MODE
        check_integrity(*tree);
#endif
}

void avltree_remove(struct AVLTree *tree, struct AVLTree_Node *node) {
#ifdef YJK_ULTRA_PARANOID_MODE
        check_integrity(*tree);
#endif
        struct AVLTree_Node *old_node = node;
        struct AVLTree_Node *replace_with;
        if (old_node->children[DIR_LEFT] && old_node->children[DIR_RIGHT]) {
                struct AVLTree_Node *successor = avltree_successor_of(node);
                avltree_remove(tree, successor);
                for (unsigned i = 0; i < 2; i++) {
                        struct AVLTree_Node *child = node->children[i];
                        successor->children[i] = node->children[i];
                        if (child) {
                                child->parent = successor;
                        }
                }
                replace_with = successor;
        } else {
                replace_with = old_node->children[DIR_LEFT];
                if (!old_node->children[DIR_LEFT]) {
                        replace_with = old_node->children[DIR_RIGHT];
                }
        }
        struct AVLTree_Node *parent = node->parent;
        if (parent) {
                parent->children[child_dir_in_parent(node)] = replace_with;
        } else {
                tree->root = replace_with;
        }
        if (replace_with) {
                replace_with->parent = parent;
        }

#ifdef YJK_ULTRA_PARANOID_MODE
        check_integrity(*tree);
#endif

        check_and_rebalance_tree(tree, ((struct AVLTree_Node *)node)->parent);

#ifdef YJK_ULTRA_PARANOID_MODE
        check_integrity(*tree);
#endif
}
