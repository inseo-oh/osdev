// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "_internal.h"
#include "kernel/heap/heap.h"
#include "kernel/kernel.h"
#include "kernel/tasks/tasks.h"
#include <stdbool.h>
#include <stdint.h>

static char const *LOG_TAG = "ioapic";

uint32_t ioapic_read(struct IOAPIC volatile *apic, uint8_t reg) {
        apic->address = reg;
        return apic->data;
}

void ioapic_write(struct IOAPIC volatile *apic, uint8_t reg, uint32_t val) {
        apic->address = reg;
        apic->data = val;
}

////////////////////////////////////////////////////////////////////////////////

static struct IOAPIC_Descriptor *s_ioapics;
static uint8_t s_ioapic_count;

static uint8_t get_ioapic_count() {
        struct MADT_EntryIter iter = madt_new_iter();
        unsigned count = 0;
        union MADT_Entry entry;
        while (madt_entry_next(&entry, &iter)) {
                madt_entry_type_t type = entry.common.type;
                if (type == MADT_ENTRY_IOAPIC) {
                        ++count;
                }
        }
        return count;
}

static void collect_and_init_ioapics() {
        s_ioapic_count = get_ioapic_count();
        s_ioapics = kmalloc(s_ioapic_count * sizeof(struct IOAPIC_Descriptor));
        if (!s_ioapics) {
                panic("OOM");
        }
        struct MADT_EntryIter iter = madt_new_iter();
        struct MADT_Entry_IOAPIC entry;
        unsigned index = 0;
        uint8_t irq_base = IOAPIC_IRQ_VECTOR_BASE;
        while (madt_entry_next_of_type(&entry, &iter, MADT_ENTRY_IOAPIC)) {
                s_ioapics[index].gsi_min = entry.gsi_base;
                s_ioapics[index].apic =
                        (struct IOAPIC volatile *)process_map_pages(
                                process_kernel(),
                                entry.ioapic_addr,
                                1,
                                (struct Proc_MapOptions
                                ){.writable = true, .executable = false}
                        );
                if (!s_ioapics[index].apic) {
                        panic("Couldn't map I/O APIC");
                }
                uint32_t ver_and_entry_count = ioapic_read(
                        s_ioapics[index].apic, IOAPIC_REG_IOAPICVER
                );
                uint32_t gsi_count =
                        ((ver_and_entry_count & (0xFF << 16)) >> 16) + 1;
                s_ioapics[index].gsi_max =
                        s_ioapics[index].gsi_min + gsi_count - 1;
                s_ioapics[index].irq_base = irq_base;
                LOGI(LOG_TAG,
                     "Mapping GSI range %u~%u -> INT %u~%u",
                     s_ioapics[index].gsi_min,
                     s_ioapics[index].gsi_max,
                     irq_base,
                     irq_base + gsi_count);
                // Initialize redirection entries
                for (unsigned i = 0; i < gsi_count; ++i) {
                        uint32_t entry_l = irq_base + i;
                        ASSERT(entry_l < 256);

                        entry_l |= IOAPIC_IOREDTBL_FLAG_DELMOD_NORMAL |
                                   IOAPIC_IOREDTBL_FLAG_INTERRUPT_MASK;
                        uint8_t reg_base = IOAPIC_REG_IOREDTBL_START + (i * 2);
                        ioapic_write(s_ioapics[index].apic, reg_base, entry_l);
                        ioapic_write(s_ioapics[index].apic, reg_base + 1, 0);
                }
        }
}

static void setup_ioapic_redirections() {
        struct MADT_EntryIter iter = madt_new_iter();
        struct MADT_Entry_IOAPIC_InterruptSourceOverride entry;
        while (madt_entry_next_of_type(
                &entry, &iter, MADT_ENTRY_IOAPIC_INTERRUPT_SOURCE_OVERRIDE
        )) {
                struct IOAPIC_Descriptor const *apic_desc =
                        ioapic_for_gsi(entry.gsi);
                uint8_t reg_index = ((entry.gsi - apic_desc->gsi_min) * 2) +
                                    IOAPIC_REG_IOREDTBL_START;
                uint32_t old_reg_value =
                        ioapic_read(apic_desc->apic, reg_index);
                uint8_t int_number = old_reg_value & 0xFF;
                uint32_t reg_value = int_number |
                                     IOAPIC_IOREDTBL_FLAG_DELMOD_NORMAL |
                                     IOAPIC_IOREDTBL_FLAG_DESTMOD_PHYSICAL;
                if (entry.flags & MADT_INT_FLAG_ACTIVE_LOW) {
                        reg_value |= IOAPIC_IOREDTBL_FLAG_INTPOL_LOW;
                } else {
                        reg_value |= IOAPIC_IOREDTBL_FLAG_INTPOL_HIGH;
                }
                if (entry.flags & MADT_INT_FLAG_LEVEL_TRIGGER) {
                        reg_value |= IOAPIC_IOREDTBL_FLAG_TRIGGER_LEVEL;
                } else {
                        reg_value |= IOAPIC_IOREDTBL_FLAG_TRIGGER_EDGE;
                }
                LOGI(LOG_TAG,
                     "Setup GSI %u(INT %u) as %s, %s",
                     entry.gsi,
                     int_number,
                     (entry.flags & MADT_INT_FLAG_ACTIVE_LOW) ? "Low-active"
                                                              : "High-active",
                     (entry.flags & MADT_INT_FLAG_LEVEL_TRIGGER)
                             ? "Level-trigger"
                             : "Edge-trigger");
                ioapic_write(apic_desc->apic, reg_index, reg_value);
        }
}

bool ioapic_legacy_irq_to_gsi(uint32_t *out, uint8_t irq) {
        if (!g_madt) {
                return false;
        }
        struct MADT_EntryIter iter = madt_new_iter();
        struct MADT_Entry_IOAPIC_InterruptSourceOverride entry;
        while (madt_entry_next_of_type(
                &entry, &iter, MADT_ENTRY_IOAPIC_INTERRUPT_SOURCE_OVERRIDE
        )) {
                if (entry.irq_source == irq) {
                        *out = entry.gsi;
                        return true;
                }
        }
        return false;
}

struct IOAPIC_Descriptor const *ioapic_for_gsi(uint32_t gsi) {
        if (!g_madt) {
                panic("MADT is not ready");
        }
        for (unsigned i = 0; i < s_ioapic_count; ++i) {
                if ((s_ioapics[i].gsi_min <= gsi) &&
                    (gsi <= s_ioapics[i].gsi_max)) {
                        return &s_ioapics[i];
                }
        }
        panic("GSI %u is not part of any I/O APICs", gsi);
}

void ioapic_init() {
        collect_and_init_ioapics();
        setup_ioapic_redirections();
}