// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "kernel.h"
#include "kernel/arch/arch.h"
#include "kernel/tasks/tasks.h"

static tick_t gs_tick_count = 0;

void ticktime_increment_tick() {
        if (processor_current()->flags & PROCESSOR_LOCALSTATE_FLAG_BSP) {
                ++gs_tick_count;
        }
        scheduler_on_timer_tick();
}

tick_t ticktime_get_count() { return gs_tick_count; }
