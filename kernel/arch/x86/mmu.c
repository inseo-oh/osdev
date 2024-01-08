// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "_internal.h"
#include <kernel/arch/arch.h>
#include <kernel/heap/heap.h>
#include <kernel/kernel.h>
#include <kernel/lock/spinlock.h>
#include <kernel/memory/memory.h>
#include <kernel/sections.h>
#include <kernel/utility/utility.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static char const *LOG_TAG = "mmu";

#define PAGING_FLAG_P   (1 << 0)
#define PAGING_FLAG_RW  (1 << 1)
#define PAGING_FLAG_US  (1 << 2)
#define PAGING_FLAG_PWT (1 << 3)
#define PAGING_FLAG_PCD (1 << 4)
#define PAGING_FLAG_A   (1 << 5)
#define PAGING_FLAG_D   (1 << 6) // Only for PT entries
#define PAGING_FLAG_PS  (1 << 7) // Only for PML3 and PD entries.
#define PAGING_FLAG_PAT (1 << 7) // Only for PT entries
#define PAGING_FLAG_G   (1 << 8) // Only for PT entries
#define PAGING_FLAG_XD  (1ULL << 63)

typedef uint64_t paging_entry_t;
#define ENTRY_BASE_ADDR_OF(_x) ((_x) & 0xFFFFFFFFFF000)

#define PAGING_ENTRY_NON_PRESENT ((paging_entry_t)0)
#define PAGING_ENTRY_COUNT       (PAGE_SIZE / sizeof(paging_entry_t))

#define PML4_ENTRY_INDEX_OF(_x) (((uintptr_t)(_x) >> 39) & 0x1FF)
#define PML3_ENTRY_INDEX_OF(_x) (((uintptr_t)(_x) >> 30) & 0x1FF)
#define PML2_ENTRY_INDEX_OF(_x) (((uintptr_t)(_x) >> 21) & 0x1FF)
#define PML1_ENTRY_INDEX_OF(_x) (((uintptr_t)(_x) >> 12) & 0x1FF)
#define OFFSET_IN_PAGE_OF(_x)   ((uintptr_t)(_x) & 0xFFF)

static paging_entry_t volatile *s_scratch_page_pml1 =
        (void *)SCRATCH_PAGE_PML1_VIRTBASE;

static void *s_direct_mapped_base;
static bool s_scratch_page_ready;
static mmu_addrspace_t s_lowmem_identity_map_handle;
static uintptr_t *s_pml4s_for_aps;
static bool s_is_smp_mode = false;
static struct SpinLock s_lock;

#define SCRATCH_PAGE_PML1_ENTRY \
        &s_scratch_page_pml1[PML1_ENTRY_INDEX_OF(SCRATCH_PAGE_VIRTBASE)]

static void invalidate_tlb(void) {
        mmu_invalidate_local_tlb();
        if (s_is_smp_mode) {
                processor_flush_other_processors_tlb();
        }
}

static void invalidate_tlb_for(void *addr) {
        mmu_invalidate_local_tlb_for(addr);
        if (s_is_smp_mode) {
                processor_flush_other_processors_tlb_for(addr);
        }
}

// __attribute__((noinline)): It seems to cause strange behavior when NOINLINE
// is not used.
//
// FIXME: Figure out what happens with UBSan when __attribute__((noinline)) is
// not present!
__attribute__((noinline)) static void *
map_to_scratch_page(uintptr_t physaddr, bool allow_write_access) {
        if (!s_scratch_page_ready) {
                return s_direct_mapped_base + physaddr;
        }
        ASSERT(!interrupts_are_enabled());
        ASSERT(is_aligned(PAGE_SIZE, physaddr));
        paging_entry_t entry = PAGING_FLAG_P | PAGING_FLAG_XD;
        if (allow_write_access) {
                entry |= PAGING_FLAG_RW;
        }
        *SCRATCH_PAGE_PML1_ENTRY = entry;
        invalidate_tlb_for((void *)SCRATCH_PAGE_VIRTBASE);
        return (void *)SCRATCH_PAGE_VIRTBASE;
}

static void unmap_scratch_page() {
        if (!s_scratch_page_ready) {
                return;
        }
        ASSERT(!interrupts_are_enabled());
        *SCRATCH_PAGE_PML1_ENTRY = PAGING_ENTRY_NON_PRESENT;
        invalidate_tlb_for((void *)SCRATCH_PAGE_VIRTBASE);
}

