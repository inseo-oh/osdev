// SPDX-FileCopyrightText: 2023, Inseo Oh (dhdlstjtr@gmail.com)
//
// SPDX-License-Identifier: BSD-2-Clause
#include <sched.h>
#include <stdio.h>
#include <unistd.h>

int main() {
        (void)fputs("Welcome to ISOS\n", stderr);
        while (1) {
                (void)fputs("# ", stderr);
                char buf[160];
                read(STDIN_FILENO, buf, sizeof(buf));
                (void)fputs("I got: ", stderr);
                (void)fputs(buf, stderr);
                sched_yield();
        }
}
