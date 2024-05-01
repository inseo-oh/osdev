// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#pragma once
#include "kernel/kernel.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

////////////////////////////////////////////////////////////////////////////////
// IDT
////////////////////////////////////////////////////////////////////////////////

void idt_init_bsp(void);
void idt_init_ap(void);
void idt_use_ist1(void);

////////////////////////////////////////////////////////////////////////////////
// Processor
////////////////////////////////////////////////////////////////////////////////

#define GDT_KERNEL_CS_INDEX 1
#define GDT_KERNEL_DS_INDEX 2
// DS and CS order is swapped for userland(DS comes first), because of how
// SYSCALL/SYSRET works.
#define GDT_USER_DS_INDEX 3
#define GDT_USER_CS_INDEX 4
#define X86_TSS_INDEX     5

// RPL(Requested Privilege Level)s
#define X86_RPL_KERNEL 0
#define X86_RPL_USER   3

#define X86_ENTRY_SIZE          8UL
#define X86_SEGDESC_OFFSET(__n) ((__n) * X86_ENTRY_SIZE)

#define GDT_KERNEL_CS_OFFSET X86_SEGDESC_OFFSET(GDT_KERNEL_CS_INDEX)
#define GDT_KERNEL_DS_OFFSET X86_SEGDESC_OFFSET(GDT_KERNEL_DS_INDEX)
#define GDT_USER_CS_OFFSET   X86_SEGDESC_OFFSET(GDT_USER_CS_INDEX)
#define GDT_USER_DS_OFFSET   X86_SEGDESC_OFFSET(GDT_USER_DS_INDEX)
#define X86_TSS_OFFSET       X86_SEGDESC_OFFSET(X86_TSS_INDEX)

#define GDT_KERNEL_CS (GDT_KERNEL_CS_OFFSET | X86_RPL_KERNEL)
#define GDT_KERNEL_DS (GDT_KERNEL_DS_OFFSET | X86_RPL_KERNEL)
#define GDT_USER_CS   (GDT_USER_CS_OFFSET | X86_RPL_USER)
#define GDT_USER_DS   (GDT_USER_DS_OFFSET | X86_RPL_USER)

#define RFLAGS_IF (1 << 9)

void processor_process_ipimessages(void);
// Valid reason must be set in Processor_LocalState's flags field of other
// processors before IPI is sent.
void processor_broadcast_ipi_to_others(void);
void processor_flush_other_processors_tlb(void);
void processor_flush_other_processors_tlb_for(void *vaddr);
;

////////////////////////////////////////////////////////////////////////////////
// MSR
////////////////////////////////////////////////////////////////////////////////

typedef uint32_t x86_msr_t;

#define MSR_IA32_APIC_BASE 0x0000001B
#define MSR_IA32_EFER      0xC0000080
#define MSR_STAR           0xC0000081
#define MSR_LSTAR          0xC0000082
#define MSR_SFMASK         0xC0000084
#define MSR_FS_BASE        0xC0000100
#define MSR_GS_BASE        0xC0000101
#define MSR_KERNEL_GS_BASE 0xC0000102

#define MSR_IA32_EFER_SCE (1 << 0)
#define MSR_IA32_EFER_NXE (1 << 11)

static inline uint64_t rdmsr(x86_msr_t msr) {
        uint32_t lsb;
        uint32_t msb;
        __asm__ volatile("rdmsr" : "=a"(lsb), "=d"(msb) : "c"(msr));
        return ((uint64_t)msb << 32) | (uint64_t)lsb;
}

static inline void wrmsr(x86_msr_t msr, uint64_t value) {
        uint32_t lsb = value;
        uint32_t msb = value >> 32;
        __asm__ volatile("wrmsr" ::"a"(lsb), "d"(msb), "c"(msr));
}

static inline void x86_msr_set_flag(x86_msr_t msr, uint64_t flag) {
        wrmsr(msr, rdmsr(msr) | flag);
}

