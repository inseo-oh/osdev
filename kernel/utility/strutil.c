// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "utility.h"
#include "kernel/kernel.h"
#include <stdbool.h>
#include <stdint.h>

void kmemset(void *dest, int byte, size_t len) {
        // TODO(OIS): Optimization is needed
        for (uint8_t *next_dest = (uint8_t *)dest; len != 0; *(next_dest++) = byte, --len) {}
}

// TODO: Turn this into memcmp style function
bool mem_equals(void const *mem1, void const *mem2, size_t n) {
        uint8_t const *src1 = mem1;
        uint8_t const *src2 = mem2;
        for (size_t remaining_len = n; remaining_len; --remaining_len) {
                if (*src1 != *src2) {
                        return false;
                }
                ++src1;
                ++src2;
        }
        return true;
}

void kmemcpy(
        void *__restrict__ dest, void const *__restrict__ src, size_t len
) {
#ifdef __x86_64__
         unsigned dummy_output;
        __asm__ volatile(
                "cld\n"
                "rep movsb\n"
                : "=c"(dummy_output),
                  "=S"(dummy_output),
                  "=D"(dummy_output)
                : "c" (len),
                  "S" (src),
                  "D" (dest)
                : "memory", "cc"
        );
#else
        size_t mod1 = (uintptr_t)src % 8;
        size_t copy_len = 0;
        if (mod1) {
                copy_len = 8 - mod1;
        } else {
                size_t mod2 = (uintptr_t)dest % 8;
                if (mod2) {
                        copy_len = 8 - mod2;
                }
        }
        if (len < copy_len) {
                copy_len = len;
        }
        {
                uint8_t const *next_src = (uint8_t const *)src;
                uint8_t *next_dest;
                for (next_dest = dest; copy_len;
                     *(next_dest++) = *(next_src++), --copy_len, --len) {}
                src = next_src;
                dest = next_dest;
        }
        {
                uint64_t const *next_src = (uint64_t const *)src;
                uint64_t *next_dest;
                for (next_dest = dest; sizeof(*next_src) <= len;
                     *(next_dest++) = *(next_src++), len -= sizeof(*next_src)) {
                }
                src = next_src;
                dest = next_dest;
        }
        {
                uint32_t const *next_src = (uint32_t const *)src;
                uint32_t *next_dest;
                for (next_dest = dest; sizeof(*next_src) <= len;
                     *(next_dest++) = *(next_src++), len -= sizeof(*next_src)) {
                }
                src = next_src;
                dest = next_dest;
        }
        {
                uint16_t const *next_src = (uint16_t const *)src;
                uint16_t *next_dest;
                for (next_dest = dest; sizeof(*next_src) <= len;
                     *(next_dest++) = *(next_src++), len -= sizeof(*next_src)) {
                }
                src = next_src;
                dest = next_dest;
        }
        uint8_t const *next_src = (uint8_t const *)src;
        uint8_t *next_dest;
        for (next_dest = dest; sizeof(*next_src) <= len;
             *(next_dest++) = *(next_src++), len -= sizeof(*next_src)) {}
#endif
}

// TODO: Turn this into strcpy style function
void str_copy(
        char *__restrict__ dest, size_t dest_size, char const *__restrict__ src
) {
        size_t remaining_size = dest_size - 1;
        char const *next_src = src;
        for (char *next_dest = dest;;
             ++next_dest, ++next_src, --remaining_size) {
                if (remaining_size == 0) {
                        panic("Given string %s is too long to copy (dest_size "
                              "is %u)",
                              src,
                              dest_size);
                }
                next_dest[0] = *next_src;
                if (*next_src == '\0') {
                        next_dest[1] = '\0';
                        break;
                }
        }
}

// TODO: Turn this into strncmp style function
bool str_equals_up_to(char const *str1, char const *str2, size_t n) {
        char const *src1 = str1;
        char const *src2 = str2;
        for (size_t remaining_len = n; remaining_len; --remaining_len) {
                if (*src1 != *src2) {
                        return false;
                }
                if (!*src1) {
                        break;
                }
                ++src1;
                ++src2;
        }
        return true;
}

// TODO: Turn this into strcmp style function
bool str_equals(char const *str1, char const *str2) {
        char const *src1 = str1;
        char const *src2 = str2;
        while (1) {
                if (*src1 != *src2) {
                        return false;
                }
                if (!*src1) {
                        break;
                }
                ++src1;
                ++src2;
        }
        return true;
}

size_t kstrlen(char const *str) {
        size_t len = 0;
        for (char const *src = str; *src; ++src, ++len) {}
        return len;
}


char *kstrchr(char const *s, int c) {
        while (1) {
                if (*s == c) {
                        return (char *)s;
                }
                if (!*s) {
                        return NULL;
                }
                s++;
        }
}

