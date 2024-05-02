// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "cli.h"
#include "cliarg.h"
#include "clicmd.h"
#include "kernel/arch/arch.h"
#include "kernel/kernel.h"
#include "kernel/utility/utility.h"
#include <stdbool.h>
// XXX: This is temporary workaround until we fully move to C++
#ifdef NORETURN_WORKAROUND
#define NORETURN [[noreturn]]
#else
#include <stdnoreturn.h>
#define NORETURN noreturn
#endif

#define CMD_NAME_MAX_LEN 15

static struct CliCmd_Descriptor find_cmd(char const *name);

////////////////////////////////////////////////////////////////////////////////
// Command list and Help command
////////////////////////////////////////////////////////////////////////////////

static void cmd_help(char *arg_str);

static const struct CliCmd_ArgHelp HELP_ARG_HELPS[] = {
        {
                .name = "(<command name>)",
                .help = "(Optional) If specified, shows help about specific "
                        "command. "
                        "Otherwise, command list is displayed.",
        },
        {0, 0},
};

static const struct CliCmd_Descriptor HELP_CMD = {
        .name = "help",
        .fn = cmd_help,
        .description = "Shows command list or help about specific command",
        .args_help = HELP_ARG_HELPS,
};

static struct CliCmd_Descriptor const *CMDS[] = {
        &HELP_CMD,
        &CLICMD_TESTMALLOC,
        &CLICMD_TESTPAGEALLOC,
};

enum {
        CMDS_COUNT = sizeof(CMDS) / sizeof(void *),
};

static size_t max_arg_name_len(struct CliCmd_Descriptor cmd) {
        size_t max_len = 0;
        for (unsigned i = 0; cmd.args_help[i].name; ++i) {
                size_t len = kstrlen(cmd.args_help[i].name);
                if (max_len < len) {
                        max_len = len;
                }
        }
        return max_len;
}

static void cmd_help_show_cmd_help(char const *name) {
        struct CliCmd_Descriptor desc = find_cmd(name);
        if (!desc.name) {
                console_alert("help: %s is not a command", name);
                return;
        }
        console_printf(" %s -- %s\n", desc.name, desc.description);
        console_printf("USAGE : %s", desc.name);
        for (unsigned i = 0; desc.args_help[i].name; ++i) {
                console_printf(" %s", desc.args_help[i].name);
        }
        console_put_string("\n");
        console_printf("OPTIONS:\n");
        size_t field_width = max_arg_name_len(desc);
        for (unsigned i = 0; desc.args_help[i].name; ++i) {
                console_put_char(' ');
                console_put_string_with_pad(
                        desc.args_help[i].name, field_width + 1
                );
                console_printf(" %s\n", desc.args_help[i].help);
        }
}

static size_t max_cmd_name_len(void) {
        size_t max_len = 0;
        for (unsigned i = 0; i < CMDS_COUNT; ++i) {
                size_t len = kstrlen(CMDS[i]->name);
                if (max_len < len) {
                        max_len = len;
                }
        }
        return max_len;
}

static void cmd_help_show_cmd_list(void) {
        console_printf("COMMAND LIST:\n");
        size_t field_width = max_cmd_name_len();
        for (unsigned i = 0; i < CMDS_COUNT; ++i) {
                console_put_char(' ');
                console_put_string_with_pad(CMDS[i]->name, field_width + 1);
                console_printf(" %s\n", CMDS[i]->description);
        }
}

static void cmd_help(char *arg_str) {
        char name_buf[CMD_NAME_MAX_LEN + 1];
        if (0 < cliarg_next_str(name_buf, sizeof(name_buf), &arg_str)) {
                cmd_help_show_cmd_help(name_buf);
        } else {
                cmd_help_show_cmd_list();
        }
}

////////////////////////////////////////////////////////////////////////////////
// Command line loop
////////////////////////////////////////////////////////////////////////////////

// Returns entry filled with 0 if there's no such command.
static struct CliCmd_Descriptor find_cmd(char const *name) {
        for (unsigned i = 0; i < CMDS_COUNT; ++i) {
                if (str_equals(CMDS[i]->name, name)) {
                        return *CMDS[i];
                }
        }
        return (struct CliCmd_Descriptor){0, 0, 0, 0};
}

static bool is_whitespace(char chr) {
        switch (chr) {
        case ' ':
        case '\t':
                return true;
        default:
                return false;
        }
}

static void skip_spaces(char **str) {
        for (; **str && is_whitespace(**str); ++(*str)) {}
}

// #define RUN_KMALLOC_TEST_IMMEDIATELY

NORETURN void cli_run(void) {
        interrupts_enable();
        while (1) {
#ifndef RUN_KMALLOC_TEST_IMMEDIATELY
                char cmdline[80];
                console_put_string("> ");
                console_get_line(
                        cmdline, sizeof(cmdline) / sizeof(*cmdline), false
                );
                console_put_string("\n");
#else
                char cmdline[80] = "testmalloc kmalloc 10";
#endif

                char name[CMD_NAME_MAX_LEN + 1];
                char *args = cmdline;
                if (cliarg_next_str(name, sizeof(name), &args) <= 0) {
                        continue;
                }
                skip_spaces(&args);
                struct CliCmd_Descriptor desc = find_cmd(name);
                if (!desc.name) {
                        console_alert("%s is not a command", name);
                        continue;
                }
                desc.fn(args);
        }
}