WARN_UNUSED_RESULT static struct PhysPage_Addr create_blank_table() {
        ASSERT(!interrupts_are_enabled());
        struct PhysPage_Addr out = physpage_alloc(1);
        if (!out.value) {
                return PHYSPAGE_NULL;
        }
        if (!s_scratch_page_ready) {
                kmemset(s_direct_mapped_base + out.value, 0, PAGE_SIZE);
                return out;
        }
        void *base = map_to_scratch_page(out.value, true);
        kmemset(base, 0, PAGE_SIZE);
        unmap_scratch_page();
        return out;
}

WARN_UNUSED_RESULT static paging_entry_t
get_table_entry(uintptr_t table_base, unsigned entry_index) {
        ASSERT(!interrupts_are_enabled());
        ASSERT(is_aligned(PAGE_SIZE, table_base));
        paging_entry_t volatile *base = map_to_scratch_page(table_base, false);
        paging_entry_t out = base[entry_index];
        unmap_scratch_page();
        return out;
}

static void set_table_entry(
        uintptr_t table_base, unsigned entry_index, paging_entry_t entry
) {
        ASSERT(!interrupts_are_enabled());
        ASSERT(is_aligned(PAGE_SIZE, table_base));

        paging_entry_t volatile *base = map_to_scratch_page(table_base, true);
        base[entry_index] = entry;
        unmap_scratch_page();
}

// If table for the entry couldn't be created, non-present entry(i.e. entry with
// p=false) is returned.
WARN_UNUSED_RESULT static paging_entry_t get_or_create_table_entry(
        uintptr_t table_base, unsigned entry_index, bool is_user_page
) {
        ASSERT(!interrupts_are_enabled());
        ASSERT(is_aligned(PAGE_SIZE, table_base));
        paging_entry_t out = get_table_entry(table_base, entry_index);
        if (!(out & PAGING_FLAG_P)) {
                struct PhysPage_Addr page = create_blank_table();
                if (!page.value) {
                        return PAGING_ENTRY_NON_PRESENT;
                }
                out = page.value | PAGING_FLAG_RW | PAGING_FLAG_P;
                if (is_user_page) {
                        out |= PAGING_FLAG_US;
                }
                set_table_entry(table_base, entry_index, out);
        }
        return out;
}

WARN_UNUSED_RESULT static paging_entry_t get_pml3(unsigned pml4e_index) {
        ASSERT(!interrupts_are_enabled());
        return get_table_entry(mmu_get_pdbr(), pml4e_index);
}

static void set_pml3(unsigned pml4e_index, paging_entry_t entry) {
        ASSERT(!interrupts_are_enabled());
        set_table_entry(mmu_get_pdbr(), pml4e_index, entry);
}

// Returned flags also include PAGING_FLAG_P
static paging_entry_t paging_flags_from_prot(mmu_prot_t prot) {
        paging_entry_t entry = PAGING_FLAG_P;
        if (!(prot & MMU_PROT_EXEC)) {
                entry |= PAGING_FLAG_XD;
        }
        if (prot & MMU_PROT_USER) {
                entry |= PAGING_FLAG_US;
        }
        if (prot & MMU_PROT_WRITE) {
                entry |= PAGING_FLAG_RW;
        }
        return entry;
}

static bool satisfies_requirement(paging_entry_t entry, mmu_prot_t requires) {
        return (!(requires & MMU_PROT_WRITE) || (entry & PAGING_FLAG_RW)) &&
               (!(requires & MMU_PROT_EXEC) || !(entry & PAGING_FLAG_XD)) &&
               (!(requires & MMU_PROT_USER) || (entry & PAGING_FLAG_US));
}