////////////////////////////////////////////////////////////////////////////////
// i8259 PIC
////////////////////////////////////////////////////////////////////////////////

#define PIC_IRQ_COUNT_PER_PIC 8
#define PIC_MASTER_VECTOR_OFF 0x20
#define PIC_SLAVE_VECTOR_OFF  (PIC_MASTER_VECTOR_OFF + PIC_IRQ_COUNT_PER_PIC)
#define PIC_IRQ_COUNT_TOTAL   (PIC_IRQ_COUNT_PER_PIC * 2)
#define PIC_VECTOR_MIN        PIC_MASTER_VECTOR_OFF
#define PIC_VECTOR_MAX        (PIC_VECTOR_MIN + PIC_IRQ_COUNT_TOTAL - 1)

void i8259pic_init(void);
void i8259pic_send_eoi(uint8_t irq);
bool i8259pic_is_irq_masked(uint8_t irq);
void i8259pic_mask_irq(uint8_t irq);
void i8259pic_unmask_irq(uint8_t irq);

////////////////////////////////////////////////////////////////////////////////
// i8254 PIT
////////////////////////////////////////////////////////////////////////////////

void i8254timer_reset_to_1ms(void);
void i8254timer_stop(void);
// This counts given milliseconds without using interrupts. Reconfigures PIT.
void i8254timer_oneshot_count(unsigned millis);

////////////////////////////////////////////////////////////////////////////////
// ACPI MADT
////////////////////////////////////////////////////////////////////////////////

struct MADT {
        struct ACPI_SDTHeader Header;
        uint32_t LocalInterruptControllerAddress;
        uint32_t Flags;
        uint8_t entries[];
} PACKED;

struct MADT_EntryHeader {
        uint8_t type;
        uint8_t len;
} PACKED;

struct MADT_Entry_LAPIC {
        struct MADT_EntryHeader header;
        uint8_t acpi_processor_id;
        uint8_t apic_id;
        uint32_t flags;
} PACKED;

struct MADT_Entry_LAPIC_NMI {
        struct MADT_EntryHeader header;
        uint8_t acpi_processor_id; // 0xFF: All processors
        uint16_t flags;
        uint8_t lint; // LINT# (0 or 1)
} PACKED;

#define MADT_LAPIC_FLAG_PROCESSOR_ENABLED (1 << 0)
#define MADT_LAPIC_FLAG_ONLINE_CAPABLE    (1 << 1)

struct MADT_Entry_IOAPIC {
        struct MADT_EntryHeader header;
        uint8_t id;
        uint8_t reserved;
        uint32_t ioapic_addr;
        uint32_t gsi_base; // Global System Interrupt
} PACKED;

struct MADT_Entry_IOAPIC_InterruptSourceOverride {
        struct MADT_EntryHeader header;
        uint8_t bus_source;
        uint8_t irq_source;
        uint32_t gsi;
        uint16_t flags;
} PACKED;

// Used by LAPIC's interrupt related flags fields.
#define MADT_INT_FLAG_ACTIVE_LOW    (1 << 1) // Active High if not set
#define MADT_INT_FLAG_LEVEL_TRIGGER (1 << 3) // Edge Trigger if not set

// https://wiki.osdev.org/MADT#Entry_Type_0:_Processor_Local_APIC
typedef enum {
        MADT_ENTRY_LAPIC = 0,
        MADT_ENTRY_IOAPIC = 1,
        MADT_ENTRY_IOAPIC_INTERRUPT_SOURCE_OVERRIDE = 2,
        MADT_ENTRY_LAPIC_NMI = 4,
        MADT_ENTRY_LAPIC_ADDR_OVERRIDE = 5,
} madt_entry_type_t;

union MADT_Entry {
        struct MADT_EntryHeader common;
        struct MADT_Entry_LAPIC processor_lapic;
        struct MADT_Entry_IOAPIC ioapic;
        struct MADT_Entry_IOAPIC_InterruptSourceOverride
                ioapic_interrupt_source_override;
        struct MADT_Entry_LAPIC_NMI lapic_nmi;
};

