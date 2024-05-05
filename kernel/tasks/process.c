// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "tasks.h"
#include <errno.h>
#include "kernel/api/bits/limits.h"
#include "kernel/arch/arch.h"
#include "kernel/heap/heap.h"
#include "kernel/kernel.h"
#include "kernel/lock/spinlock.h"
#include "kernel/memory/memory.h"
#include "kernel/sections.h"
#include "kernel/utility/utility.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>

struct ChildThread {
        struct AVLTree_Node node_head;
        struct Thread *thread;
};

struct Process {
        struct SpinLock lock;
        mmu_addrspace_t addrspace;
        struct VirtZone virtzone;
        struct AVLTree child_threads;
        pid_t id;
        unsigned next_thread_id;
        char name[PROC_NAME_MAX_LEN + 1];
};

static mmu_prot_t make_mmu_prot_flags(struct Process *process, struct Proc_MapOptions options) {
        mmu_prot_t prot_flags = 0;
        if (options.executable) {
                prot_flags |= MMU_PROT_EXEC;
        }
        if (options.writable) {
                prot_flags |= MMU_PROT_WRITE;
        }
        if (!process_is_kernel(process)) {
                prot_flags |= MMU_PROT_USER;
        }
        return prot_flags;
}

WARN_UNUSED_RESULT static bool map_pages(mmu_addrspace_t addrspace, void *virtbase, uintptr_t physbase, mmu_prot_t prot, size_t count) {
        size_t mapped_count = 0;
        uintptr_t current_virtbase = (uintptr_t)virtbase;
        uintptr_t current_physbase = (uintptr_t)physbase;
        ASSERT(is_aligned(PAGE_SIZE, current_virtbase));
        ASSERT(is_aligned(PAGE_SIZE, current_physbase));
        for (size_t i = 0; i < count; ++i,
                    ++mapped_count,
                    current_virtbase += PAGE_SIZE,
                    current_physbase += PAGE_SIZE) {
                if (!mmu_map(
                            addrspace,
                            current_physbase,
                            (void *)current_virtbase,
                            prot
                    )) {
                        goto fail;
                }
        }
        return true;
fail:
        current_virtbase = (uintptr_t)virtbase;
        for (size_t i = 0; i < mapped_count;
             ++i, current_virtbase += PAGE_SIZE) {
                mmu_unmap(addrspace, (void *)current_virtbase);
        }
        return false;
}

char const *process_get_name(struct Process const *process) {
        return process->name;
}

pid_t process_get_id(struct Process const *process) { return process->id; }

// Returns NULL on failure
WARN_UNUSED_RESULT static struct Process *do_spawn(
        char const *name, mmu_addrspace_t mmu_addrspace, bool is_kernel_process
);

struct Process *process_running(void) {
        struct Thread *thread = thread_running();
        if (!thread) {
                return NULL;
        }
        return thread_get_parent_proc(thread);
}

tid_t process_add_thread(struct Process *process, struct Thread *thread) {
        struct ChildThread *cthread = kmalloc(sizeof(*cthread));
        if (!cthread) {
                return THREAD_ID_INVALID;
        }
        kmemset(cthread, 0, sizeof(*cthread));
        cthread->thread = thread;
        unsigned tid;
        {
                bool prev_interrupt_state;
                spinlock_lock(&process->lock, &prev_interrupt_state);
                tid = process->next_thread_id;
                if (tid > THREAD_ID_MAX) {
                        panic("TODO");
                }
                ++process->next_thread_id;
                avltree_insert(
                        &process->child_threads, &cthread->node_head, tid
                );
                spinlock_unlock(&process->lock, prev_interrupt_state);
        }
        return tid;
}

void *process_map_unaligned(struct Process *process, uintptr_t physaddr, size_t size, struct Proc_MapOptions options) {
        uintptr_t aligned_base = physaddr - (physaddr % PAGE_SIZE);
        uintptr_t offset = physaddr - aligned_base;
        size += offset;
        unsigned page_count = to_block_count(PAGE_SIZE, size);
        uint8_t *virtbase =
                process_map_pages(process, aligned_base, page_count, options);
        if (!virtbase) {
                return 0;
        }
        return (void *)(virtbase + offset);
}