// `middle_entries_requires` specifies anything required before the requested PT
// entry.
WARN_UNUSED_RESULT static bool get_pt_base_and_entry(
        void *virtaddr,
        uintptr_t pml3_physbase,
        mmu_prot_t middle_entries_require,
        uintptr_t *base_out,
        paging_entry_t *entry_out
) {
        ASSERT(!interrupts_are_enabled());

        paging_entry_t entry;
        entry = get_table_entry(pml3_physbase, PML3_ENTRY_INDEX_OF(virtaddr));
        if (!satisfies_requirement(entry, middle_entries_require)) {
                return false;
        }
        uintptr_t pml2_physbase = ENTRY_BASE_ADDR_OF(entry);
        entry = get_table_entry(pml2_physbase, PML2_ENTRY_INDEX_OF(virtaddr));
        if (!satisfies_requirement(entry, middle_entries_require)) {
                return false;
        }
        uintptr_t pm1_physbase = ENTRY_BASE_ADDR_OF(entry);
        *base_out = pm1_physbase;
        entry = get_table_entry(pm1_physbase, PML1_ENTRY_INDEX_OF(virtaddr));
        if (!(entry & PAGING_FLAG_P)) {
                return false;
        }
        *entry_out = entry;
        return true;
}

static mmu_addrspace_t active_user_vm_addrspace(void) {
        paging_entry_t entry = get_pml3(PML4_ENTRY_INDEX_OF(USER_VM_VIRTBASE));
        return !(entry & PAGING_FLAG_P) ? MMU_ADDRSPACE_INVALID
                                        : ENTRY_BASE_ADDR_OF(entry);
}

uintptr_t mmu_get_pdbr(void) {
        uintptr_t pdbr;
        __asm__ volatile("mov %0, cr3" : "=r"(pdbr));
        return pdbr;
}

void mmu_set_pdbr(uintptr_t pdbr) {
        __asm__ volatile("mov cr3, %0" ::"r"(pdbr));
}

void mmu_invalidate_tlb(void) {
        bool prev_interrupt_state;
        spinlock_lock(&s_lock, &prev_interrupt_state);
        invalidate_tlb();
        spinlock_unlock(&s_lock, prev_interrupt_state);
}

void mmu_invalidate_tlb_for(void *addr) {
        bool prev_interrupt_state;
        spinlock_lock(&s_lock, &prev_interrupt_state);
        invalidate_tlb_for(addr);
        spinlock_unlock(&s_lock, prev_interrupt_state);
}

bool mmu_map(
        mmu_addrspace_t handle,
        uintptr_t physaddr,
        void *virtaddr,
        mmu_prot_t prot
) {
        bool prev_interrupt_state;
        spinlock_lock(&s_lock, &prev_interrupt_state);
        ASSERT(is_aligned(PAGE_SIZE, physaddr));
        ASSERT(is_aligned(PAGE_SIZE, (uintptr_t)virtaddr));
        paging_entry_t entry;
        bool success = false;
        bool is_user = prot & MMU_PROT_USER;
        bool is_exec = prot & MMU_PROT_EXEC;
        bool is_write = prot & MMU_PROT_WRITE;
        // NOTE: `handle` is address to PML3
        uintptr_t pml3_physbase = handle;
        entry = get_or_create_table_entry(
                pml3_physbase, PML3_ENTRY_INDEX_OF(virtaddr), is_user
        );
        if (!(entry & PAGING_FLAG_P)) {
                goto out;
        }
        ASSERT(!(entry & PAGING_FLAG_XD) || !is_exec);
        ASSERT((entry & PAGING_FLAG_RW) || !is_write);
        uintptr_t pml2_physbase = ENTRY_BASE_ADDR_OF(entry);
        entry = get_or_create_table_entry(
                pml2_physbase, PML2_ENTRY_INDEX_OF(virtaddr), is_user
        );
        if (!(entry & PAGING_FLAG_P)) {
                goto out;
        }
        ASSERT(!(entry & PAGING_FLAG_XD) || !is_exec);
        ASSERT((entry & PAGING_FLAG_RW) || !is_write);
        uintptr_t pm1_physbase = ENTRY_BASE_ADDR_OF(entry);
        entry = physaddr | paging_flags_from_prot(prot);
        set_table_entry(pm1_physbase, PML1_ENTRY_INDEX_OF(virtaddr), entry);
        invalidate_tlb_for(virtaddr);
        success = true;
out:
        spinlock_unlock(&s_lock, prev_interrupt_state);
        return success;
}

bool mmu_lowmem_identity_map(uintptr_t physaddr, mmu_prot_t prot) {
        return mmu_map(
                s_lowmem_identity_map_handle, physaddr, (void *)physaddr, prot
        );
}