struct MADT_EntryIter {
        unsigned next_byte_index;
        unsigned byte_count;
};

void madt_init(struct MADT *madt);
struct MADT_EntryIter madt_new_iter(void);
bool madt_entry_next(union MADT_Entry *out, struct MADT_EntryIter *iter);
bool madt_entry_next_of_type(
        void *out, struct MADT_EntryIter *iter, madt_entry_type_t type
);

extern struct MADT *g_madt;

////////////////////////////////////////////////////////////////////////////////
// Local APIC
////////////////////////////////////////////////////////////////////////////////

struct LAPIC_Descriptor_NMI {
        uint16_t flags; // See MADT_INT_~ flags
        bool valid;     // If false, this entry is invalid.
};

struct LAPIC_Descriptor {
        uint8_t acpi_processor_id;
        uint8_t apic_id;
        uint8_t flags;
        struct LAPIC_Descriptor_NMI nmi_info[2];
};

#define LAPIC_SPURIOUS_VECTOR            0xff
#define LAPIC_THERMAL_SENSOR_VECTOR      0xfe
#define LAPIC_PERFORMENCE_COUNTER_VECTOR 0xfc
#define LAPIC_ERROR_VECTOR               0xfb
#define LAPIC_LINT1_VECTOR               0xfa
#define LAPIC_LINT0_VECTOR               0xf9
#define LAPIC_CMCI_VECTOR                0xf8
#define LAPIC_TIMER_VECTOR               0xf7
// This one is not used by LAPIC itself, but by the broadcast IPI.
#define LAPIC_BROADCAST_IPI_VECTOR 0xf6

#define LAPIC_LVT_FLAG_DELIVERY_MODE_FIXED (0 << 8)
#define LAPIC_LVT_FLAG_DELIVERY_MODE_SMI   (2 << 8)
#define LAPIC_LVT_FLAG_DELIVERY_MODE_NMI   (4 << 8)
#define LAPIC_LVT_FLAG_DELIVERY_MODE_INIT  (5 << 8)
#define LAPIC_LVT_FLAG_DELIVERY_MODE_EXINT (7 << 8)
#define LAPIC_LVT_FLAG_HIGH_TRIGGERED      (0 << 13)
#define LAPIC_LVT_FLAG_LOW_TRIGGERED       (1 << 13)
#define LAPIC_LVT_FLAG_EDGE_TRIGGERED      (0 << 15)
#define LAPIC_LVT_FLAG_LEVEL_TRIGGERED     (1 << 15)
#define LAPIC_LVT_FLAG_MASKED              (1 << 16)

// Only applicable to Timer LVT register
#define LAPIC_LVT_TIMER_FLAG_MODE(__n)         ((__n) << 17)
#define LAPIC_LVT_TIMER_FLAG_MODE_ONESHOT      LAPIC_LVT_TIMER_FLAG_MODE(0)
#define LAPIC_LVT_TIMER_FLAG_MODE_PERIODIC     LAPIC_LVT_TIMER_FLAG_MODE(1)
#define LAPIC_LVT_TIMER_FLAG_MODE_TSC_DEADLINE LAPIC_LVT_TIMER_FLAG_MODE(2)

#define LAPIC_LVT_THERMAL_SENSOR_REG_RESERVED_MASK         (~0x117FFU)
#define LAPIC_LVT_PERFORMANCE_MONITORING_REG_RESERVED_MASK (~0x117FFU)
#define LAPIC_LVT_ERROR_REG_RESERVED_MASK                  (~0x110FFU)
#define LAPIC_LVT_LINT1_REG_RESERVED_MASK                  (~0x1F7FFU)
#define LAPIC_LVT_LINT0_REG_RESERVED_MASK                  (~0x1F7FFU)
#define LAPIC_LVT_CMCI_REG_RESERVED_MASK                   (~0x117FFU)
#define LAPIC_LVT_TIMER_REG_RESERVED_MASK                  (~0x710FFU)
#define LAPIC_SVR_REG_RESERVED_MASK                        (~0x13FFU)
#define LAPIC_ESR_RESERVED_MASK                            (~0xFFU)
#define LAPIC_ICRL_MASK                                    (~0xCDFFFU)
#define LAPIC_ICRH_MASK                                    (~0xFF000000U)
#define LAPIC_DIVIDE_CONFIG_REG_MASK                       (~0xFU)

