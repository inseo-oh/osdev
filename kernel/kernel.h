// SPDX-FileCopyrightText: 2023, Inseo Oh (dhdlstjtr@gmail.com)
//
// SPDX-License-Identifier: BSD-2-Clause
#pragma once
#include <kernel/utility/utility.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdnoreturn.h>

////////////////////////////////////////////////////////////////////////////////
// Console
////////////////////////////////////////////////////////////////////////////////

typedef enum {
        LOGLEVEL_ERR = 'E',
        LOGLEVEL_INFO = 'I',
        LOGLEVEL_WARN = 'W',
} loglevel_t;

struct Console_Driver {
        struct List_Node node_head;

        // Below fields must be filled by the driver.
        void (*put_char_fn)(struct Console_Driver *self, char chr);
        int (*get_char_fn)(struct Console_Driver *self);
        void (*flush_fn)(struct Console_Driver *self);
};

// NOTE: Given driver must be pointer to static location(which includes
//       heap-allocated memory).
void console_register_driver(struct Console_Driver *driver);
void console_flush(void);
void console_put_string(char const *str);
void console_put_string_with_pad(char const *str, size_t width);
void console_put_char(char chr);
void console_put_unsigned_dec(uint64_t uint);
void console_put_unsigned_hex(uint64_t uint, unsigned digit_count);
int console_get_char(void);
int console_wait_char(void);
size_t console_get_line(char *buf, size_t buf_size, bool include_newline);
void console_vprintf(char const *fmt, va_list argptr);
void console_printf(char const *fmt, ...);
void console_valert(char const *fmt, va_list argptr);
void console_alert(char const *fmt, ...);
void console_log(loglevel_t level, char const *tag, char const *fmt, ...);

#define LOGI(...) console_log(LOGLEVEL_INFO, __VA_ARGS__)
#define LOGW(...) console_log(LOGLEVEL_WARN, __VA_ARGS__)
#define LOGE(...) console_log(LOGLEVEL_ERR, __VA_ARGS__)

// Quick and dirty printf debugging :D
#define HERE() LOGI("HERE", " *** HERE *** %s:%u", __FILE__, __LINE__)

////////////////////////////////////////////////////////////////////////////////
// Panic
////////////////////////////////////////////////////////////////////////////////

noreturn void panic(char const *fmt, ...);
noreturn void panic_assert_fail(char const *file, int line, char *what);

#define ASSERT(__x) \
        (void)((__x) ? ((void)0) : panic_assert_fail(__FILE__, __LINE__, #__x))
#define TODO_HANDLE_ERROR() \
        panic("%s:%u: TODO: Handle errors", __FILE__, __LINE__);
#define TODO() panic("%s:%u: TODO", __FILE__, __LINE__);
#define UNREACHABLE() \
        panic("%s:%u: Unreachable code reached", __FILE__, __LINE__);

////////////////////////////////////////////////////////////////////////////////
// Ticktime
////////////////////////////////////////////////////////////////////////////////

typedef uint64_t tick_t;

// NOTE: This temporaily locks scheduler, threads, and procs.
void ticktime_increment_tick();
tick_t ticktime_get_count();

////////////////////////////////////////////////////////////////////////////////
// Video console
////////////////////////////////////////////////////////////////////////////////

void videoconsole_init(
        void *buf,
        unsigned width,
        unsigned height,
        unsigned pixels_per_line,
        uint32_t red_bitmask,
        uint32_t green_bitmask,
        uint32_t blue_bitmask,
        uint8_t *font,
        bool vm_available
);

////////////////////////////////////////////////////////////////////////////////
// ACPI
////////////////////////////////////////////////////////////////////////////////

// TODO: Change field names to match normal ISOS naming convention.
struct ACPI_RSDP {
        uint8_t Signature[8]; // "RSD PTR "
        uint8_t Checksum;
        char OEMID[6];
        uint8_t Revision;
        uint32_t RsdtAddress; // Only if Revision == 0
        // Below are applicable only if 2 <= Revision
        uint32_t Length;
        uint64_t XsdtAddress;
        uint8_t ExtendedChecksum;
        uint8_t Reserved[3];
} PACKED;

struct ACPI_SDTHeader {
        char Signature[4];
        uint32_t Length;
        uint8_t Revision;
        uint8_t Checksum;
        char OEMID[6];
        char OEMTableID[8];
        uint32_t OEMRevision;
        uint32_t CreatorID;
        uint32_t CreatorRevision;
} PACKED;

// Returns NULL if table cannot be found.
void *acpi_locate_table(char const *signature);
void acpi_load_root_sdt(struct ACPI_RSDP *rsdp);
