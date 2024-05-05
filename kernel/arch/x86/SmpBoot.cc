// SPDX-FileCopyrightText: (c) 2024 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "SmpBoot.h"
#include "kernel/utility/Atomic.h"

extern "C" {
#define NORETURN_WORKAROUND
#include "_internal.h"
#include "kernel/arch/arch.h"
#include "kernel/kernel.h"
#include "kernel/lock/spinlock.h"
#include "kernel/memory/memory.h"
#include "kernel/tasks/tasks.h"
#include "kernel/utility/utility.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
}

#define AP_STARTUP_VECTOR SMPBOOT_AP_BOOT_CODE_PHYS_BASE >> 12

namespace Kernel::SmpBoot {

namespace {
        char const *LOG_TAG = "smpboot";

        static_assert((AP_STARTUP_VECTOR << 12) == SMPBOOT_AP_BOOT_CODE_PHYS_BASE, "AP code address isn't aligned");
        static_assert(AP_STARTUP_VECTOR <= 255, "AP code address is too high");

        #define INIT_IPI_BASE_FLAGS                                            \
                (LAPIC_IPI_FLAG_VECTOR(0) | LAPIC_IPI_FLAG_DELIVERY_INIT |     \
                LAPIC_IPI_FLAG_DEST_PHYSICAL | LAPIC_IPI_FLAG_TRIGGER_LEVEL | \
                LAPIC_IPI_FLAG_DEST_SHORTHAND_NONE)

        void start_ap(uint8_t target_apic_id) {
                // Send INIT IPI
                lapic_send_ipi(target_apic_id, INIT_IPI_BASE_FLAGS | LAPIC_IPI_FLAG_LEVEL_ASSERT);
                lapic_send_ipi(target_apic_id, INIT_IPI_BASE_FLAGS | LAPIC_IPI_FLAG_LEVEL_DEASSERT);
                i8254timer_oneshot_count(10);

                // Send two STARTUP IPIs
                for (unsigned i = 0; i < 2; ++i) {
                        lapic_send_ipi(
                                target_apic_id,
                                LAPIC_IPI_FLAG_VECTOR(AP_STARTUP_VECTOR) |
                                        LAPIC_IPI_FLAG_DELIVERY_STARTUP |
                                        LAPIC_IPI_FLAG_DEST_PHYSICAL |
                                        LAPIC_IPI_FLAG_TRIGGER_EDGE |
                                        LAPIC_IPI_FLAG_LEVEL_ASSERT |
                                        LAPIC_IPI_FLAG_DEST_SHORTHAND_NONE
                        );
                        i8254timer_oneshot_count(1);
                }
        }

        // #define INCLUDE_AP_DEBUG_PRINT

        // The symbols used by AP startup code are located in high-address area, which
        // AP cannot access until the MMU has ben set-up. So addresses have to be
        // relocated to lower addresses.
        // These symbols are declared inside startup code inline assembly, so addresses
        // can be calculated easily, and here's macro(for asm) and functions(for C) that
        // does this.
        //
        // NOTE: It is *very* important to keep these inside one inline assembly area,
        //       because otherwise locations of the symbols become unpredictable.

        // clang-format off
        #define ASM_AP_SYMBOL(_x)                \
                "((" _x " - smpboot_ap_boot_code) + " \
                STRINGIFY(SMPBOOT_AP_BOOT_CODE_PHYS_BASE) ")"
        // clang-format on

        extern "C" {
                extern void *smpboot_ap_boot_code;
                extern void *smpboot_ap_boot_code_end;
                extern void *smpboot_ap_entry;
                extern void *smpboot_ap_pml4;
                extern void *smpboot_ap_initial_rsps;
        }

        // Note that since the resulting address is low-address used by AP startup code,
        // those areas must be identity-mapped on BSP in order to use these functions.

        void volatile *ap_boot_code_var(void *var) {
                uintptr_t addr = ((uintptr_t)var - (uintptr_t)&smpboot_ap_boot_code) + SMPBOOT_AP_BOOT_CODE_PHYS_BASE;
                // UBsan will hate us if it's not aligned to 8-byte boundary
                ASSERT(!(addr & 0x7));
                return (void volatile *)addr;
        }