#define LAPIC_REG_ID                                  0x020
#define LAPIC_REG_VERSION                             0x030
#define LAPIC_REG_EOI                                 0x0B0
#define LAPIC_REG_SVR                                 0x0F0
#define LAPIC_REG_ESR                                 0x280
#define LAPIC_REG_LVT_CMCI                            0x2F0
#define LAPIC_REG_ICRL                                0x300
#define LAPIC_REG_ICRH                                0x310
#define LAPIC_REG_LVT_TIMER                           0x320
#define LAPIC_REG_LVT_THERMAL_SENSOR                  0x330
#define LAPIC_REG_LVT_PERFORMANCE_MONITORING_COUNTERS 0x340
#define LAPIC_REG_LVT_LINT0                           0x350
#define LAPIC_REG_LVT_LINT1                           0x360
#define LAPIC_REG_LVT_ERROR                           0x370
#define LAPIC_REG_TIMER_INITIAL_COUNT                 0x380
#define LAPIC_REG_TIMER_CURRENT_COUNT                 0x390
#define LAPIC_REG_TIMER_DIVIDE_CONFIG                 0x3E0

// Used by LAPIC SVR register
#define LAPIC_SVR_FLAG_APIC_SOFTWARE_ELABLE (1 << 8)

#define LAPIC_IPI_FLAG_VECTOR(_n)                  ((_n) & 0xFF)
#define LAPIC_IPI_FLAG_DELIVERY_FIXED              (0 << 8)
#define LAPIC_IPI_FLAG_DELIVERY_INIT               (5 << 8)
#define LAPIC_IPI_FLAG_DELIVERY_STARTUP            (6 << 8)
#define LAPIC_IPI_FLAG_DEST_PHYSICAL               (0 << 11)
#define LAPIC_IPI_FLAG_DEST_LOGICAL                (1 << 11)
#define LAPIC_IPI_FLAG_LEVEL_DEASSERT              (0 << 14)
#define LAPIC_IPI_FLAG_LEVEL_ASSERT                (1 << 14)
#define LAPIC_IPI_FLAG_TRIGGER_EDGE                (0 << 15)
#define LAPIC_IPI_FLAG_TRIGGER_LEVEL               (1 << 15)
#define LAPIC_IPI_FLAG_DEST_SHORTHAND_NONE         (0 << 18)
#define LAPIC_IPI_FLAG_DEST_SHORTHAND_SELF         (1 << 18)
#define LAPIC_IPI_FLAG_DEST_SHORTHAND_ALL          (2 << 18)
#define LAPIC_IPI_FLAG_DEST_SHORTHAND_ALL_BUT_SELF (3 << 18)

uint32_t lapic_read_unchecked(uint32_t reg);
void lapic_write_unchecked(uint32_t reg, uint32_t val);
uint32_t lapic_read(uint32_t reg);
uint8_t lapic_read_esr(void);
void lapic_reset_esr(void);
void lapic_write(uint32_t reg, uint32_t val, uint32_t reserved_mask);
uint8_t lapic_id_for_current_processor(void);
// When LAPIC_IPI_FLAG_DELIVERY_STARTUP is used, the vector number acts as
// starting page number(e.g. 8 -> 0x8000).
void lapic_send_ipi(uint8_t target_apic_id, uint32_t flags);
void lapic_send_eoi(void);
void lapic_set_base(void *base);
void lapic_enable(void);
void lapic_timer_reset_to_1ms(void);
struct LAPIC_Descriptor const *lapic_for_current_processor(void);
struct LAPIC_Descriptor const *lapic_for_processor(unsigned idx);
size_t lapic_count(void);
void lapic_init_for_ap(void);
void lapic_init_for_bsp();