void mmu_update_options(
        mmu_addrspace_t handle, void *virtaddr, mmu_prot_t prot
) {
        bool prev_interrupt_state;
        spinlock_lock(&s_lock, &prev_interrupt_state);
        uintptr_t pm1_physbase;
        paging_entry_t pt_entry;
        // NOTE: `handle` is address to PML3
        bool is_present = get_pt_base_and_entry(
                virtaddr, handle, prot, &pm1_physbase, &pt_entry
        );
        ASSERT(is_present);
        set_table_entry(

                pm1_physbase, PML1_ENTRY_INDEX_OF(virtaddr), pt_entry
        );
        invalidate_tlb_for(virtaddr);
        spinlock_unlock(&s_lock, prev_interrupt_state);
}

void mmu_unmap(mmu_addrspace_t handle, void *virtaddr) {
        bool prev_interrupt_state;
        spinlock_lock(&s_lock, &prev_interrupt_state);
        ASSERT(is_aligned(PAGE_SIZE, (uintptr_t)virtaddr));
        uintptr_t pm1_physbase;
        paging_entry_t pt_entry;
        // NOTE: `handle` is address to PML3
        bool is_present = get_pt_base_and_entry(
                virtaddr, handle, 0, &pm1_physbase, &pt_entry
        );
        ASSERT(is_present);
        set_table_entry(
                pm1_physbase,
                PML1_ENTRY_INDEX_OF(virtaddr),
                PAGING_ENTRY_NON_PRESENT
        );
        invalidate_tlb_for(virtaddr);
        spinlock_unlock(&s_lock, prev_interrupt_state);
}

void mmu_lowmem_identity_unmap(uintptr_t physaddr) {
        mmu_unmap(s_lowmem_identity_map_handle, (void *)physaddr);
}

// Returns MMU_ADDRSPACE_INVALID if creation failed.
mmu_addrspace_t mmu_addrspace_create(void) {
        bool prev_interrupt_state;
        spinlock_lock(&s_lock, &prev_interrupt_state);
        struct PhysPage_Addr table_base = create_blank_table();
        spinlock_unlock(&s_lock, prev_interrupt_state);
        if (!table_base.value) {
                return MMU_ADDRSPACE_INVALID;
        }
        return table_base.value;
}

void mmu_addrspace_delete(mmu_addrspace_t addrspace) {
        bool prev_interrupt_state;
        spinlock_lock(&s_lock, &prev_interrupt_state);
        (void)addrspace;
        TODO();
        spinlock_unlock(&s_lock, prev_interrupt_state);
}

void mmu_activate_addrspace(
        mmu_addrspace_t addrspace, uintptr_t addrspace_base
) {
        bool prev_interrupt_state;
        spinlock_lock(&s_lock, &prev_interrupt_state);
        // NOTE: `addrspace` is address to PML3
        ASSERT(is_aligned(PAGE_SIZE, addrspace));
        paging_entry_t entry =
                addrspace | PAGING_FLAG_US | PAGING_FLAG_RW | PAGING_FLAG_P;
        set_pml3(PML4_ENTRY_INDEX_OF(addrspace_base), entry);
        invalidate_tlb();
        spinlock_unlock(&s_lock, prev_interrupt_state);
}

void mmu_activate_user_vm_addrspace(mmu_addrspace_t addrspace) {
        mmu_activate_addrspace(addrspace, USER_VM_VIRTBASE);
}

void mmu_deactivate_user_vm_addrspace(void) {
        bool prev_interrupt_state;
        spinlock_lock(&s_lock, &prev_interrupt_state);
        if (active_user_vm_addrspace() != MMU_ADDRSPACE_INVALID) {
                set_pml3(
                        PML4_ENTRY_INDEX_OF(USER_VM_VIRTBASE),
                        PAGING_ENTRY_NON_PRESENT
                );
                invalidate_tlb();
        }
        spinlock_unlock(&s_lock, prev_interrupt_state);
}

mmu_addrspace_t mmu_active_user_vm_addrspace(void) {
        bool prev_interrupt_state;
        spinlock_lock(&s_lock, &prev_interrupt_state);
        mmu_addrspace_t result = active_user_vm_addrspace();
        spinlock_unlock(&s_lock, prev_interrupt_state);
        return result;
}

