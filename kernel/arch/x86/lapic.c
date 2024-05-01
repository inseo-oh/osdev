// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "_internal.h"
#include "kernel/arch/arch.h"
#include "kernel/heap/heap.h"
#include "kernel/kernel.h"
#include "kernel/tasks/tasks.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static char const *LOG_TAG = "lapic";

static uint32_t s_timer_calibration_value;
static bool s_timer_calibrated = false;

// As far as I know, Local APIC address is the same on every processor, and thus
// only current processor's APIC can be accessed. (It does mean that if we are
// on SMP system, AP's LAPIC must be configured by AP themselves)
static void *s_lapic_base;

uint32_t lapic_read_unchecked(uint32_t reg) {
        return *(uint32_t volatile *)(s_lapic_base + reg);
}

void lapic_write_unchecked(uint32_t reg, uint32_t val) {
        *(uint32_t volatile *)(s_lapic_base + reg) = val;
}

uint8_t lapic_read_esr(void) {
        return lapic_read_unchecked(LAPIC_REG_ESR) & ~LAPIC_ESR_RESERVED_MASK;
}

void lapic_reset_esr(void) { lapic_write_unchecked(LAPIC_REG_ESR, 0); }

uint32_t lapic_read(uint32_t reg) {
        lapic_reset_esr();
        uint32_t result = lapic_read_unchecked(reg);
        {
                lapic_reset_esr();
                uint32_t esr = lapic_read_esr();
                if (esr) {
                        LOGE(LOG_TAG, "APIC register read error: %x", esr);
                }
        }
        return result;
}

void lapic_write(uint32_t reg, uint32_t val, uint32_t reserved_mask) {
        lapic_reset_esr();
        ASSERT(!(val & reserved_mask));
        uint32_t reserved_bits = lapic_read(reg) & reserved_mask;
        lapic_reset_esr();
        lapic_write_unchecked(reg, val | reserved_bits);
        {
                uint32_t esr = lapic_read_esr();
                if (esr) {
                        LOGE(LOG_TAG,
                             "APIC register write error: %x R:%x",
                             esr,
                             reg);
                }
        }
}

void lapic_send_ipi(uint8_t target_apic_id, uint32_t flags) {
        ASSERT(target_apic_id < 16);

        lapic_reset_esr();
        lapic_write(LAPIC_REG_ICRH, target_apic_id << 24, LAPIC_ICRH_MASK);
        lapic_write(LAPIC_REG_ICRL, flags, LAPIC_ICRL_MASK);
        // Wait for IPI to finish
        while (1) {
                if (!(lapic_read(LAPIC_REG_ICRL) & (1 << 12))) {
                        break;
                }
                processor_wait_during_spinloop();
        }
}

void lapic_send_eoi(void) { lapic_write_unchecked(LAPIC_REG_EOI, 0); }

void lapic_set_base(void *base) { s_lapic_base = base; }

void lapic_enable(void) {
        lapic_write(
                LAPIC_REG_SVR,
                lapic_read(LAPIC_REG_SVR) | LAPIC_SVR_FLAG_APIC_SOFTWARE_ELABLE,
                LAPIC_SVR_REG_RESERVED_MASK
        );
}