        void volatile *ap_boot_code_array_item(void *var, size_t item_size, unsigned item_index) {
                uintptr_t addr = ((uintptr_t)var - (uintptr_t)&smpboot_ap_boot_code) + SMPBOOT_AP_BOOT_CODE_PHYS_BASE;
                addr += item_size * item_index;
                // UBsan will hate us if it's not aligned to 8-byte boundary
                ASSERT(!(addr & 0x7));
                return (void volatile *)addr;
        }

        #define NEW

        // AP Startup code
        // clang-format off
        __asm__(".align 8\n"
                ".global smpboot_ap_boot_code, smpboot_ap_boot_code_end, smpboot_ap_entry, smpboot_ap_pml4, smpboot_ap_initial_rsps\n"
                "smpboot_ap_boot_code:\n"
                ////////////////////////////////////////////////////////////////////////
                // 16-bit real-mode code
                ////////////////////////////////////////////////////////////////////////
                ".code16\n"
                "        cli\n"
                "        cld\n"
                "        xor ax, ax\n"
                "        mov ax, ds\n"
                // Load protected mode GDT
                "        lgdt [" ASM_AP_SYMBOL("101f") "]\n"
                // Enable protected mode
                "        mov eax, cr0\n"
                "        or al, 1\n"
                "        mov cr0, eax\n"
                // Enter 32-bit mode protected code
                "        ljmp 0x08, " ASM_AP_SYMBOL("smpboot_ap_boot32") "\n"

                // Protected mode GDT
                "100:\n"
                "        .long 0, 0\n"                 // NULL descriptor
                "        .long 0x0000ffff, 0xcf9a00\n" // Base 0 Limit 0xFFFFF P=1 DPL=0
                                                // E=1 DC=0 RW=1 A=0 G=1 DB=0 L=0
                "        .long 0x0000ffff, 0xcf9200\n" // Base 0 Limit 0xFFFFF P=1 DPL=0
                                                // E=0 DC=0 RW=1 A=0 G=1 DB=0 L=0
                // Protected mode GDTR
                "101:\n"
                "         .word (101b - 100b - 1)\n"
                "         .long " ASM_AP_SYMBOL("100b") "\n"

                ////////////////////////////////////////////////////////////////////////
                // 32-bit protected mode code
                ////////////////////////////////////////////////////////////////////////
                ".code32\n"
                "smpboot_ap_boot32:\n"
                "         mov ax, 0x10\n"
                "         mov ds, ax\n"
                "         mov es, ax\n"
                "         mov fs, ax\n"
                "         mov gs, ax\n"
                "         mov ss, ax\n"
                // Enable PAE
                "        mov eax, cr4\n"
                "        or eax, 1 << 5\n"
                "        mov cr4, eax\n"
                // Load CR3 value(PDBR)
                "         mov eax, dword ptr [" ASM_AP_SYMBOL("smpboot_ap_pml4") "]\n"
                "         mov cr3, eax\n"
                // Activate Long Mode
                "        mov ecx, 0xC0000080\n"
                "        rdmsr\n"
                "        or eax, 1 << 8\n"
                "        or eax, 1 << 11\n"
                "        wrmsr\n"
                // Enable paging
                "        mov eax, cr0\n"
                "        or eax, 1 << 31\n"
                "        mov cr0, eax\n"
                // Load 64-bit GDT
                "        lgdt [" ASM_AP_SYMBOL("101f") "]\n"
                // Enter 64-bit mode code
                "        ljmp 0x08, " ASM_AP_SYMBOL("smpboot_ap_boot64") "\n"

                // Temporary 64-bit GDT
                "100:\n"
                "        .long 0, 0\n"                 // NULL descriptor
                "        .long 0x0000ffff, 0xaf9a00\n" // Base 0 Limit 0xFFFFF P=1 DPL=0
                                                // E=1 DC=0 RW=1 A=0 G=1 DB=0 L=1
                "        .long 0x0000ffff, 0xaf9200\n" // Base 0 Limit 0xFFFFF P=1 DPL=0
                                                // E=0 DC=0 RW=1 A=0 G=1 DB=0 L=1
                // Temporary 64-bit GDTR
                "101:\n"
                "        .word (101b - 100b - 1)\n"
                "         .long " ASM_AP_SYMBOL("100b") "\n"

                ////////////////////////////////////////////////////////////////////////
                // 64-bit mode code(includes C code that comes after!)
                ////////////////////////////////////////////////////////////////////////
                ".code64\n"
                "smpboot_ap_boot64:\n"
                "        mov ax, 0x10\n"
                "        mov ds, ax\n"
                "        mov es, ax\n"
                "        mov fs, ax\n"
                "        mov gs, ax\n"
                "        mov ss, ax\n"
                // Generate sequencial AP number
                "        xor eax, eax\n"
                "        inc eax\n"
                "        lock xadd dword ptr [" ASM_AP_SYMBOL("smpboot_ap_id") "], eax\n"
                // Setup AP stack
                "        mov rsp, qword ptr ["
                "                " ASM_AP_SYMBOL("smpboot_ap_initial_rsps")
                "                + rax * 8]\n"
                "        mov rbp, 0\n"
                // Call C entry point function(never returns)
                "        mov rdi, rax\n"
                "        mov rax, qword ptr [" ASM_AP_SYMBOL("smpboot_ap_entry") "]\n"
                "        call rax\n"
                // We shouldn't reach here, but do infinite loop in case we do.
                "1:\n"
                "        jmp 1b\n"
                // Thankfully debug printing is easy, the since serial port was already initialized by BSP.
                // So we only need code for actually sending characters, which is easy.
        #ifdef INCLUDE_AP_DEBUG_PRINT
                // 900: Print character and halt
                //      ah: Character to print
                "900:\n"
                // Wait for serial port to become ready
                "        mov dx, 0x3FD\n"
                "        in al, dx\n"
                "        and al, 0x20\n"
                "        jz 900b\n"
                // Serial port is ready
                "901:\n"
                "        mov dx, 0x3F8\n"
                "        mov al, ah\n"
                "        out dx, al\n"
                // Halt
                "902:\n"
                "        jmp 902b\n"
        #endif

                ////////////////////////////////////////////////////////////////////////
                // Variables
                ////////////////////////////////////////////////////////////////////////
                // AP entry point address
                ".align 8\n"
                "smpboot_ap_entry: .fill 1, 8, 0\n"
                // Kernel PML4 address
                ".align 8\n"
                "smpboot_ap_pml4: .fill 1, 4, 0\n"
                // Next AP number(0: First AP, 1: Second, ...)
                ".align 8\n"
                "smpboot_ap_id:   .fill 1, 4, 0\n"
                // Array of initial RSP values for each processor.
                // Actual contents come right after the end of this code.
                //
                // Note that when this bootstrap code is running, these pointers are located at low-memory.
                // So it is safe to put values after this point.
                ".align 8\n"
                "smpboot_ap_initial_rsps:"
                ".set smpboot_ap_boot_code_end, .\n");
        // clang-format on

