// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "tasks.h"
#include <kernel/arch/arch.h>
#include <kernel/heap/heap.h>
#include <kernel/kernel.h>
#include <kernel/lock/mutex.h>
#include <kernel/lock/spinlock.h>
#include <kernel/utility/utility.h>
#include <stdbool.h>
#include <stddef.h>

struct ThreadEntry {
        struct List_Node node_head;
        struct Thread *thread;
};

static tick_t const MAX_THREAD_TIME = 5;

static void enqueue_thread(struct List *queue, struct Thread *thread) {
        struct ThreadEntry *queued_thread = kmalloc(sizeof(*queued_thread));
        if (!queued_thread) {
                panic("Not enough kmalloc memory to enqueue a thread");
        }
        queued_thread->thread = thread;
        list_insert_head(queue, &queued_thread->node_head);
}

static struct Thread *
remove_thread_from_queue(struct List *queue, struct ThreadEntry *entry) {
        list_remove(queue, &entry->node_head);
        struct Thread *result = entry->thread;
        ASSERT(result);
        kfree(entry);
        return result;
}

// Returns NULL if thread couldn't be found
WARN_UNUSED_RESULT static struct ThreadEntry *
find_thread_in_queue(struct List *queue, struct Thread *thread) {
        for (struct ThreadEntry *entry = (struct ThreadEntry *)queue->head;
             entry;
             entry = entry->node_head.next) {
                if (entry->thread == thread) {
                        return entry;
                }
        }
        return NULL;
}

// Returns `NULL` if there's no thread to dequeue.
WARN_UNUSED_RESULT static struct Thread *dequeue_thread(struct List *queue) {
        struct ThreadEntry *queued_thread = (struct ThreadEntry *)queue->tail;
        if (!queued_thread) {
                return NULL;
        }
        return remove_thread_from_queue(queue, queued_thread);
}

struct List s_schedule_wait_queue;
struct List s_sleeping_threads;
// TODO: Move this to processor-local structure
tick_t s_remaining_thread_time;
static struct SpinLock s_lock;

static void add_thread_to_wait_queue(struct Thread *thread) {
        ASSERT(thread);
        enqueue_thread(&s_schedule_wait_queue, thread);
}

static void wakeup_thread(struct Thread *thread) {
        struct ThreadEntry *entry =
                find_thread_in_queue(&s_sleeping_threads, thread);
        if (!entry) {
                panic("Attempted to wake up non-existing or non-sleeping "
                      "thread %u",
                      thread);
        }
        remove_thread_from_queue(&s_sleeping_threads, entry);
        thread_set_sleep_scheduled(thread, false);
        add_thread_to_wait_queue(thread);
}

void scheduler_sleep_until_mutex_unlock(struct Mutex *mutex) {
        struct Thread *thread = thread_running();
        thread_set_sleep_scheduled(thread, true);
        thread_set_waiting_mutex(thread, mutex);
        scheduler_yield();
}

static void wakeup_mutex_lock_successful_threads(void) {
        struct ThreadEntry *next_entry = NULL;
        for (struct ThreadEntry *entry =
                     (struct ThreadEntry *)s_sleeping_threads.head;
             entry;
             entry = next_entry) {
                // We must remember what the next thread was, because if we do
                // end up waking up a thread, the current entry will be deleted.
                next_entry = entry->node_head.next;
                struct Mutex *mutex = thread_get_waiting_mutex(entry->thread);
                if (!mutex) {
                        continue;
                }
                if (!mutex_try_lock_with_owner(mutex, entry->thread)) {
                        continue;
                }
                thread_set_waiting_mutex(entry->thread, NULL);
                wakeup_thread(entry->thread);
        }
}

// Returns `NULL` if there's no next thread to run
WARN_UNUSED_RESULT static struct Thread *next_thread_to_run(void) {
        return dequeue_thread(&s_schedule_wait_queue);
}

void scheduler_add_thread_to_wait_queue(struct Thread *thread) {
        ASSERT(thread);
        bool prev_interrupt_state;
        spinlock_lock(&s_lock, &prev_interrupt_state);
        add_thread_to_wait_queue(thread);
        spinlock_unlock(&s_lock, prev_interrupt_state);
}

void scheduler_wakeup_thread(struct Thread *thread) {
        bool prev_interrupt_state;
        spinlock_lock(&s_lock, &prev_interrupt_state);
        wakeup_thread(thread);
        spinlock_unlock(&s_lock, prev_interrupt_state);
}

void scheduler_yield(void) {
        bool prev_interrupt_state;
        spinlock_lock(&s_lock, &prev_interrupt_state);
        wakeup_mutex_lock_successful_threads();
        struct Processor_LocalState *processor = processor_current();
        struct Thread *to_thread = next_thread_to_run();
        if (to_thread) {
                struct Thread *from_thread =
                        processor_running_thread(processor);
                ASSERT(from_thread);
                ASSERT(from_thread != to_thread);
                bool is_sleep_scheduled =
                        thread_is_sleep_scheduled(from_thread);
                if (is_sleep_scheduled) {
                        thread_set_sleep_scheduled(from_thread, false);
                        enqueue_thread(&s_sleeping_threads, from_thread);
                } else {
                        add_thread_to_wait_queue(from_thread);
                }
                thread_context_switch(from_thread, to_thread);
        }
        spinlock_unlock(&s_lock, prev_interrupt_state);
}

void scheduler_on_timer_tick(void) {
        ASSERT(!interrupts_are_enabled());
        bool need_switch = false;
        {
                bool prev_interrupt_state;
                spinlock_lock(&s_lock, &prev_interrupt_state);
                if (--s_remaining_thread_time <= 0) {
                        s_remaining_thread_time = MAX_THREAD_TIME;
                        need_switch = true;
                }
                spinlock_unlock(&s_lock, prev_interrupt_state);
        }
        if (need_switch) {
                scheduler_yield();
        }
}

void scheduler_about_to_enter_new_thread(void) {
        spinlock_unlock_without_restoring_interrupt(&s_lock);
}

void scheduler_init_for_bsp(void (*thread_entry)()) {
        s_remaining_thread_time = MAX_THREAD_TIME;

        struct Thread *kernel_boot_thread =
                thread_create(process_kernel(), "boot/idle(bsp)", thread_entry);
        if (!kernel_boot_thread) {
                panic("Failed to spawn kernel boot thread");
        }
        // Scheduler gets unlocked when entering a new thread, so it needs to be
        // locked first.
        bool prev_interrupt_state;
        spinlock_lock(&s_lock, &prev_interrupt_state);
        thread_enter_initial_kernel_thread(kernel_boot_thread);
}

void scheduler_init_for_ap(void (*thread_entry)()) {
        struct Thread *kernel_boot_thread =
                thread_create(process_kernel(), "boot/idle(ap)", thread_entry);
        if (!kernel_boot_thread) {
                panic("Failed to spawn kernel boot thread");
        }
        // Scheduler gets unlocked when entering a new thread, so it needs to be
        // locked first.
        bool prev_interrupt_state;
        spinlock_lock(&s_lock, &prev_interrupt_state);
        thread_enter_initial_kernel_thread(kernel_boot_thread);
}

void scheduler_run_idle_loop() {
        while (1) {
                interrupts_wait();
                scheduler_yield();
        }
}