void lapic_timer_reset_to_1ms(void) {
        if (!s_timer_calibrated) {
                lapic_write(
                        LAPIC_REG_TIMER_DIVIDE_CONFIG,
                        0x3,
                        LAPIC_DIVIDE_CONFIG_REG_MASK
                );
                lapic_write(
                        LAPIC_REG_LVT_TIMER,
                        LAPIC_LVT_TIMER_FLAG_MODE_PERIODIC |
                                LAPIC_LVT_FLAG_MASKED | LAPIC_TIMER_VECTOR,
                        LAPIC_LVT_TIMER_REG_RESERVED_MASK
                );
                lapic_write_unchecked(
                        LAPIC_REG_TIMER_INITIAL_COUNT, 0xffffffff
                );
                uint32_t time1 =
                        lapic_read_unchecked(LAPIC_REG_TIMER_CURRENT_COUNT);
                for (unsigned i = 0; i < 10; ++i) {
                        i8254timer_oneshot_count(10);
                }
                uint32_t time2 =
                        lapic_read_unchecked(LAPIC_REG_TIMER_CURRENT_COUNT);
                uint32_t diff = time1 - time2;
                s_timer_calibration_value = diff / 100;
                s_timer_calibrated = true;
        }
        lapic_write(
                LAPIC_REG_LVT_TIMER,
                LAPIC_LVT_TIMER_FLAG_MODE_PERIODIC | LAPIC_TIMER_VECTOR,
                LAPIC_LVT_TIMER_REG_RESERVED_MASK
        );
        lapic_write(
                LAPIC_REG_TIMER_DIVIDE_CONFIG, 0x3, LAPIC_DIVIDE_CONFIG_REG_MASK
        );
        lapic_write(
                LAPIC_REG_TIMER_INITIAL_COUNT, s_timer_calibration_value, 0
        );
}

////////////////////////////////////////////////////////////////////////////////

static bool is_usable_processor(struct MADT_Entry_LAPIC const *apic) {
        return apic->flags & (MADT_LAPIC_FLAG_PROCESSOR_ENABLED |
                              MADT_LAPIC_FLAG_ONLINE_CAPABLE);
}

static unsigned get_usable_processor_count() {
        struct MADT_EntryIter iter = madt_new_iter();
        unsigned count = 0;
        struct MADT_Entry_LAPIC entry;
        while (madt_entry_next_of_type(&entry, &iter, MADT_ENTRY_LAPIC)) {
                if (is_usable_processor(&entry)) {
                        ++count;
                }
        }
        return count;
}

static struct LAPIC_Descriptor *s_lapic_descriptors;
static uint8_t s_lapic_count;

static struct LAPIC_Descriptor *find_lapic_using_apic_id(uint32_t lapic_id) {
        for (unsigned i = 0; i < s_lapic_count; ++i) {
                if (s_lapic_descriptors[i].apic_id == lapic_id) {
                        return &s_lapic_descriptors[i];
                }
        }
        panic("Local APIC ID %u is missing from Local APIC list!", lapic_id);
}

static struct LAPIC_Descriptor *
find_lapic_using_acpi_processor_id(uint32_t procesor_id) {
        for (unsigned i = 0; i < s_lapic_count; ++i) {
                if (s_lapic_descriptors[i].acpi_processor_id == procesor_id) {
                        return &s_lapic_descriptors[i];
                }
        }
        return NULL;
}

static void *map_lapic() {
        return process_map_pages(
                process_kernel(),
                g_madt->LocalInterruptControllerAddress,
                1,
                (struct Proc_MapOptions){.writable = true, .executable = false}
        );
}

static void collect_lapics() {
        s_lapic_count = get_usable_processor_count();
        s_lapic_descriptors =
                kmalloc(s_lapic_count * sizeof(struct LAPIC_Descriptor));
        if (!s_lapic_descriptors) {
                panic("OOM");
        }
        struct MADT_EntryIter iter = madt_new_iter();
        struct MADT_Entry_LAPIC entry;
        unsigned index = 0;
        while (madt_entry_next_of_type(&entry, &iter, MADT_ENTRY_LAPIC)) {
                if (!is_usable_processor(&entry)) {
                        continue;
                }
                LOGI(LOG_TAG,
                     "Found ACPI processor %u (LAPIC ID %u)",
                     entry.acpi_processor_id,
                     entry.apic_id);
                s_lapic_descriptors[index].acpi_processor_id =
                        entry.acpi_processor_id;
                s_lapic_descriptors[index].apic_id = entry.apic_id;
                s_lapic_descriptors[index].flags = 0;
                s_lapic_descriptors[index].nmi_info[0].valid = false;
                s_lapic_descriptors[index].nmi_info[1].valid = false;
                ++index;
        }
}

