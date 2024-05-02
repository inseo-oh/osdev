
// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#pragma once
#include "kernel/arch/arch.h"
#include "kernel/lock/mutex.h"
#include "kernel/utility/utility.h"
#include <stdbool.h>
#include <stddef.h>
// XXX: This is temporary workaround until we fully move to C++
#ifdef NORETURN_WORKAROUND
#define NORETURN [[noreturn]]
#else
#include <stdnoreturn.h>
#define NORETURN noreturn
#endif
#include <unistd.h>

////////////////////////////////////////////////////////////////////////////////
// Threads
////////////////////////////////////////////////////////////////////////////////

#define THREAD_ID_MAX       65535
#define THREAD_ID_INVALID   (THREAD_ID_MAX + 1)
#define THREAD_NAME_MAX_LEN 32

typedef avltree_key_t tid_t;

struct Thread;

struct Thread *thread_running(void);
char const *thread_get_name(struct Thread const *thread);
tid_t thread_get_id(struct Thread const *thread);
void thread_set_sleep_scheduled(struct Thread *thread, bool scheduled);
bool thread_is_sleep_scheduled(struct Thread const *thread);
void thread_set_waiting_mutex(struct Thread *thread, struct Mutex *mutex);
struct Mutex *thread_get_waiting_mutex(struct Thread const *thread);
struct Process *thread_get_parent_proc(struct Thread const *thread);
NORETURN void thread_enter_initial_kernel_thread(struct Thread *thread);
void thread_context_switch(struct Thread *from_thread, struct Thread *to_thread);
// Similar to thread_spawn, but doesn't add thread to the scheduler.
struct Thread *thread_create(struct Process *parent_process, char const *name, void (*entry_point)());
struct Thread *thread_spawn(struct Process *parent_proc, char const *name, void (*entry_point)());

////////////////////////////////////////////////////////////////////////////////
// Processes
////////////////////////////////////////////////////////////////////////////////

#define PROCESS_ID_MAX     65535
#define PROCESS_ID_INVALID (PROCESS_ID_MAX + 1)
#define PROC_NAME_MAX_LEN  32

struct Proc_MapOptions {
        bool writable;
        bool executable;
};

struct Process;

struct Process *process_running(void);
struct Process *process_kernel(void);
char const *process_get_name(struct Process const *process);
pid_t process_get_id(struct Process const *process);
bool process_is_kernel(struct Process const *process);
// Returns THREAD_ID_INVALID on OOM.
tid_t process_add_thread(struct Process *process, struct Thread *thread);
void process_activate_user_addrspace(struct Process *process);
void process_spawn_kernel(mmu_addrspace_t mmu_addrspace);
// Returns NULL on OOM.
WARN_UNUSED_RESULT struct Process *process_spawn_user(char const *name);

// Returns negative errno on error;
WARN_UNUSED_RESULT ssize_t process_fd_write(struct Process *process, int fd, void const *buf, size_t count);
// Returns negative errno on error;
WARN_UNUSED_RESULT ssize_t process_fd_read(struct Process *process, int fd, void *buf, size_t count);

// Performs mapping of possibly non-aligned physical pages. This down-aligns
// physaddr to page boundary, maps it, and then returns offset added to
// page-aligned virtual base address.
//
// Due to the way it works, unmapping must be done using
// `process_unmap_unaligned()`.
//
// Returns NULL on failure.
void *process_map_unaligned(struct Process *process, uintptr_t physaddr, size_t size, struct Proc_MapOptions options);
void process_unmap_unaligned(struct Process *process, void *base, size_t size);

// Returns NULL on failure.
WARN_UNUSED_RESULT void *process_map_pages(struct Process *process, uintptr_t physbase, size_t page_count, struct Proc_MapOptions options);
WARN_UNUSED_RESULT bool process_map_pages_at(struct Process *process, uintptr_t physbase, void *virtbase, size_t page_count, struct Proc_MapOptions options);
void process_unmap_pages(struct Process *process, void *virtbase, size_t page_count);
void process_set_map_options(struct Process *process,void *virtbase,size_t page_count,struct Proc_MapOptions options);
// Returns NULL on failure.
WARN_UNUSED_RESULT void *process_alloc_pages(struct Process *process, uintptr_t *paddr_out, size_t page_count, struct Proc_MapOptions options);
void process_free_pages(struct Process *process, void *ptr, size_t page_count);

////////////////////////////////////////////////////////////////////////////////
// Scheduling
////////////////////////////////////////////////////////////////////////////////

void scheduler_add_thread_to_wait_queue(struct Thread *thread);
// NOTE: Only used for context switching
USED void scheduler_about_to_enter_new_thread(void);
void scheduler_wakeup_thread(struct Thread *thread);
NORETURN void scheduler_init_for_bsp(void (*thread_entry)());
NORETURN void scheduler_init_for_ap(void (*thread_entry)());
NORETURN void scheduler_run_idle_loop();

// WARNING: Below functions lock the scheduler!
void scheduler_yield(void);
void scheduler_on_timer_tick(void);
void scheduler_sleep_until_mutex_unlock(struct Mutex *mutex_id);

////////////////////////////////////////////////////////////////////////////////
// Exec
////////////////////////////////////////////////////////////////////////////////

// Returns 0 on success, negative errno on failure.
int64_t exec(char const *name, void const *data, size_t size);