void process_unmap_unaligned(struct Process *process, void *base, size_t size) {
        uintptr_t base_addr = (uintptr_t)base;
        uintptr_t aligned_base = base_addr - (base_addr % PAGE_SIZE);
        uintptr_t offset = base_addr - aligned_base;
        size += offset;
        unsigned page_count = to_block_count(PAGE_SIZE, size);

        process_unmap_pages(process, (void *)aligned_base, page_count);
}

void *process_map_pages(struct Process *process, uintptr_t physbase, size_t page_count, struct Proc_MapOptions options) {
        ASSERT(physbase != 0);
        ASSERT(is_aligned(PAGE_SIZE, physbase));
        bool prev_interrupt_state;
        mmu_prot_t prot_flags = make_mmu_prot_flags(process, options);
        spinlock_lock(&process->lock, &prev_interrupt_state);
        void *virtbase = virtzone_alloc_region(&process->virtzone, page_count);
        if (!virtbase) {
                goto fail;
        }
        if (!map_pages(
                    process->addrspace,
                    virtbase,
                    physbase,
                    prot_flags,
                    page_count
            )) {
                goto fail;
        }
        goto out;
fail:
        if (virtbase) {
                virtzone_free_region(&process->virtzone, virtbase, page_count);
                virtbase = NULL;
        }
out:
        spinlock_unlock(&process->lock, prev_interrupt_state);
        return virtbase;
}

bool process_map_pages_at(struct Process *process, uintptr_t physbase, void *virtbase, size_t page_count, struct Proc_MapOptions options) {
        ASSERT(is_aligned(PAGE_SIZE, (uintptr_t)virtbase));
        ASSERT(is_aligned(PAGE_SIZE, physbase));
        bool prev_interrupt_state;
        spinlock_lock(&process->lock, &prev_interrupt_state);
        mmu_prot_t prot_flags = make_mmu_prot_flags(process, options);
        bool alloc_ok = virtzone_alloc_region_at(
                &process->virtzone, virtbase, page_count
        );
        if (!alloc_ok) {
                goto fail;
        }
        if (!map_pages(
                    process->addrspace,
                    virtbase,
                    physbase,
                    prot_flags,
                    page_count
            )) {
                goto fail;
        }
        goto out;
fail:
        if (alloc_ok) {
                virtzone_free_region(&process->virtzone, virtbase, page_count);
                alloc_ok = false;
        }
out:
        spinlock_unlock(&process->lock, prev_interrupt_state);
        return alloc_ok;
}

void process_unmap_pages(struct Process *process, void *virtbase, size_t page_count) {
        ASSERT(is_aligned(PAGE_SIZE, (uintptr_t)virtbase));
        bool prev_interrupt_state;
        spinlock_lock(&process->lock, &prev_interrupt_state);
        uintptr_t current_virtbase = (uintptr_t)virtbase;
        for (size_t i = 0; i < page_count; ++i, current_virtbase += PAGE_SIZE) {
                mmu_unmap(process->addrspace, (void *)current_virtbase);
        }
        virtzone_free_region(&process->virtzone, virtbase, page_count);
        spinlock_unlock(&process->lock, prev_interrupt_state);
}

void process_set_map_options(struct Process *process, void *virtbase, size_t page_count, struct Proc_MapOptions options) {
        ASSERT(is_aligned(PAGE_SIZE, (uintptr_t)virtbase));
        bool prev_interrupt_state;
        spinlock_lock(&process->lock, &prev_interrupt_state);
        uint8_t *next_virtaddr = virtbase;
        mmu_prot_t prot_flags = make_mmu_prot_flags(process, options);
        for (size_t i = 0; i < page_count; ++i, next_virtaddr += PAGE_SIZE) {
                mmu_update_options(
                        process->addrspace, next_virtaddr, prot_flags
                );
        }
        spinlock_unlock(&process->lock, prev_interrupt_state);
}