static void collect_lapic_nmis() {
        struct MADT_EntryIter iter = madt_new_iter();
        union MADT_Entry entry;
        while (madt_entry_next(&entry, &iter)) {
                if (entry.common.type != MADT_ENTRY_LAPIC_NMI) {
                        continue;
                }
                struct MADT_Entry_LAPIC_NMI *nmi_entry = &entry.lapic_nmi;
                if (1 < nmi_entry->lint) {
                        LOGE(LOG_TAG,
                             "Local APIC NMI setup - Invalid LINT# %u. "
                             "Ignoring...",
                             nmi_entry->lint);
                }
                if (nmi_entry->acpi_processor_id == 0xFF) {
                        for (unsigned i = 0; i < s_lapic_count; ++i) {
                                struct LAPIC_Descriptor *lapic =
                                        &s_lapic_descriptors[i];
                                lapic->nmi_info[nmi_entry->lint].valid = true;
                                lapic->nmi_info[nmi_entry->lint].flags =
                                        nmi_entry->flags;
                        }
                } else {
                        struct LAPIC_Descriptor *lapic =
                                find_lapic_using_acpi_processor_id(
                                        nmi_entry->acpi_processor_id
                                );
                        if (lapic) {
                                lapic->nmi_info[nmi_entry->lint].valid = true;
                                lapic->nmi_info[nmi_entry->lint].flags =
                                        nmi_entry->flags;
                        } else {
                                LOGE(LOG_TAG,
                                     "Local APIC NMI setup - ACPI processor %u "
                                     "does not exist. "
                                     "Ignoring...",
                                     nmi_entry->acpi_processor_id);
                        }
                }
        }
}

static uint32_t madt_interrupt_flags_to_lint_flags(uint32_t madt_flags) {
        uint32_t result = 0;
        if (madt_flags & MADT_INT_FLAG_ACTIVE_LOW) {
                result |= LAPIC_LVT_FLAG_LOW_TRIGGERED;
        } else {
                result |= LAPIC_LVT_FLAG_HIGH_TRIGGERED;
        }
        if (madt_flags & MADT_INT_FLAG_LEVEL_TRIGGER) {
                result |= LAPIC_LVT_FLAG_LEVEL_TRIGGERED;
        } else {
                result |= LAPIC_LVT_FLAG_EDGE_TRIGGERED;
        }
        return result;
}

#define IA32_APIC_BASE_FLAG_APIC_GLOBAL_ENABLE (1 << 11)

