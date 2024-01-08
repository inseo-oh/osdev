// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "cliarg.h"
#include "clicmd.h"
#include <kernel/arch/arch.h>
#include <kernel/kernel.h>
#include <kernel/tasks/tasks.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static char const *LOG_TAG = "testpagealloc";
static atomic_uint s_task_counter = 0;
static bool s_failed = false;

static void *make_random_alloc(size_t *page_count_out, uintptr_t *paddr_out) {
        while (1) {
                *page_count_out = ticktime_get_count() % 8UL + 1;
                void *alloc = process_alloc_pages(
                        process_running(),
                        paddr_out,
                        *page_count_out,
                        (struct Proc_MapOptions
                        ){.writable = true, .executable = false}
                );
                if (alloc) {
                        return alloc;
                }
        }
}

static bool address_collides(
        uintptr_t begin_a, uintptr_t end_a, uintptr_t begin_b, uintptr_t end_b
) {
        if ((begin_b <= begin_a) && (begin_a < end_b)) {
                return true;
        }
        if ((begin_b < end_a) && (end_a < end_b)) {
                return true;
        }
        if ((begin_a <= begin_b) && (begin_b < end_a)) {
                return true;
        }
        if ((begin_a < end_b) && (end_b < end_a)) {
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
                s_failed = true;
                LOGE(LOG_TAG,
                     "[%u] %s FAIL: ALLOC %u, OFFSET %u: expected %u, got %u",
                     task_id,
                     test_name,
                     alloc,
                     offset,
                     expected,
                     got);
        }
}

static void run_single_pass(unsigned task_id) {
        enum { ALLOC_COUNT = 10 };

        size_t alloc_page_count[ALLOC_COUNT];
        uint8_t *allocs[ALLOC_COUNT];
        uintptr_t paddrs[ALLOC_COUNT];
        for (unsigned alloc = 0; alloc < ALLOC_COUNT; ++alloc) {
                allocs[alloc] = make_random_alloc(
                        &alloc_page_count[alloc], &paddrs[alloc]
                );
        }
        // Virtual address collision test
        LOGI(LOG_TAG, "[%u] Virtual address collision test", task_id);
        for (unsigned alloc_a = 0; alloc_a < ALLOC_COUNT; ++alloc_a) {
                uintptr_t begin_a = (uintptr_t)allocs[alloc_a];
                uintptr_t end_a =
                        begin_a + (alloc_page_count[alloc_a] * PAGE_SIZE);
                for (unsigned alloc_b = 0; alloc_b < ALLOC_COUNT; ++alloc_b) {
                        if (alloc_a == alloc_b) {
                                continue;
                        }
                        uintptr_t begin_b = (uintptr_t)allocs[alloc_b];
                        uintptr_t end_b = begin_b + (alloc_page_count[alloc_b] *
                                                     PAGE_SIZE);
                        if (address_collides(begin_a, end_a, begin_b, end_b)) {
                                s_failed = true;
                                LOGE(LOG_TAG,
                                     "[%u] VAddress collision: ALLOC A %u @ "
                                     "%p~%p, ALLOC B %u @ %p~%p",
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
        // Physical address collision test
        LOGI(LOG_TAG, "[%u] Physical address collision test", task_id);
        for (unsigned alloc_a = 0; alloc_a < ALLOC_COUNT; ++alloc_a) {
                uintptr_t begin_a = paddrs[alloc_a];
                uintptr_t end_a =
                        begin_a + (alloc_page_count[alloc_a] * PAGE_SIZE);
                for (unsigned alloc_b = 0; alloc_b < ALLOC_COUNT; ++alloc_b) {
                        if (alloc_a == alloc_b) {
                                continue;
                        }
                        uintptr_t begin_b = paddrs[alloc_b];
                        uintptr_t end_b = begin_b + (alloc_page_count[alloc_b] *
                                                     PAGE_SIZE);
                        if (address_collides(begin_a, end_a, begin_b, end_b)) {
                                s_failed = true;
                                LOGE(LOG_TAG,
                                     "[%u] PAddress collision: ALLOC A %u @ "
                                     "%p~%p, ALLOC B %u @ %p~%p",
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
        LOGI(LOG_TAG, "[%u] Same byte fill test", task_id);
        for (uint16_t b = 0; b <= 0xff; ++b) {
                for (unsigned alloc = 0; alloc < ALLOC_COUNT; ++alloc) {
                        size_t byte_count = alloc_page_count[alloc] * PAGE_SIZE;
                        // We fill the memory without kmemset() to eliminate
                        // potential source of bug.
                        for (size_t offset = 0; offset < byte_count; ++offset) {
                                allocs[alloc][offset] = b;
                        }
                }
                for (unsigned alloc = 0; alloc < ALLOC_COUNT; ++alloc) {
                        size_t byte_count = alloc_page_count[alloc] * PAGE_SIZE;
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
        LOGI(LOG_TAG, "[%u] Random fill test(Write)", task_id);
        for (unsigned alloc = 0; alloc < ALLOC_COUNT; ++alloc) {
                size_t byte_count = alloc_page_count[alloc] * PAGE_SIZE;
                for (size_t offset = 0; offset < byte_count; ++offset) {
                        allocs[alloc][offset] = offset & 0xFF;
                }
        }
        LOGI(LOG_TAG, "[%u] Random fill test(Read)", task_id);
        for (unsigned alloc = 0; alloc < ALLOC_COUNT; ++alloc) {
                size_t byte_count = alloc_page_count[alloc] * PAGE_SIZE;
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
        }
        // Physical address mapping test
        LOGI(LOG_TAG, "[%u] Physical address mapping test", task_id);
        for (unsigned alloc = 0; alloc < ALLOC_COUNT; ++alloc) {
                uintptr_t vaddr = (uintptr_t)allocs[alloc];
                uintptr_t expected_paddr = paddrs[alloc];
                for (unsigned i = 0; i < alloc_page_count[alloc];
                     ++i, vaddr += PAGE_SIZE, expected_paddr += PAGE_SIZE) {
                        uintptr_t current_paddr =
                                mmu_virt_to_phys((void *)vaddr);
                        if (current_paddr != expected_paddr) {
                                s_failed = true;
                                LOGE(LOG_TAG,
                                     "[%u] Unexpected mapping @ %p: Expected "
                                     "%p, got %p",
                                     task_id,
                                     vaddr,
                                     expected_paddr,
                                     current_paddr);
                        }
                }
        }
        for (unsigned alloc = 0; alloc < ALLOC_COUNT; ++alloc) {
                process_free_pages(
                        process_running(),
                        allocs[alloc],
                        alloc_page_count[alloc]
                );
        }
}

static void run_test() {
        interrupts_enable();

        unsigned task_id = ++s_task_counter;
        LOGI(LOG_TAG, "[%u] Test started", task_id);
        unsigned pass_count = 0;
        while (1) {
                while (s_failed) {}
                run_single_pass(task_id);
                ++pass_count;
                if (s_failed) {
                        panic("Test failed");
                }
                LOGI(LOG_TAG, "[%u] Test OK (Pass %u)", task_id, pass_count);
        }
}

static void cmd_main(char *arg_str) {
        unsigned remaining_thread_count;
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
                .name = "<threads>",
                .help = "Specifies thread count. 1 runs test without spawning "
                        "threads.",
        },
        {0, 0},
};

const struct CliCmd_Descriptor CLICMD_TESTPAGEALLOC = {
        .name = "testpagealloc",
        .fn = cmd_main,
        .description = "Runs page allocator tests",
        .args_help = ARG_HELP,
};
