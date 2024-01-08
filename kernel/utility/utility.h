// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#index-warn_005funused_005fresult-function-attribute
#define WARN_UNUSED_RESULT __attribute((warn_unused_result))

// https://gcc.gnu.org/onlinedocs/gcc/x86-Function-Attributes.html#index-naked-function-attribute_002c-x86
#define NAKED __attribute((naked))

// https://gcc.gnu.org/onlinedocs/gcc-3.3/gcc/Type-Attributes.html
#define PACKED       __attribute((packed))
#define ALIGNED(__n) __attribute((aligned(__n)))

// https://gcc.gnu.org/onlinedocs/gcc-4.8.5/cpp/Stringification.html
#define STRINGIFY_HELPER(__x) #__x
#define STRINGIFY(__x)        STRINGIFY_HELPER(__x)

////////////////////////////////////////////////////////////////////////////////
// Alignment utilities
////////////////////////////////////////////////////////////////////////////////

// Computes mask value which gets ANDed to check if the given value is aligned.
// (Also used for aligning values)
//
// For example, if 4096 byte align is given, and assuming align value is power
// of 2, we get mask value by subtracting 1 from it. 4096: 1_0000_0000_0000_000
// --> 4095: 0_1111_1111_1111_1111
//
// If those bits are set, the given value is not aligned.
static inline uintptr_t align_and_mask(unsigned align) { return align - 1; }

static inline bool is_aligned(unsigned align, uintptr_t x) {
        return (x & align_and_mask(align)) == 0;
}

static inline uintptr_t align_down(unsigned align, uintptr_t x) {
        return x & ~align_and_mask(align);
}

static inline uintptr_t align_up(unsigned align, uintptr_t x) {
        return align_down(align, x + align - 1);
}

static inline size_t to_block_count(size_t block_size, size_t x) {
        if ((x % block_size) != 0) {
                return x / block_size + 1;
        }
        return x / block_size;
}

////////////////////////////////////////////////////////////////////////////////
// AVL tree
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
// Bitmap
////////////////////////////////////////////////////////////////////////////////

typedef uint64_t bitmap_word_t;
typedef uint64_t bitmap_bit_index_t;

#define BITMAP_BIT_INDEX_INVALID ((bitmap_bit_index_t)~0)
#define BITMAP_BITS_PER_WORD     (sizeof(bitmap_word_t) * 8)

static inline size_t bitmap_needed_word_count(size_t bit_cnt) {
        return (bit_cnt % BITMAP_BITS_PER_WORD)
                       ? ((bit_cnt / BITMAP_BITS_PER_WORD) + 1UL)
                       : bit_cnt / BITMAP_BITS_PER_WORD;
}

static inline void bitmap_set(bitmap_word_t *bitmap, size_t idx) {
        bitmap[idx / BITMAP_BITS_PER_WORD] |=
                ((bitmap_word_t)1 << (idx % BITMAP_BITS_PER_WORD));
}

static inline void bitmap_clear(bitmap_word_t *bitmap, size_t idx) {
        bitmap[idx / BITMAP_BITS_PER_WORD] &=
                ~((bitmap_word_t)1 << (idx % BITMAP_BITS_PER_WORD));
}

static inline bool bitmap_is_set(bitmap_word_t const *bitmap, size_t idx) {
        return bitmap[idx / BITMAP_BITS_PER_WORD] &
               ((bitmap_word_t)1 << (idx % BITMAP_BITS_PER_WORD));
}

// Returns BITMAP_BIT_INDEX_INVALID if bitmap is empty.
bitmap_bit_index_t bitmap_find_set_bit(
        bitmap_word_t const *bitmap,
        bitmap_bit_index_t start_pos,
        size_t words_len
);
// Returns BITMAP_BIT_INDEX_INVALID if an active bit doesn't exist.
bitmap_bit_index_t bitmap_find_last_continuous_set_bit(
        bitmap_word_t const *bitmap,
        bitmap_bit_index_t start_pos,
        size_t words_len
);
bool bitmap_are_set(
        bitmap_word_t const *bitmap,
        bitmap_bit_index_t offset,
        bitmap_bit_index_t len
);
void bitmap_set_multi(
        bitmap_word_t *bitmap, bitmap_bit_index_t offset, bitmap_bit_index_t len
);
void bitmap_clear_multi(
        bitmap_word_t *bitmap, bitmap_bit_index_t offset, bitmap_bit_index_t len
);
bitmap_bit_index_t bitmap_find_set_bits(
        bitmap_word_t const *bitmap,
        bitmap_bit_index_t start_pos,
        bitmap_bit_index_t len,
        size_t words_len
);

////////////////////////////////////////////////////////////////////////////////
// List
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
// Fixed size queue
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
// String utilities
////////////////////////////////////////////////////////////////////////////////

void kmemset(void *dest, int byte, size_t len);
bool mem_equals(void const *mem1, void const *mem2, size_t n);
void kmemcpy(void *__restrict__ dest, void const *__restrict__ src, size_t len);
void str_copy(
        char *__restrict__ dest, size_t dest_size, char const *__restrict__ src
);
bool str_equals_up_to(char const *str1, char const *str2, size_t n);
bool str_equals(char const *str1, char const *str2);
size_t kstrlen(char const *str);

////////////////////////////////////////////////////////////////////////////////
// Platform detection macros
////////////////////////////////////////////////////////////////////////////////

#ifdef __x86_64__
#define PLATFORM_IS_X86
#else
#error Unknown processor architecture
#endif