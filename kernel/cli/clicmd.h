// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#pragma once

struct CliCmd_ArgHelp {
        char const *name;
        char const *help;
};

struct CliCmd_Descriptor {
        void (*fn)(char *arg_str);
        char const *name;
        char const *description;
        // Help for arguments. Terminate the list with all zeros.
        struct CliCmd_ArgHelp const *args_help;
};

extern const struct CliCmd_Descriptor CLICMD_TESTMALLOC;
extern const struct CliCmd_Descriptor CLICMD_TESTPAGEALLOC;
