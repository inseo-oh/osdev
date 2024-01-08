// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause

extern int main(int argc, char **argv);

void _start() {
        char *argv[] = {"<todo: argv>"};
        main(1, argv);
        while (1) {}
}
