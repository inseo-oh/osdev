// SPDX-FileCopyrightText: 2023, Inseo Oh (dhdlstjtr@gmail.com)
//
// SPDX-License-Identifier: BSD-2-Clause
#include <errno.h>
#include "kernel/arch/arch.h"
#include "kernel/kernel.h"
#include "kernel/tasks/tasks.h"
#include "kernel/utility/utility.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef uint64_t Elf64_Xword, Elf64_Addr, Elf64_Off;

struct ElfFile {
        void const *data;
        size_t size;
};

typedef struct {
        unsigned char e_ident[16];
        Elf64_Half e_type;
        Elf64_Half e_machine;
        Elf64_Word e_version;
        Elf64_Addr e_entry;
        Elf64_Off e_phoff;
        Elf64_Off e_shoff;
        Elf64_Word e_flags;
        Elf64_Half e_ehsize;
        Elf64_Half e_phentsize;
        Elf64_Half e_phnum;
        Elf64_Half e_shentsize;
        Elf64_Half e_shnum;
        Elf64_Half e_shstrndx;
} Elf64_Ehdr;

// Indices for e_ident
#define EI_MAG0       0
#define EI_MAG1       1
#define EI_MAG2       2
#define EI_MAG3       3
#define EI_CLASS      4
#define EI_DATA       5
#define EI_VERSION    6
#define EI_OSABI      7
#define EI_ABIVERSION 8
#define EI_PAD        9
#define EI_NIDENT     16

// Values for e_ident[EI_CLASS]
#define ELFCLASS64 2

// Values for e_ident[EI_DATA]
#define ELFDATA2LSB 1

// Values for e_ident[EI_OSABI]
#define ELFOSABI_SYSV 0

// Values for e_ident[EI_VERSION] and e_version
#define EV_CURRENT 1

// Values for e_type
#define ET_EXEC 2

// Values for e_machine
#define EM_X86_64 62

// Convenience macro for comparing what e_machine is supposed to be
#ifdef PLATFORM_IS_X86
#define EM_NATIVE EM_X86_64
#else
#error "Unknown CPU architecture"
#endif

typedef struct {
        Elf64_Word p_type;
        Elf64_Word p_flags;
        Elf64_Off p_offset;
        Elf64_Addr p_vaddr;
        Elf64_Addr p_paddr;
        Elf64_Xword p_filesz;
        Elf64_Xword p_memsz;
        Elf64_Xword p_align;
} Elf64_Phdr;

// Values for p_type
#define PT_LOAD 1

// Values for p_flags
#define PF_X (1 << 0)
#define PF_W (1 << 1)
#define PF_R (1 << 2)

static WARN_UNUSED_RESULT bool
read_file(struct ElfFile *file, void *buf, uint64_t offset, size_t len) {
        if (file->size < offset + len) {
                return false;
        }
        kmemcpy(buf, &((uint8_t *)file->data)[offset], len);
        return true;
}

static WARN_UNUSED_RESULT bool
read_ehdr(struct ElfFile *file, Elf64_Ehdr *buf) {
        return read_file(file, buf, 0, sizeof(*buf));
}

static WARN_UNUSED_RESULT bool
read_phdr(struct ElfFile *file, Elf64_Phdr *buf, size_t offset) {
        return read_file(file, buf, offset, sizeof(*buf));
}

static bool check_e_ident(unsigned char const *e_ident) {
        return (e_ident[EI_MAG0] == '\x7f') && (e_ident[EI_MAG1] == 'E') &&
               (e_ident[EI_MAG2] == 'L') && (e_ident[EI_MAG3] == 'F') &&
               (e_ident[EI_CLASS] == ELFCLASS64) &&
               (e_ident[EI_DATA] == ELFDATA2LSB) &&
               (e_ident[EI_VERSION] == EV_CURRENT) &&
               (e_ident[EI_OSABI] == ELFOSABI_SYSV) &&
               (e_ident[EI_ABIVERSION] == 0);
}

