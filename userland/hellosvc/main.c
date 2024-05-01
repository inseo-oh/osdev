// SPDX-FileCopyrightText: 2023, Inseo Oh (dhdlstjtr@gmail.com)
//
// SPDX-License-Identifier: BSD-2-Clause
#include <sched.h>
#include <stdio.h>
#include <unistd.h>
#include <yjk/dprint.h>

int main() {
        (void)fprintf(dprnout, "Start executive\n");
        
        while (1) {
                continue;
                (void)fputs("# ", stderr);
                char buf[160];
                read(STDIN_FILENO, buf, sizeof(buf));
                (void)fputs("I got: ", stderr);
                (void)fputs(buf, stderr);
                sched_yield();
        }
}

// testmalloc vmmalloc 10