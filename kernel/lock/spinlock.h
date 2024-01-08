// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#pragma once
#include <kernel/utility/utility.h>
#include <stdbool.h>

#define SPINLOCK_STORE_LOCKED_LOCATION

struct SpinLock {
        bool locked;
#ifdef SPINLOCK_STORE_LOCKED_LOCATION
        char const *lockloc_file;
        int lockloc_line;
#endif
};

// Returns false if it cannot be locked.
#ifdef SPINLOCK_STORE_LOCKED_LOCATION
WARN_UNUSED_RESULT bool spinlock_try_lock_impl(
        struct SpinLock *lock,
        bool *prev_interrupt_state_out,
        char const *file,
        int line
);
#define spinlock_try_lock(_lock, _prev_interrupt_state_out)          \
        spinlock_try_lock_impl(                                      \
                _lock, _prev_interrupt_state_out, __FILE__, __LINE__ \
        )
#else
WARN_UNUSED_RESULT bool
spinlock_try_lock(struct SpinLock *lock, bool *prev_interrupt_state_out);
#endif

#ifdef SPINLOCK_STORE_LOCKED_LOCATION
void spinlock_lock_impl(
        struct SpinLock *lock,
        bool *prev_interrupt_state_out,
        char const *file,
        int line
);
#define spinlock_lock(_lock, _prev_interrupt_state_out) \
        spinlock_lock_impl(_lock, _prev_interrupt_state_out, __FILE__, __LINE__)
#else
void spinlock_lock(struct SpinLock *lock, bool *prev_interrupt_state_out);
#endif
void spinlock_unlock(struct SpinLock *lock, bool prev_interrupt_state);
// Don't use below function unless it's REALLY necessary
void spinlock_unlock_without_restoring_interrupt(struct SpinLock *lock);
