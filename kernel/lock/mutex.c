// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "mutex.h"
#include "kernel/heap/heap.h"
#include "kernel/kernel.h"
#include "kernel/utility/utility.h"
#include "kernel/tasks/tasks.h"
#include <stdbool.h>
#include <stddef.h>

struct Mutex {
        struct AVLTree_Node node_head;
        bool locked;
        struct Thread *lock_owner;
};

struct Mutex *mutex_new() {
        struct Mutex *mutex = kmalloc(sizeof(*mutex));
        if (!mutex) {
                TODO_HANDLE_ERROR();
        }
        mutex->locked = false;
        mutex->lock_owner = NULL;
        return mutex;
}

bool mutex_try_lock(struct Mutex *mutex) {
        return mutex_try_lock_with_owner(mutex, thread_running());
}

bool mutex_try_lock_with_owner(struct Mutex *mutex, struct Thread *lock_owner) {
        bool expected = false;
        bool result = __atomic_compare_exchange_n(
                &mutex->locked,
                &expected,
                true,
                false,
                __ATOMIC_ACQUIRE,
                __ATOMIC_RELAXED
        );
        if (result) {
                mutex->lock_owner = lock_owner;
        } else if (mutex->lock_owner == lock_owner) {
                panic("NESTED LOCK! Owner attempted to lock mutex locked by "
                      "owner thread",
                      mutex);
        }
        return result;
}

void mutex_lock(struct Mutex *mutex) {
        if (mutex_try_lock(mutex)) {
                return;
        }
        scheduler_sleep_until_mutex_unlock(mutex);
}

void mutex_unlock(struct Mutex *mutex) {
        if (!mutex->locked) {
                panic("Attempted to unlock non-locked mutex %p", mutex);
        }
        if (mutex->lock_owner != thread_running()) {
                panic("Attempted to unlock mutex %p locked by thread %p, but "
                      "current thread is "
                      "%p",
                      mutex,
                      mutex->lock_owner,
                      thread_running());
        }
        mutex->lock_owner = NULL;
        __atomic_store_n(&mutex->locked, false, __ATOMIC_RELEASE);
}

bool mutex_is_locked(struct Mutex const *mutex) { return mutex->locked; }