////////////////////////////////////////////////////////////////////////////////
// I/O APIC
////////////////////////////////////////////////////////////////////////////////

// See also: Intel 82093AA I/O APIC datasheet:
// https://web.archive.org/web/20161130153145/http://download.intel.com/design/chipsets/datashts/29056601.pdf

// I/O APIC registers
#define IOAPIC_REG_IOAPICID       0x00
#define IOAPIC_REG_IOAPICVER      0x01
#define IOAPIC_REG_IOAPICARB      0x02
#define IOAPIC_REG_IOREDTBL_START 0x10

struct IOAPIC {
        uint32_t address;
        uint32_t _reserved[3];
        uint32_t data;
};

struct IOAPIC_Descriptor {
        struct IOAPIC volatile *apic;
        uint32_t gsi_min;
        uint32_t gsi_max;
        uint8_t irq_base;
};

#define IOAPIC_IOREDTBL_FLAG_DELMOD(__n)      ((__n) << 8)
#define IOAPIC_IOREDTBL_FLAG_DELMOD_NORMAL    IOAPIC_IOREDTBL_FLAG_DELMOD(0)
#define IOAPIC_IOREDTBL_FLAG_DESTMOD_PHYSICAL (0 << 11)
#define IOAPIC_IOREDTBL_FLAG_INTPOL_LOW       (1 << 13)
#define IOAPIC_IOREDTBL_FLAG_INTPOL_HIGH      (0 << 13)
#define IOAPIC_IOREDTBL_FLAG_TRIGGER_LEVEL    (1 << 15)
#define IOAPIC_IOREDTBL_FLAG_TRIGGER_EDGE     (0 << 15)
#define IOAPIC_IOREDTBL_FLAG_INTERRUPT_MASK   (1 << 16)

#define IOAPIC_IRQ_VECTOR_BASE 0x30

uint32_t ioapic_read(struct IOAPIC volatile *apic, uint8_t reg);
void ioapic_write(struct IOAPIC volatile *apic, uint8_t reg, uint32_t val);
// Returns false if there's no matching IRQ.
bool ioapic_legacy_irq_to_gsi(uint32_t *out, uint8_t irq);
struct IOAPIC_Descriptor const *ioapic_for_gsi(uint32_t gsi);
void ioapic_init();

////////////////////////////////////////////////////////////////////////////////
// SMP boot
////////////////////////////////////////////////////////////////////////////////

#define SMPBOOT_AP_BOOT_CODE_PHYS_BASE 0x8000

size_t smpboot_ap_boot_code_page_count();
void smpboot_ap_did_boot(void);
void smpboot_start(void);

////////////////////////////////////////////////////////////////////////////////
// MMU
////////////////////////////////////////////////////////////////////////////////
uintptr_t mmu_get_pdbr(void);
void mmu_set_pdbr(uintptr_t pdbr);
void mmu_nuke_non_kernel_pages();
uintptr_t mmu_clone_pml4(void);

static inline void mmu_invalidate_local_tlb() {
        __asm__ volatile("mov rax, cr3\n"
                         "mov cr3, rax" ::
                                 : "rax");
}

static inline void mmu_invalidate_local_tlb_for(void *addr) {
        __asm__ volatile("invlpg [%0]" ::"r"(addr));
}

void mmu_invalidate_tlb(void);
void mmu_invalidate_tlb_for(void *addr);

////////////////////////////////////////////////////////////////////////////////
// System calls
////////////////////////////////////////////////////////////////////////////////

void syscall_init_tables(void);
void syscall_init_msrs(void);

////////////////////////////////////////////////////////////////////////////////
// Misc
////////////////////////////////////////////////////////////////////////////////

void stacktrace_show_using_rbp(void *rbp);
noreturn void kernel_entry_ap(unsigned ap_index);
void uartconsole_init();