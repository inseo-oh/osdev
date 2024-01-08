// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "kernel.h"
#include <kernel/tasks/tasks.h>
#include <kernel/utility/utility.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static char const *LOG_TAG = "acpi";

#define LOG_TABLE_ARGS(_hdr, _fmt)                                      \
        "[%c%c%c%c] " _fmt, (_hdr)->Signature[0], (_hdr)->Signature[1], \
                (_hdr)->Signature[2], (_hdr)->Signature[3]

struct RSDT {
        struct ACPI_SDTHeader Header;
        uint32_t Entry[];
} PACKED;

struct XSDT {
        struct ACPI_SDTHeader Header;
        uint64_t Entry[];
} PACKED;

static struct XSDT *s_xsdt = NULL;
static struct RSDT *s_rsdt = NULL;
static struct ACPI_SDTHeader *s_madt = NULL;

static bool check_rsdp_signature(struct ACPI_RSDP const *rsdp) {
        return mem_equals(rsdp->Signature, "RSD PTR", 6);
}

static bool rsdp_checksum_rev0(struct ACPI_RSDP const *rsdp) {
        uint8_t *bytes = (uint8_t *)rsdp;
        uint8_t sum = 0;
        for (unsigned i = 0; i < 20; ++i) {
                sum += bytes[i];
        }
        return sum == 0;
}

static bool rsdp_checksum_rev2(struct ACPI_RSDP const *rsdp) {
        ASSERT(2 <= rsdp->Revision);
        if (rsdp->Length < sizeof(struct ACPI_RSDP)) {
                panic("Bad ACPI RSDP length %u", rsdp->Length);
        }
        uint8_t *bytes = (uint8_t *)rsdp;
        uint8_t sum = 0;
        for (unsigned i = 0; i < rsdp->Length; ++i) {
                sum += bytes[i];
        }
        return sum == 0;
}

static bool rsdp_checksum(struct ACPI_RSDP *rsdp) {
        if (!rsdp_checksum_rev0(rsdp)) {
                return false;
        }
        if (2 <= rsdp->Revision) {
                if (!rsdp_checksum_rev2(rsdp)) {
                        return false;
                }
        }
        return true;
}

static bool validate_sdt(struct ACPI_SDTHeader const *hdr) {
        uint8_t const *src = (uint8_t *)hdr;
        uint8_t sum = 0;
        for (unsigned i = 0; i < hdr->Length; ++i) {
                sum += src[i];
        }
        return sum == 0;
}

static void *map_table(uintptr_t base) {
        // First we map only the SDT part of the table to figure out how large
        // it is.
        unsigned length = sizeof(struct ACPI_SDTHeader);
        struct ACPI_SDTHeader *hdr = process_map_unaligned(
                process_kernel(),
                base,
                length,
                (struct Proc_MapOptions){.writable = false, .executable = false}
        );
        if (!hdr) {
                goto fail;
        }
        length = hdr->Length;
        process_unmap_unaligned(
                process_kernel(), hdr, sizeof(struct ACPI_SDTHeader)
        );
        // Then perform real mapping using the size we've got.
        hdr = process_map_unaligned(
                process_kernel(),
                base,
                length,
                (struct Proc_MapOptions){.writable = false, .executable = false}
        );
        if (!hdr) {
                goto fail;
        }
        return hdr;
fail:
        if (hdr) {
                process_unmap_unaligned(process_kernel(), hdr, length);
        }
        return NULL;
}

static void unmap_table(struct ACPI_SDTHeader *hdr) {
        process_unmap_unaligned(process_kernel(), hdr, hdr->Length);
}

static void *try_table(uintptr_t base, char const *signature) {
        struct ACPI_SDTHeader *hdr = map_table(base);
        if (!hdr) {
                LOGE(LOG_TAG, "ACPI table @ %p map failed", base);
                goto fail;
        }
        if (!mem_equals(hdr->Signature, signature, 4)) {
                goto fail;
        }
        if (!validate_sdt(hdr)) {
                LOGE(LOG_TAG,
                     LOG_TABLE_ARGS(
                             hdr, "Ignoring ACPI table with bad checksum"
                     ));
                goto fail;
        }
        return hdr;
fail:
        if (hdr) {
                unmap_table(hdr);
        }
        return NULL;
}

// Returns NULL if table cannot be found.
static void *locate_table_xsdt(char const *signature) {
        ASSERT(s_xsdt);
        unsigned count = (s_xsdt->Header.Length - sizeof(s_xsdt->Header)) / 8;
        for (unsigned i = 0; i < count; ++i) {
                uintptr_t base = s_xsdt->Entry[i];
                void *result = try_table(base, signature);
                if (result) {
                        return result;
                }
        }
        return NULL;
}

// Returns NULL if table cannot be found.
static void *locate_table_rsdt(char const *signature) {
        ASSERT(s_rsdt);
        unsigned count = (s_rsdt->Header.Length - sizeof(s_rsdt->Header)) / 8;
        for (unsigned i = 0; i < count; ++i) {
                uintptr_t base = s_rsdt->Entry[i];
                void *result = try_table(base, signature);
                if (result) {
                        return result;
                }
        }
        return NULL;
}

// Returns NULL if table cannot be found.
void *acpi_locate_table(char const *signature) {
        if (s_xsdt) {
                return locate_table_xsdt(signature);
        }
        if (s_rsdt) {
                return locate_table_rsdt(signature);
        }
        return NULL;
}

void acpi_load_root_sdt(struct ACPI_RSDP *rsdp) {
        if (!check_rsdp_signature(rsdp)) {
                panic("RSDP signature check failed");
        }
        if (!rsdp_checksum(rsdp)) {
                panic("Bad RSDP checksum");
        }
        if (2 <= rsdp->Revision) {
                s_xsdt = map_table(rsdp->XsdtAddress);
        } else {
                s_rsdt = map_table(rsdp->RsdtAddress);
        }
        s_madt = acpi_locate_table("APIC");
}