static bool check_ehdr(Elf64_Ehdr const *ehdr) {
        if (!check_e_ident(ehdr->e_ident)) {
                return false;
        }
        return (ehdr->e_machine == EM_NATIVE) &&
               (ehdr->e_version == EV_CURRENT);
}

static int64_t load_phdr(
        struct ElfFile *file, struct Process *process, Elf64_Phdr const *phdr
) {
        if ((phdr->p_type != PT_LOAD) || !(phdr->p_flags & PF_R)) {
                return 0;
        }
        if (phdr->p_memsz < phdr->p_filesz) {
                return -ENOEXEC;
        }
        struct Proc_MapOptions options = {
                .writable = phdr->p_flags & PF_W,
                .executable = phdr->p_flags & PF_X,
        };

        uintptr_t paddr;
        uintptr_t offset = phdr->p_vaddr % PAGE_SIZE;
        uintptr_t vaddr = phdr->p_vaddr - offset;
        size_t memsz = phdr->p_memsz + offset;
        unsigned page_count = to_block_count(PAGE_SIZE, memsz);
        void *dest = process_alloc_pages(
                process_kernel(),
                &paddr,
                page_count,
                (struct Proc_MapOptions){.writable = true, .executable = false}
        );
        if (!dest) {
                return -ENOMEM;
        }
        kmemset(dest, 0, page_count * PAGE_SIZE);
        bool file_read_ok = read_file(
                file, &((uint8_t *)dest)[offset], phdr->p_offset, phdr->p_filesz
        );
        if (!file_read_ok) {
                process_free_pages(process_kernel(), dest, page_count);
                return false;
        }
        process_unmap_pages(process_kernel(), dest, page_count);
        if (!process_map_pages_at(
                    process, paddr, (void *)vaddr, page_count, options
            )) {
                return -ENOMEM;
        }
        process_activate_user_addrspace(process);
        return 0;
}

static int64_t load_phdrs(
        struct ElfFile *file,
        struct Process *process,
        size_t offset,
        size_t entry_count,
        size_t entry_size
) {
        int64_t result;
        if (entry_size < sizeof(Elf64_Phdr)) {
                result = -ENOEXEC;
                goto fail;
        }
        for (size_t i = 0; i < entry_count; ++i, offset += entry_size) {
                Elf64_Phdr phdr;
                if (!read_phdr(file, &phdr, offset)) {
                        result = -ENOEXEC;
                        goto fail;
                }
                result = load_phdr(file, process, &phdr);
                if (result < 0) {
                        goto fail;
                }
        }
        return 0;
fail:
        return result;
}

// Returns 0 on success, negative errno on failure.
int64_t exec(char const *name, void const *data, size_t size) {
        int64_t result;
        struct ElfFile file = {.data = data, .size = size};
        Elf64_Ehdr ehdr;
        struct Process *process = NULL;
        bool loaded_phdrs = false;

        if (!read_ehdr(&file, &ehdr)) {
                result = -ENOEXEC;
                goto fail;
        }
        if (!check_ehdr(&ehdr)) {
                result = -ENOEXEC;
                goto fail;
        }
        if (ehdr.e_type != ET_EXEC) {
                TODO();
        }
        process = process_spawn_user(name);
        if (!process) {
                result = -ENOMEM;
                goto fail;
        }
        result = load_phdrs(
                &file, process, ehdr.e_phoff, ehdr.e_phnum, ehdr.e_phentsize
        );
        if (result < 0) {
                goto fail;
        }
        loaded_phdrs = true;
        void (*entry_point)() = (void (*)())ehdr.e_entry;
        console_printf("spawn main\n");
        if (!thread_spawn(process, "main", entry_point)) {
                result = -ENOMEM;
                goto fail;
        }
        return 0;
fail:
        if (loaded_phdrs) {
                // Unload phdrs
                TODO_HANDLE_ERROR();
        }
        if (process) {
                // Delete process
                TODO_HANDLE_ERROR();
        }
        return result;
}