uintptr_t mmu_virt_to_phys(void *virtaddr) {
        bool prev_interrupt_state;
        spinlock_lock(&s_lock, &prev_interrupt_state);
        paging_entry_t entry = get_pml3(PML4_ENTRY_INDEX_OF(virtaddr));
        ASSERT((entry & PAGING_FLAG_P));
        uintptr_t pml3_physbase = ENTRY_BASE_ADDR_OF(entry);
        uintptr_t pm1_physbase;
        bool is_present = get_pt_base_and_entry(
                virtaddr, pml3_physbase, 0, &pm1_physbase, &entry
        );
        ASSERT(is_present);
        spinlock_unlock(&s_lock, prev_interrupt_state);
        return ENTRY_BASE_ADDR_OF(entry) + OFFSET_IN_PAGE_OF(virtaddr);
}

bool mmu_is_accessible(void *virtaddr, mmu_prot_t requires) {
        bool prev_interrupt_state;
        spinlock_lock(&s_lock, &prev_interrupt_state);
        paging_entry_t entry;
        entry = get_pml3(PML4_ENTRY_INDEX_OF(virtaddr));
        bool result = false;
        if (!(entry & PAGING_FLAG_P)) {
                goto out;
        }
        uintptr_t pml3_physbase = ENTRY_BASE_ADDR_OF(entry);
        uintptr_t pm1_physbase;
        bool is_present = get_pt_base_and_entry(
                virtaddr, pml3_physbase, requires, &pm1_physbase, &entry
        );
        if (!is_present) {
                goto out;
        }
        if (!satisfies_requirement(entry, requires)) {
                goto out;
        }
        result = true;
out:
        spinlock_unlock(&s_lock, prev_interrupt_state);
        return result;
}

mmu_addrspace_t mmu_init_for_bsp(void *direct_mapped_base) {
        ASSERT(!interrupts_are_enabled());
        s_direct_mapped_base = direct_mapped_base;
        s_scratch_page_ready = false;
        mmu_addrspace_t kernel_addrspace_handle =
                ENTRY_BASE_ADDR_OF(get_pml3(511));
        s_lowmem_identity_map_handle = mmu_addrspace_create();
        if (s_lowmem_identity_map_handle == MMU_ADDRSPACE_INVALID) {
                panic("OOM");
        }
        mmu_activate_addrspace(s_lowmem_identity_map_handle, 0x0);
        return kernel_addrspace_handle;
}

void mmu_nuke_non_kernel_pages() {
        ASSERT(!interrupts_are_enabled());
        for (uint64_t i = 0; i < PAGING_ENTRY_COUNT; ++i) {
                if (i == 0 || 256 <= i) {
                        continue;
                }
                set_pml3(i, PAGING_ENTRY_NON_PRESENT);
        }
        invalidate_tlb();
}

uintptr_t mmu_clone_pml4(void) {
        struct PhysPage_Addr new_pdbr = physpage_alloc(1);
        if (!new_pdbr.value) {
                goto fail;
        }
        for (unsigned i = 0; i < PAGING_ENTRY_COUNT; ++i) {
                set_table_entry(new_pdbr.value, i, get_pml3(i));
        }
        return new_pdbr.value;
fail:
        if (new_pdbr.value) {
                physpage_free(new_pdbr, 1);
        }
        return 0;
}

void mmu_init_for_ap(unsigned ap_index) {
        s_is_smp_mode = true;
        mmu_set_pdbr(s_pml4s_for_aps[ap_index]);
}

bool mmu_prepare_aps(unsigned ap_count) {
        // TODO(OIS): Create something like cmalloc() and replace this with
        //            that.
        size_t size = sizeof(*s_pml4s_for_aps) * ap_count;
        s_pml4s_for_aps = kmalloc(size);
        if (!s_pml4s_for_aps) {
                LOGE(LOG_TAG,
                     "Not enough memory to initialize %u processors",
                     ap_count);
                goto fail;
        }
        kmemset(s_pml4s_for_aps, 0, size);
        for (unsigned i = 0; i < ap_count; ++i) {
                s_pml4s_for_aps[i] = mmu_clone_pml4();
                if (!s_pml4s_for_aps[i]) {
                        goto fail;
                }
        }
        return true;
fail:
        if (s_pml4s_for_aps) {
                for (unsigned i = 0; i < ap_count; ++i) {
                        if (!s_pml4s_for_aps[i]) {
                                physpage_free(
                                        (struct PhysPage_Addr
                                        ){s_pml4s_for_aps[i]},
                                        1
                                );
                        }
                }
                kfree(s_pml4s_for_aps);
        }
        return false;
}