// SPDX-FileCopyrightText: (c) 2024 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause
#include "objpath.h"
#include "kernel/heap/heap.h"
#include "kernel/kernel.h"

void objpath_free(char **opath) {
        while (1) {
                char *ptr = *opath;
                if (!ptr) {
                        break;
                }
                vmfree(*opath);
                opath++;
        }
}

void objpath_print(char **opath) {
        while (1) {
                char *ptr = *opath;
                if (!ptr) {
                        break;
                }
                console_printf("[objpath] %p %s\n", opath, *opath);
                opath++;
        }
}

char **objpath_new(char const *path) {
        char **out = NULL;
        size_t path_count = 0;
        size_t path_cap = 0;
        while (path[0]) {
                char const *slash_pos = kstrchr(path, '/');
                if (!slash_pos) {
                        slash_pos = &path[kstrlen(path)];
                }
                size_t len = slash_pos - path;
                if (!len) {
                        path++;
                        continue;
                }
                if (len == 1 && path[0] == '.') {
                        // .
                } else if (len == 2 && path[0] == '.' && path[1] == '.') {
                        // ..
                        if (path_count) {
                                path_count--;
                        }
                } else {
                        char *segment = vmmalloc(len + 1);
                        if (!segment) {
                                goto oom;
                        }
                        kmemcpy(segment, path, len);
                        segment[len] = '\0';
                        if (path_cap < (path_count + 1)) {
                                char **new_out = vmrealloc(out, sizeof(void *) * (path_count + 1));
                                if (!new_out) {
                                        vmfree(segment);
                                        goto oom;
                                }
                                out = new_out;
                                path_cap = path_count + 1;
                        }
                        out[path_count] = segment;
                        path_count++;
                }
                if (*slash_pos == '\0') {
                        break;
                }
                path = slash_pos + 1;
        }

        if (path_cap < (path_count + 1)) {
                char **new_out = vmrealloc(out, sizeof(void *) * (path_count + 1));
                if (!new_out) {
                        goto oom;
                }
                out = new_out;
        }
        out[path_count] = NULL;
        return out;
oom:
        for (size_t i = 0; i < path_count; i++) {
                vmfree(out[i]);
        }
        vmfree(out);
        return NULL;
}

