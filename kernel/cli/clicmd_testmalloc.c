// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "cliarg.h"
#include "clicmd.h"
#include <kernel/arch/arch.h>
#include <kernel/heap/heap.h>
#include <kernel/kernel.h>
#include <kernel/tasks/tasks.h>
#include <kernel/utility/utility.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

static char const *LOG_TAG = "testmalloc";

static void *(*s_malloc_fn)(size_t);
static void (*s_free_fn)(void *);
static atomic_uint s_task_counter = 0;

static void *make_random_alloc(size_t *size_out) {
        while (1) {
                *size_out = ticktime_get_count() % (32UL * 1024) + 1;
                void *result = s_malloc_fn(*size_out);
                if (result) {
                        return result;
                }
        }
}

static bool address_collides(
        uintptr_t begin_a, uintptr_t end_a, uintptr_t begin_b, uintptr_t end_b
) {
        if ((begin_b <= begin_a) && (begin_a < end_b)) {
                return true;
        }
        if ((begin_b <= end_a) && (end_a < end_b)) {
                return true;
        }
        if ((begin_a <= begin_b) && (begin_b < end_a)) {
                return true;
        }
        if ((begin_a <= end_b) && (end_b < end_a)) {
                return true;
        }
        return false;
}

static void test_equal(
        char const *test_name,
        uint8_t **allocs,
        uint8_t expected,
        unsigned task_id,
        unsigned alloc,
        unsigned offset
) {
        unsigned got = allocs[alloc][offset];
        if (expected != got) {
                LOGE(LOG_TAG,
                     "[%u] %s FAIL: ALLOC %u, OFFSET %u: expected %u, got %u",
                     test_name,
                     task_id,
                     alloc,
                     offset,
                     expected,
                     got);
                panic("Memory test failed");
        }
}

static void run_single_pass(unsigned task_id) {
        enum { ALLOC_COUNT = 10 };

        size_t alloc_sizes[ALLOC_COUNT];
        uint8_t *allocs[ALLOC_COUNT];
        for (unsigned alloc = 0; alloc < ALLOC_COUNT; ++alloc) {
                allocs[alloc] = make_random_alloc(&alloc_sizes[alloc]);
        }
        // Address collision test
        for (unsigned alloc_a = 0; alloc_a < ALLOC_COUNT; ++alloc_a) {
                uintptr_t begin_a = (uintptr_t)allocs[alloc_a];
                uintptr_t end_a = begin_a + alloc_sizes[alloc_a];
                for (unsigned alloc_b = 0; alloc_b < ALLOC_COUNT; ++alloc_b) {
                        if (alloc_a == alloc_b) {
                                continue;
                        }
                        uintptr_t begin_b = (uintptr_t)allocs[alloc_b];
                        uintptr_t end_b = begin_b + alloc_sizes[alloc_b];
                        if (address_collides(begin_a, end_a, begin_b, end_b)) {
                                LOGE(LOG_TAG,
                                     "[%u] Address collision test FAIL: ALLOC "
                                     "A %u @ %p~%p, ALLOC B %u @ %p~%p",
                                     task_id,
                                     alloc_a,
                                     begin_a,
                                     end_a - 1,
                                     alloc_b,
                                     begin_b,
                                     end_b - 1);
                                panic("Memory test failed");
                        }
                }
        }
        // Same byte fill test
        for (uint16_t b = 0; b <= 0xff; ++b) {
                for (unsigned alloc = 0; alloc < ALLOC_COUNT; ++alloc) {
                        size_t byte_count = alloc_sizes[alloc];
                        // We fill the memory without kmemset() to eliminate
                        // potential source of bug.
                        for (size_t offset = 0; offset < byte_count; ++offset) {
                                allocs[alloc][offset] = b;
                        }
                }
                for (unsigned alloc = 0; alloc < ALLOC_COUNT; ++alloc) {
                        size_t byte_count = alloc_sizes[alloc];
                        for (size_t offset = 0; offset < byte_count; ++offset) {
                                test_equal(
                                        "Same byte fill test",
                                        allocs,
                                        b,
                                        task_id,
                                        alloc,
                                        offset
                                );
                        }
                }
        }
        // Random fill test
        for (unsigned alloc = 0; alloc < ALLOC_COUNT; ++alloc) {
                size_t byte_count = alloc_sizes[alloc];
                for (size_t offset = 0; offset < byte_count; ++offset) {
                        allocs[alloc][offset] = offset & 0xFF;
                }
        }
        for (unsigned alloc = 0; alloc < ALLOC_COUNT; ++alloc) {
                size_t byte_count = alloc_sizes[alloc];
                for (size_t offset = 0; offset < byte_count; ++offset) {
                        unsigned expected = offset & 0xFF;
                        test_equal(
                                "Random fill test",
                                allocs,
                                expected,
                                task_id,
                                alloc,
                                offset
                        );
                }
                // And free it
                s_free_fn(allocs[alloc]);
        }
}

static void run_test(void) {
        interrupts_enable();

        unsigned task_id = ++s_task_counter;
        LOGI(LOG_TAG, "[%u] Test started", task_id);
        unsigned pass_count = 0;
        while (1) {
                run_single_pass(task_id);
                ++pass_count;
                LOGI(LOG_TAG, "[%u] Test OK (Pass %u)", task_id, pass_count);
        }
}

static void cmd_main(char *arg_str) {
        char type_buf[9];
        unsigned remaining_thread_count;
        if (cliarg_next_str(type_buf, sizeof(type_buf), &arg_str) <= 0) {
                goto bad_args;
        }
        if (str_equals(type_buf, "vmmalloc")) {
                s_malloc_fn = vmmalloc;
                s_free_fn = vmfree;
        } else if (str_equals(type_buf, "kmalloc")) {
                s_malloc_fn = kmalloc;
                s_free_fn = kfree;
        } else {
                goto bad_args;
        }
        if (!cliarg_next_unsigned(&remaining_thread_count, &arg_str)) {
                goto bad_args;
        }
        if (remaining_thread_count == 0) {
                return;
        }
        while (remaining_thread_count != 1) {
                thread_spawn(process_running(), "testmalloc thread", run_test);
                --remaining_thread_count;
        }
        run_test();
        return;
bad_args:
        console_alert("Bad arguments");
}

static const struct CliCmd_ArgHelp ARG_HELP[] = {
        {
                .name = "<type>",
                .help = "Specifies malloc type. Options are kmalloc or "
                        "vmmalloc.",
        },
        {
                .name = "<threads>",
                .help = "Specifies thread count. 1 runs test without spawning "
                        "threads.",
        },
        {0, 0},
};

const struct CliCmd_Descriptor CLICMD_TESTMALLOC = {
        .name = "testmalloc",
        .fn = cmd_main,
        .description = "Runs malloc tests",
        .args_help = ARG_HELP,
};
