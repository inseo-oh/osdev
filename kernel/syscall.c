// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "syscall.h"
#include <errno.h>
#include "kernel/arch/arch.h"
#include "kernel/heap/heap.h"
#include "kernel/kernel.h"
#include "kernel/utility/utility.h"
#include "kernel/tasks/tasks.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static bool is_accessible(void const *buf, size_t size, mmu_prot_t prot_flags) {
        uintptr_t begin_addr = align_down(PAGE_SIZE, (uintptr_t)buf);
        uintptr_t end_addr = align_up(PAGE_SIZE, (uintptr_t)buf + size);
        for (uintptr_t addr = begin_addr; addr < end_addr; addr += PAGE_SIZE) {
                bool is_accessible = mmu_is_accessible((void *)addr, prot_flags);
                if (!is_accessible) {
                        return false;
                }
        }
        return true;
}

static void fatal_oom() { TODO(); }

// Returned buffer must be freed using kfree() after using it.
static int64_t copy_from_user(void **out, void const *u_buf, size_t u_size, bool enomem_allowed) {
        if (!is_accessible(u_buf, u_size, MMU_PROT_USER)) {
                return -EFAULT;
        }
        *out = kmalloc(u_size);
        if (!(*out)) {
                if (!enomem_allowed) {
                        fatal_oom();
                }
                return -ENOMEM;
        }
        uaccess_begin();
        kmemcpy(*out, u_buf, u_size);
        uaccess_end();
        return 0;
}

static int64_t copy_to_user(void *u_dest, void const *buf, size_t size) {
        if (!is_accessible(u_dest, size, MMU_PROT_USER | MMU_PROT_WRITE)) {
                return -EFAULT;
        }
        uaccess_begin();
        kmemcpy(u_dest, buf, size);
        uaccess_end();
        return 0;
}

void syscall_impl_sched_yield(void) { scheduler_yield(); }

int64_t syscall_impl_write(int u_fd, void const *u_buf, size_t u_count) {
        void *buf;
        int64_t result;
        result = copy_from_user(&buf, u_buf, u_count, false);
        if (result < 0) {
                buf = NULL;
                goto out;
        }
        result = process_fd_write(process_running(), u_fd, buf, u_count);
        if (result < 0) {
                goto out;
        }
out:
        kfree(buf);
        return result;
}

int64_t syscall_impl_read(int u_fd, void *u_buf, size_t u_count) {
        void *buf = kmalloc(u_count);
        if (!buf) {
                fatal_oom();
        }
        int64_t result;
        result = process_fd_read(process_running(), u_fd, buf, u_count);
        if (result < 0) {
                goto out;
        }
        result = copy_to_user(u_buf, buf, u_count);
        if (result < 0) {
                goto out;
        }
out:
        if (buf) {
                kfree(buf);
        }
        return result;
}

int64_t syscall_impl_dprint(void const *u_buf, size_t u_count) {
        void *buf;
        int64_t result = copy_from_user(&buf, u_buf, u_count, false);
        if (result < 0) {
                buf = NULL;
                goto out;
        }
        for (size_t i = 0; i < u_count; i++) {
                console_put_char(((char *)buf)[i]);
        }
out:
        kfree(buf);
        return 0;
}
