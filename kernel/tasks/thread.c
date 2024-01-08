// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "tasks.h"
#include <kernel/arch/arch.h>
#include <kernel/heap/heap.h>
#include <kernel/kernel.h>
#include <kernel/utility/utility.h>
#include <kernel/lock/mutex.h>
#include <kernel/memory/memory.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct Thread {
        struct Processor_Thread processor_thread;
        uintptr_t stack_physbase;
        void (*entry_point)();
        struct Process *parent_proc;
        struct Mutex *waiting_mutex;
        tid_t id;
        char name[THREAD_NAME_MAX_LEN + 1];
        bool sleep_scheduled, is_entering_for_first_time;
};

struct Thread *thread_running(void) {
        ENTER_NO_INTERRUPT_SECTION();
        struct Thread *thread = processor_running_thread(processor_current());
        LEAVE_NO_INTERRUPT_SECTION();
        return thread;
}

#define MAX_THREAD_TIME         5
#define THREAD_STACK_SIZE       (1024UL * 1024UL * 4UL)
#define THREAD_STACK_PAGE_COUNT (THREAD_STACK_SIZE / PAGE_SIZE)

struct Thread *thread_create(
        struct Process *parent_process, char const *name, void (*entry_point)()
) {
        ASSERT(parent_process);
        struct PhysPage_Addr stack_physpage =
                physpage_alloc(THREAD_STACK_PAGE_COUNT);
        if (!stack_physpage.value) {
                TODO_HANDLE_ERROR();
        }
        struct Thread *thread = kmalloc(sizeof(*thread));
        if (!thread) {
                TODO_HANDLE_ERROR();
        }
        str_copy(thread->name, sizeof(thread->name), name);
        thread->sleep_scheduled = false;
        thread->is_entering_for_first_time = true;
        thread->entry_point = entry_point;
        thread->stack_physbase = stack_physpage.value;
        thread->parent_proc = parent_process;
        thread->waiting_mutex = NULL;
        void *stack_base_virtaddr = process_map_pages(
                parent_process,
                stack_physpage.value,
                THREAD_STACK_PAGE_COUNT,
                (struct Proc_MapOptions){.executable = false, .writable = true}
        );
        if (!stack_base_virtaddr) {
                TODO_HANDLE_ERROR();
        }
        void *stack_top = (uint8_t *)stack_base_virtaddr + THREAD_STACK_SIZE;
        ASSERT((void *)stack_base_virtaddr != stack_top);
        if (!processor_thread_init(&thread->processor_thread, stack_top)) {
                TODO_HANDLE_ERROR();
        }
        thread->id = process_add_thread(parent_process, thread);
        return thread;
}

struct Thread *thread_spawn(
        struct Process *parent_process, char const *name, void (*entry_point)()
) {
        struct Thread *thread =
                thread_create(parent_process, name, entry_point);
        if (!thread) {
                return NULL;
        }
        scheduler_add_thread_to_wait_queue(thread);
        return thread;
}

char const *thread_get_name(struct Thread const *thread) {
        return thread->name;
}

tid_t thread_get_id(struct Thread const *thread) { return thread->id; }

void thread_set_sleep_scheduled(struct Thread *thread, bool scheduled) {
        thread->sleep_scheduled = scheduled;
}

bool thread_is_sleep_scheduled(struct Thread const *thread) {
        return thread->sleep_scheduled;
}

void thread_set_waiting_mutex(struct Thread *thread, struct Mutex *mutex) {
        thread->waiting_mutex = mutex;
}

struct Mutex *thread_get_waiting_mutex(struct Thread const *thread) {
        return thread->waiting_mutex;
}

struct Process *thread_get_parent_proc(struct Thread const *thread) {
        return thread->parent_proc;
}

void thread_enter_initial_kernel_thread(struct Thread *thread) {
        ASSERT(!interrupts_are_enabled());
        processor_set_running_thread(processor_current(), thread);
        ASSERT(thread->is_entering_for_first_time);
        thread->is_entering_for_first_time = false;
        processor_thread_enter_initial_kernel_thread(
                &thread->processor_thread, thread->entry_point
        );
}

void thread_context_switch(
        struct Thread *from_thread, struct Thread *to_thread
) {
        ASSERT(from_thread != to_thread);
        ENTER_NO_INTERRUPT_SECTION();
        {
                struct Processor_LocalState *cpu = processor_current();
                bool is_user_thread =
                        !process_is_kernel(to_thread->parent_proc);
                if (is_user_thread) {
                        process_activate_user_addrspace(to_thread->parent_proc);
                } else {
                        mmu_deactivate_user_vm_addrspace();
                }
                processor_set_running_thread(cpu, to_thread);
                if (to_thread->is_entering_for_first_time) {
                        to_thread->is_entering_for_first_time = false;
                        // NOTE: SpinLock will be unlocked by
                        // processor_thread_enter, through
                        // `scheduler_about_to_enter_new_thread()`
                        processor_thread_enter(
                                &from_thread->processor_thread,
                                &to_thread->processor_thread,
                                is_user_thread,
                                to_thread->entry_point
                        );
                } else {
                        processor_thread_context_switch(
                                &from_thread->processor_thread,
                                &to_thread->processor_thread,
                                is_user_thread
                        );
                }
        }
        LEAVE_NO_INTERRUPT_SECTION();
}
