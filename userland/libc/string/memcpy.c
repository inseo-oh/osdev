#include "string.h"

void *memcpy(void *restrict s1, void const *restrict s2, size_t n) {
#ifdef __x86_64__
         unsigned dummy_output;
        __asm__ volatile(
                "cld\n"
                "rep movsb\n"
                : "=c"(dummy_output),
                  "=S"(dummy_output),
                  "=D"(dummy_output)
                : "c" (n),
                  "S" (s1),
                  "D" (s2)
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
        if (n < copy_len) {
                copy_len = n;
        }
        {
                uint8_t const *next_src = (uint8_t const *)src;
                uint8_t *next_dest;
                for (next_dest = dest; copy_len;
                     *(next_dest++) = *(next_src++), --copy_len, --len) {}
                s1 = next_src;
                s2 = next_dest;
        }
        {
                uint64_t const *next_src = (uint64_t const *)src;
                uint64_t *next_dest;
                for (next_dest = dest; sizeof(*next_src) <= len;
                     *(next_dest++) = *(next_src++), len -= sizeof(*next_src)) {
                }
                s1 = next_src;
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
        return s1;
}