WARN_UNUSED_RESULT void *process_alloc_pages(struct Process *process, uintptr_t *paddr_out, size_t page_count, struct Proc_MapOptions options) {
        // TODO: Allocate one page at a time, instead of multiple at once!
        struct PhysPage_Addr paddr = physpage_alloc(page_count);
        void *vaddr = NULL;
        if (!paddr.value) {
                goto fail;
        }
        vaddr = process_map_pages(process, paddr.value, page_count, options);
        if (!vaddr) {
                goto fail;
        }
        *paddr_out = paddr.value;
        return vaddr;
fail:
        if (vaddr) {
                process_unmap_pages(process, vaddr, page_count);
                vaddr = NULL;
        }
        if (paddr.value) {
                physpage_free(paddr, page_count);
        }
        return vaddr;
}

void process_free_pages(struct Process *process, void *ptr, size_t page_count) {
        uintptr_t physpage = mmu_virt_to_phys(ptr);
        process_unmap_pages(process, ptr, page_count);
        physpage_free((struct PhysPage_Addr){physpage}, page_count);
}

void process_activate_user_addrspace(struct Process *process) {
        ASSERT(!process_is_kernel(process));
        mmu_activate_user_vm_addrspace(process->addrspace);
}

struct Process *process_spawn_user(char const *name) {
        mmu_addrspace_t mmu_addrspace = mmu_addrspace_create();
        if (mmu_addrspace == MMU_ADDRSPACE_INVALID) {
                mmu_addrspace_delete(mmu_addrspace);
                return NULL;
        }
        return do_spawn(name, mmu_addrspace, false);
}

WARN_UNUSED_RESULT ssize_t process_fd_write(struct Process *process, int fd, void const *buf, size_t count) {
        if (SSIZE_MAX < count) {
                return -EINVAL;
        }
        ssize_t result;
        bool prev_interrupt_state;
        spinlock_lock(&process->lock, &prev_interrupt_state);
        switch (fd) {
        case STDIN_FILENO:
                result = -EBADF;
                goto out;
        case STDOUT_FILENO:
        case STDERR_FILENO:
                for (size_t i = 0; i < count; ++i) {
                        console_put_char(((char *)buf)[i]);
                }
                result = (ssize_t)count;
                console_flush();
                break;
        default:
                result = -EBADF;
                goto out;
        }

out:
        spinlock_unlock(&process->lock, prev_interrupt_state);
        return result;
}

WARN_UNUSED_RESULT ssize_t process_fd_read(struct Process *process, int fd, void *buf, size_t count) {
        if (SSIZE_MAX < count) {
                return -EINVAL;
        }
        ssize_t result;
        bool prev_interrupt_state;
        spinlock_lock(&process->lock, &prev_interrupt_state);
        switch (fd) {
        case STDIN_FILENO:
                result = (ssize_t)console_get_line(buf, count, true);
                break;
        case STDOUT_FILENO:
        case STDERR_FILENO:
        default:
                result = -EBADF;
                goto out;
        }

out:
        spinlock_unlock(&process->lock, prev_interrupt_state);
        return result;
}

static pid_t s_next_pid = 0;
static struct Process *s_kernel_process;

WARN_UNUSED_RESULT static struct Process *do_spawn(char const *name, mmu_addrspace_t addrspace, bool is_kernel_process) {
        struct Process *process = kmalloc(sizeof(*process));
        kmemset(process, 0, sizeof(*process));
        if (!process) {
                TODO_HANDLE_ERROR();
        }
        str_copy(process->name, sizeof(process->name), name);
        process->addrspace = addrspace;
        if (is_kernel_process) {
                virtzone_init(
                        &process->virtzone,
                        KERNEL_VM_VIRTBASE,
                        KERNEL_VM_VIRTEND
                );
        } else {
                virtzone_init(
                        &process->virtzone, USER_VM_VIRTBASE, USER_VM_VIRTEND
                );
        }
        {
                bool prev_interrupt_state;
                spinlock_lock(&process->lock, &prev_interrupt_state);
                process->id = s_next_pid;
                if (process->id >= PROCESS_ID_MAX) {
                        TODO_HANDLE_ERROR();
                }
                ++s_next_pid;
                spinlock_unlock(&process->lock, prev_interrupt_state);
        }
        return process;
}

struct Process *process_kernel(void) { return s_kernel_process; }

void process_spawn_kernel(mmu_addrspace_t mmu_addrspace) {
        s_kernel_process = do_spawn("kernel", mmu_addrspace, true);
}

bool process_is_kernel(struct Process const *process) {
        return process == s_kernel_process;
}
