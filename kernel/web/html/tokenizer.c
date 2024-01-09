// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "html.h"
#include <kernel/heap/heap.h>
#include <kernel/kernel.h>
#include <kernel/utility/utility.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static char const *LOG_TAG = "html-tokenizer";

// https://html.spec.whatwg.org/multipage/parsing.html#tokenization

static tokenizer_next_state_fn_t run_data_state, tag_open_state,
        end_tag_open_state, tag_name_state;

static void emit_eof(struct HTMLTokenizer *state) {
        (void)state;
        state->next_state_fn = NULL; // End of tokenization
        LOGI(LOG_TAG, "emit eof");
        // TODO: Emit the token to somewhere actually
}

static bool is_eof(struct HTMLTokenizer *state) {
        return state->next_input_character == state->eof_position;
}

// https://html.spec.whatwg.org/multipage/parsing.html#data-state
static void run_data_state(struct HTMLTokenizer *state) {
        if (is_eof(state)) {
                emit_eof(state);
                return;
        }
        htmlchar32_t chr = *state->next_input_character;
        switch (chr) {
        case '&':
                ++state->next_input_character;
                TODO();
        case '<':
                ++state->next_input_character;
                state->next_state_fn = tag_open_state;
                break;
        case '\0':
                ++state->next_input_character;
                TODO();
        default:
                ++state->next_input_character;
                TODO();
        }
}

WARN_UNUSED_RESULT static bool
create_tag_token(struct HTMLTokenizer *state, uint8_t flags) {
        struct HTMLToken_Tag *tok = vmmalloc(sizeof(struct HTMLToken_Tag));
        if (!tok) {
                goto oom;
        }
        tok->header.kind = HTMLTOKEN_KIND_TAG;
        tok->name = NULL;
        tok->name_len = 0;
        tok->flags = flags;
        state->current_token = &tok->header;
        return true;
oom:
        if (tok) {
                vmfree(tok);
        }
        return false;
}

WARN_UNUSED_RESULT static bool
create_start_tag_token(struct HTMLTokenizer *state) {
        return create_tag_token(state, 0);
}

WARN_UNUSED_RESULT static bool create_end_tag_token(struct HTMLTokenizer *state
) {
        return create_tag_token(state, HTMLTOKEN_TAG_FLAG_END_TAG);
}

WARN_UNUSED_RESULT static bool
append_tag_token_char(struct HTMLTokenizer *state, htmlchar32_t chr) {
        ASSERT(state->current_token &&
               (state->current_token->kind == HTMLTOKEN_KIND_TAG));
        struct HTMLToken_Tag *tok =
                (struct HTMLToken_Tag *)state->current_token;
        ++tok->name_len;
        if (!tok->name_len) {
                // Integer overflow -> Tag name is too long.
                return false;
        }
        htmlchar32_t *new = vmrealloc(tok->name, tok->name_len * sizeof(*new));
        if (!new) {
                return false;
        }
        new[tok->name_len - 1] = chr;
        tok->name = new;
        return true;
}

// Returned string must be freed using vmfree()
static char *htmlstring32_as_ascii(htmlchar32_t const *str, size_t len) {
        char *dest = vmmalloc(len * sizeof(char) + 1);
        if (!dest) {
                panic("OOM");
        }
        for (size_t i = 0; i < len; ++i) {
                dest[i] = (char)str[i];
        }
        dest[len] = '\0';
        return dest;
}

static void emit_current_tag_token(struct HTMLTokenizer *state) {
        ASSERT(state->current_token &&
               (state->current_token->kind == HTMLTOKEN_KIND_TAG));
        struct HTMLToken_Tag *tok =
                (struct HTMLToken_Tag *)state->current_token;
        LOGI(LOG_TAG,
             "emit tag <%s%s>",
             (tok->flags & HTMLTOKEN_TAG_FLAG_END_TAG) ? "/" : "",
             htmlstring32_as_ascii(tok->name, tok->name_len));
        // TODO: Emit the token to somewhere actually
}

// https://html.spec.whatwg.org/multipage/parsing.html#tag-open-state
static void tag_open_state(struct HTMLTokenizer *state) {
        if (is_eof(state)) {
                TODO();
        }
        htmlchar32_t chr = *state->next_input_character;
        switch (chr) {
        case '!':
                ++state->next_input_character;
                TODO();
        case '/':
                ++state->next_input_character;
                state->next_state_fn = end_tag_open_state;
                break;
        case '?':
                ++state->next_input_character;
                TODO();
        default:
                if (is_ascii_alpha(chr)) {
                        if (!create_start_tag_token(state)) {
                                TODO_HANDLE_ERROR();
                        }
                        state->next_state_fn = tag_name_state;
                } else {
                        ++state->next_input_character;
                        TODO();
                }
                break;
        }
}

// https://html.spec.whatwg.org/multipage/parsing.html#end-tag-open-state
static void end_tag_open_state(struct HTMLTokenizer *state) {
        if (is_eof(state)) {
                TODO();
        }
        htmlchar32_t chr = *state->next_input_character;
        switch (chr) {
        case '>':
                ++state->next_input_character;
                TODO();
        default:
                if (is_ascii_alpha(chr)) {
                        if (!create_end_tag_token(state)) {
                                TODO_HANDLE_ERROR();
                        }
                        state->next_state_fn = tag_name_state;
                } else {
                        ++state->next_input_character;
                        TODO();
                }
                break;
        }
}

// https://html.spec.whatwg.org/multipage/parsing.html#tag-name-state
static void tag_name_state(struct HTMLTokenizer *state) {
        if (is_eof(state)) {
                TODO();
        }
        htmlchar32_t chr = *state->next_input_character;
        switch (chr) {
        case '\t':
        case '\n':
        case '\x0c': // FF
        case ' ':
                ++state->next_input_character;
                TODO();
        case '/':
                ++state->next_input_character;
                TODO();
        case '>':
                ++state->next_input_character;
                emit_current_tag_token(state);
                state->next_state_fn = run_data_state;
                break;
        case '\0':
                ++state->next_input_character;
                TODO();
        default:
                ++state->next_input_character;
                if (!append_tag_token_char(state, chr)) {
                        TODO_HANDLE_ERROR();
                }
                break;
        }
}

struct HTMLTokenizer tokenizer_new(htmlchar32_t const *src, size_t len) {
        return (struct HTMLTokenizer){
                .next_input_character = src,
                .eof_position = src + len,
                .input_len = len,
                .next_state_fn = run_data_state,
                .current_token = NULL,
        };
}

void htmltokenizer_run(struct HTMLTokenizer *state) {
        while (state->next_state_fn) {
                state->next_state_fn(state);
        }
}