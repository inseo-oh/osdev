// SPDX-FileCopyrightText: (c) 2024 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#pragma once
#include <stddef.h>
#include <stdint.h>

typedef uint32_t htmlchar32_t;

typedef enum {
        HTMLTOKEN_KIND_TAG,
} htmltoken_kind_t;

struct HTMLTokenHeader {
        htmltoken_kind_t kind;
};

#define HTMLTOKEN_TAG_FLAG_END_TAG (1 << 0) // End tag

struct HTMLToken_Tag {
        struct HTMLTokenHeader header;
        htmlchar32_t *name;
        uint8_t name_len;
        uint8_t flags;
};

struct HTMLTokenizer;

typedef void(tokenizer_next_state_fn_t)(struct HTMLTokenizer *state);

struct HTMLTokenizer {
        htmlchar32_t const *next_input_character;
        htmlchar32_t const *eof_position;
        size_t input_len;
        // Tokenizing is done when this field is NULL.
        tokenizer_next_state_fn_t *next_state_fn;
        struct HTMLTokenHeader *current_token;
};

struct HTMLTokenizer tokenizer_new(htmlchar32_t const *src, size_t len);
void htmltokenizer_run(struct HTMLTokenizer *state);