        size_t ap_boot_code_byte_count() {
                return (uintptr_t)&smpboot_ap_boot_code_end - (uintptr_t)&smpboot_ap_boot_code;
        }

        // AP stack size
        #define AP_BOOT_STACK_SIZE       (2UL * 1024UL * 1024UL)
        #define AP_BOOT_STACK_PAGE_COUNT (AP_BOOT_STACK_SIZE / PAGE_SIZE)

        // TODO: This style of allocation can be seen in other places as well, so move
        //       this to somewhere else!
        WARN_UNUSED_RESULT void *alloc_ap_boot_stack(void) {
                struct PhysPage_Addr page = physpage_alloc(AP_BOOT_STACK_PAGE_COUNT);
                if (page.value == 0) {
                        TODO_HANDLE_ERROR();
                }
                void *addr = process_map_pages(
                        process_kernel(),
                        page.value,
                        AP_BOOT_STACK_PAGE_COUNT,
                        (struct Proc_MapOptions){.writable = true, .executable = false}
                );
                if (addr == nullptr) {
                        TODO_HANDLE_ERROR();
                }
                return (void *)addr;
        }

        Atomic<size_t> s_booted_ap_count = 0;
        // struct SpinLock s_booted_ap_count_lock;

        [[noreturn]] void ap_boot_oom() {
                LOGE(LOG_TAG, "Not enough memory to boot APs");
                TODO_HANDLE_ERROR();
        }

}

// Maximum size for stack pointer array
#define INITIAL_RSPS_MAX_SIZE (PAGE_SIZE / sizeof(uintptr_t))

size_t ap_boot_code_page_count() {
        // Give it one more page to leave enough room for the stack pointer
        // array
        return to_block_count(PAGE_SIZE, ap_boot_code_byte_count()) + 1;
}

void ap_did_boot(void) {
        // bool prev_interrupt_state;
        // spinlock_lock(&s_booted_ap_count_lock, &prev_interrupt_state);
        s_booted_ap_count.add_fetch(1);
        // spinlock_unlock(&s_booted_ap_count_lock, prev_interrupt_state);
}

size_t next_ap_to_init(void) {
        return s_booted_ap_count.load();
}

void start(void) {
        ENTER_NO_INTERRUPT_SECTION();
        size_t apic_count = lapic_count();
        size_t ap_count = apic_count - 1;
        if (!processor_prepare_aps(ap_count)) {
                ap_boot_oom();
        }
        if (!mmu_prepare_aps(ap_count)) {
                ap_boot_oom();
        }

        struct MADT_EntryIter iter = madt_new_iter();
        union MADT_Entry entry;
        while (madt_entry_next(&entry, &iter)) {
                madt_entry_type_t type = static_cast<madt_entry_type_t>(entry.common.type);
                switch (type) {
                case MADT_ENTRY_LAPIC:
                case MADT_ENTRY_IOAPIC:
                case MADT_ENTRY_IOAPIC_INTERRUPT_SOURCE_OVERRIDE:
                case MADT_ENTRY_LAPIC_NMI:
                        break;
                case MADT_ENTRY_LAPIC_ADDR_OVERRIDE:
                        panic("Handle LOCALAPIC_ADDRESS_OVERRIDE!!\n");
                default:
                        panic("TODO(OIS): Handle entry type %u", type);
                }
        }

        // Identity-map low address area
        for (uintptr_t addr = SMPBOOT_AP_BOOT_CODE_PHYS_BASE;
             addr < SMPBOOT_AP_BOOT_CODE_PHYS_BASE +
                            ap_boot_code_page_count() * PAGE_SIZE;
             addr += PAGE_SIZE) {
                bool map_ok = mmu_lowmem_identity_map(
                        addr, MMU_PROT_EXEC | MMU_PROT_WRITE
                );
                ASSERT(map_ok);
        }

        // Copy startup code along with initial variable values.
        kmemcpy((void *)SMPBOOT_AP_BOOT_CODE_PHYS_BASE,
                &smpboot_ap_boot_code,
                ap_boot_code_page_count() * PAGE_SIZE);

        // Setup AP startup code variables
        *(uintptr_t volatile *)(ap_boot_code_var(&smpboot_ap_entry)) =
                (uintptr_t)kernel_entry_ap;
        *(uint32_t volatile *)(ap_boot_code_var(&smpboot_ap_pml4)) =
                mmu_get_pdbr();

        // Create stack for each AP
        for (size_t i = 0; i < ap_count; ++i) {
                void *stack_base = alloc_ap_boot_stack();
                if (stack_base == nullptr) {
                        TODO_HANDLE_ERROR();
                }
                uintptr_t initial_rsp = (uintptr_t)stack_base + AP_BOOT_STACK_SIZE;
                *(uintptr_t volatile *)ap_boot_code_array_item(&smpboot_ap_initial_rsps, sizeof(uintptr_t), i) = initial_rsp;
        }
        mmu_invalidate_local_tlb();
        LEAVE_NO_INTERRUPT_SECTION();

        // Start APs
        LOGI(LOG_TAG, "Booting APs...");
        for (unsigned i = 0; i < apic_count; ++i) {
                struct LAPIC_Descriptor const *info = lapic_for_processor(i);
                if (info->apic_id == lapic_for_current_processor()->apic_id) {
                        LOGI(LOG_TAG, "APIC %u is BSP. Skipping...", info->apic_id);
                        continue;
                }
                start_ap(info->apic_id);
        }
        // During AP startup BSP needs to listen to interrupts coming from APs.
        bool prev_interrupt_state = interrupts_enable();
        bool all_booted = false;
        while (!all_booted) {
                processor_process_ipimessages();
                // bool prev_interrupt_state;
                // spinlock_lock(&s_booted_ap_count_lock, &prev_interrupt_state);
                if (s_booted_ap_count.load() == ap_count) {
                        all_booted = true;
                }
                // spinlock_unlock(&s_booted_ap_count_lock, prev_interrupt_state);
        }
        if (!prev_interrupt_state) {
                interrupts_disable();
        }
        LOGI(LOG_TAG, "AP boot complete");
}

}