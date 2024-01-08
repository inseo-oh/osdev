// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#pragma once
#include <kernel/lock/spinlock.h>

struct Thread;

struct Mutex *mutex_new();
bool mutex_try_lock(struct Mutex *mutex);
// Below is only designed to be used with Scheduler.
bool mutex_try_lock_with_owner(struct Mutex *mutex, struct Thread *lock_owner);
void mutex_lock(struct Mutex *mutex);
void mutex_unlock(struct Mutex *mutex);
bool mutex_is_locked(struct Mutex const *mutex);
