// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "spinlock.h"
#include <kernel/arch/arch.h>
#include <kernel/kernel.h>
#include <stdbool.h>
#ifdef SPINLOCK_STORE_LOCKED_LOCATION
#include <stddef.h>
#endif

#ifdef SPINLOCK_STORE_LOCKED_LOCATION
bool spinlock_try_lock_impl(
        struct SpinLock *lock,
        bool *prev_interrupt_state_out,
        char const *file,
        int line
) {
#else
bool spinlock_try_lock(struct SpinLock *lock, bool *prev_interrupt_state_out) {
#endif
        *prev_interrupt_state_out = interrupts_disable();
        bool expected = false;
        if (!__atomic_compare_exchange_n(
                    &lock->locked,
                    &expected,
                    true,
                    false,
                    __ATOMIC_ACQUIRE,
                    __ATOMIC_RELAXED
            )) {
                if (*prev_interrupt_state_out) {
                        interrupts_enable();
                }
                return false;
        }
        lock->lockloc_file = file;
        lock->lockloc_line = line;
        return true;
}

#ifdef SPINLOCK_STORE_LOCKED_LOCATION
void spinlock_lock_impl(
        struct SpinLock *lock,
        bool *prev_interrupt_state_out,
        char const *file,
        int line
) {
#else
void spinlock_lock(struct SpinLock *lock, bool *prev_interrupt_state_out) {
#endif
#ifdef SPINLOCK_STORE_LOCKED_LOCATION
        while (!spinlock_try_lock_impl(
                lock, prev_interrupt_state_out, file, line
        )) {
#else
        while (!spinlock_try_lock(lock, prev_interrupt_state_out)) {
#endif
                processor_wait_during_spinloop();
        }
}

void spinlock_unlock(struct SpinLock *lock, bool prev_interrupt_state) {
        ASSERT(!interrupts_are_enabled());
        ASSERT(lock->locked);
#ifdef SPINLOCK_STORE_LOCKED_LOCATION
        lock->lockloc_file = NULL;
        lock->lockloc_line = 0;
#endif
        __atomic_store_n(&lock->locked, false, __ATOMIC_RELEASE);
        if (prev_interrupt_state) {
                interrupts_enable();
        }
}

void spinlock_unlock_without_restoring_interrupt(struct SpinLock *lock) {
        ASSERT(!interrupts_are_enabled());
        ASSERT(lock->locked);
        __atomic_store_n(&lock->locked, false, __ATOMIC_RELEASE);
}