static void init_current_lapic() {
        uint8_t lapic_id = lapic_id_for_current_processor();

        uint32_t reg_value = rdmsr(MSR_IA32_APIC_BASE);
        reg_value |= IA32_APIC_BASE_FLAG_APIC_GLOBAL_ENABLE;
        wrmsr(MSR_IA32_APIC_BASE, reg_value);

        struct LAPIC_Descriptor const *apic = lapic_for_current_processor();

        // Initialize LVT
        lapic_write(
                LAPIC_REG_LVT_THERMAL_SENSOR,
                LAPIC_LVT_FLAG_DELIVERY_MODE_FIXED |
                        LAPIC_THERMAL_SENSOR_VECTOR,
                LAPIC_LVT_THERMAL_SENSOR_REG_RESERVED_MASK
        );
        lapic_write(
                LAPIC_REG_LVT_PERFORMANCE_MONITORING_COUNTERS,
                LAPIC_LVT_FLAG_DELIVERY_MODE_FIXED |
                        LAPIC_PERFORMENCE_COUNTER_VECTOR,
                LAPIC_LVT_PERFORMANCE_MONITORING_REG_RESERVED_MASK
        );
        lapic_write(
                LAPIC_REG_LVT_ERROR,
                LAPIC_ERROR_VECTOR,
                LAPIC_LVT_ERROR_REG_RESERVED_MASK
        );
        lapic_write(
                LAPIC_REG_LVT_LINT0,
                LAPIC_LVT_FLAG_MASKED | LAPIC_LVT_FLAG_DELIVERY_MODE_FIXED |
                        LAPIC_LINT0_VECTOR,
                LAPIC_LVT_LINT0_REG_RESERVED_MASK
        );
        lapic_write(
                LAPIC_REG_LVT_LINT1,
                LAPIC_LVT_FLAG_MASKED | LAPIC_LVT_FLAG_DELIVERY_MODE_FIXED |
                        LAPIC_LINT1_VECTOR,
                LAPIC_LVT_LINT1_REG_RESERVED_MASK
        );
        if (apic->nmi_info[0].valid) {
                LOGI(LOG_TAG, "[LAPIC %u] Configuring LINT0 as NMI", lapic_id);
                lapic_write(
                        LAPIC_REG_LVT_LINT0,
                        madt_interrupt_flags_to_lint_flags(
                                apic->nmi_info[0].flags
                        ) | LAPIC_LVT_FLAG_DELIVERY_MODE_NMI,
                        LAPIC_LVT_LINT0_REG_RESERVED_MASK
                );
        }
        if (apic->nmi_info[1].valid) {
                LOGI(LOG_TAG, "[LAPIC %u] Configuring LINT1 as NMI", lapic_id);
                lapic_write(
                        LAPIC_REG_LVT_LINT1,
                        madt_interrupt_flags_to_lint_flags(
                                apic->nmi_info[1].flags
                        ) | LAPIC_LVT_FLAG_DELIVERY_MODE_NMI,
                        LAPIC_LVT_LINT1_REG_RESERVED_MASK
                );
        }

        // FIXME: Writing to CMCI register reports error... for some reason?
#if FIXME_WRITE_TO_CMCI_REG
        lapic_write_reg(
                LAPIC_REG_LVT_CMCI,
                LAPIC_LVT_FLAG_DELIVERY_MODE_FIXED | LAPIC_CMCI_VECTOR,
                LAPIC_LVT_CMCI_REG_RESERVED_MASK
        );
#endif

        lapic_write(
                LAPIC_REG_LVT_TIMER,
                LAPIC_LVT_FLAG_MASKED | LAPIC_TIMER_VECTOR,
                LAPIC_LVT_TIMER_REG_RESERVED_MASK
        );

        // Initialize SVR
        lapic_write(
                LAPIC_REG_SVR,
                LAPIC_SPURIOUS_VECTOR,
                LAPIC_SVR_REG_RESERVED_MASK
        );
}

uint8_t lapic_id_for_current_processor(void) {
        return lapic_read(LAPIC_REG_ID) >> 24;
}

struct LAPIC_Descriptor const *lapic_for_current_processor(void) {
        return find_lapic_using_apic_id(lapic_id_for_current_processor());
}

struct LAPIC_Descriptor const *lapic_for_processor(unsigned idx) {
        ASSERT(idx < s_lapic_count);
        return &s_lapic_descriptors[idx];
}

size_t lapic_count(void) { return s_lapic_count; }

void lapic_init_for_ap(void) { init_current_lapic(); }

void lapic_init_for_bsp(void) {
        void *lapic_base = map_lapic();
        if (!lapic_base) {
                panic("LAPIC couldn't be mapped");
        }
        lapic_set_base(lapic_base);
        // Mask all PIC interrupts
        if (g_madt->Flags & 0x1) {
                LOGI(LOG_TAG, "Masking all PIC interrupts");
                for (unsigned i = 0; i < 16; ++i) {
                        i8259pic_mask_irq(i);
                }
        } else {
                LOGI(LOG_TAG, "8259 PICs doesn't appear to be there");
        }

        collect_lapics();
        collect_lapic_nmis();
        init_current_lapic();
}
