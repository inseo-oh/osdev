// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "_internal.h"
#include <kernel/kernel.h>
#include <kernel/utility/utility.h>
#include <stdbool.h>
#include <stddef.h>

struct MADT *g_madt = NULL;

void madt_init(struct MADT *madt) { g_madt = madt; }

struct MADT_EntryIter madt_new_iter(void) {
        return (struct MADT_EntryIter){
                .next_byte_index = 0,
                .byte_count =
                        g_madt->Header.Length - offsetof(struct MADT, entries),
        };
}

bool madt_entry_next(union MADT_Entry *out, struct MADT_EntryIter *iter) {
        if (iter->byte_count <= iter->next_byte_index) {
                return false;
        }
        void const *src = &g_madt->entries[iter->next_byte_index];
        kmemcpy(&out->common, src, sizeof(out->common));
        size_t len = out->common.len;
        // TODO(OIS): We could only copy rest of data, rather than re-copying
        //            the whole thing.
        size_t copy_len =
                sizeof(union MADT_Entry) < len ? sizeof(union MADT_Entry) : len;
        kmemcpy(out, src, copy_len);
        iter->next_byte_index += len;
        return true;
}

bool madt_entry_next_of_type(
        void *out, struct MADT_EntryIter *iter, madt_entry_type_t type
) {
        size_t len;
        for (; iter->next_byte_index < iter->byte_count;
             iter->next_byte_index += len) {
                void const *src = &g_madt->entries[iter->next_byte_index];
                struct MADT_EntryHeader hdr;
                kmemcpy(&hdr, src, sizeof(hdr));
                len = hdr.len;
                ASSERT(len);
                if (hdr.type != type) {
                        continue;
                }
                // TODO(OIS): We could only copy rest of data, rather than
                //            re-copying the whole thing.
                size_t copy_len = sizeof(union MADT_Entry) < len
                                          ? sizeof(union MADT_Entry)
                                          : len;
                kmemcpy(out, src, copy_len);
                iter->next_byte_index += len;
                return true;
        }
        return false;
}
