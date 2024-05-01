// SPDX-FileCopyrightText: (c) 2023-2024 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause

#include "_internal.h"
#include <errno.h>
#include "kernel/arch/arch.h"
#include "kernel/builddate.h"
#include "kernel/cli/cli.h"
#include "kernel/heap/heap.h"
#include "kernel/kernel.h"
#include "kernel/memory/memory.h"
#include "kernel/tasks/tasks.h"
#include "kernel/utility/utility.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdnoreturn.h>
#include <support/thirdparty/limine/limine.h>

USED LIMINE_BASE_REVISION(1)

USED struct limine_framebuffer_request framebuffer_request = {
        .id = LIMINE_FRAMEBUFFER_REQUEST,
        .revision = 0,
};

USED struct limine_memmap_request memmap_request = {
        .id = LIMINE_MEMMAP_REQUEST,
        .revision = 0,
};

USED struct limine_rsdp_request rsdp_request = {
        .id = LIMINE_RSDP_REQUEST,
        .revision = 0,
};

USED struct limine_hhdm_request hhdm_request = {
        .id = LIMINE_HHDM_REQUEST,
        .revision = 0,
};

USED static struct limine_internal_module internal_module_font = {
        .path = "/yjk/font.bin",
        .cmdline = "",
        .flags = LIMINE_INTERNAL_MODULE_REQUIRED,
};
USED static struct limine_internal_module internal_module_hellosvc = {
        .path = "/yjk/hellosvc",
        .cmdline = "",
        .flags = LIMINE_INTERNAL_MODULE_REQUIRED,
};
USED static struct limine_internal_module *internal_modules[] = {
        [0] = &internal_module_font,
        [1] = &internal_module_hellosvc,
};
USED struct limine_module_request module_request = {
        .id = LIMINE_MODULE_REQUEST,
        .revision = 1,

        .internal_module_count = 2,
        .internal_modules = internal_modules,
};

static struct limine_file *search_for_module(char const *wanted_path) {
        if (!module_request.response) {
                panic("No module response was given by bootloader");
        }
        for (unsigned i = 0; i < module_request.response->module_count; ++i) {
                struct limine_file *file = module_request.response->modules[i];
                if (str_equals(file->path, wanted_path)) {
                        return file;
                }
        }
        return NULL;
}

static int64_t exec_module(char const *wanted_path) {
        struct limine_file *file = search_for_module(wanted_path);
        if (!file) {
                return -ENOENT;
        }
        return exec(wanted_path, file->address, file->size);
}

static size_t s_total_mem_size_in_mb;

static void register_physpages(void) {
        size_t registered_page_count = 0;
        if (!memmap_request.response) {
                panic("Bootloader didn't provide response to memmap request");
        }
        for (size_t i = 0; i < memmap_request.response->entry_count; ++i) {
                struct limine_memmap_entry const *entry =
                        memmap_request.response->entries[i];
                if (entry->type != LIMINE_MEMMAP_USABLE) {
                        continue;
                }
                struct PhysPage_Descriptor descriptor = {
                        .base = entry->base,
                        .page_count = entry->length / PAGE_SIZE,
                };
                if (descriptor.base == 0) {
                        // Don't allow base == 0 to avoid allocating to NULL
                        // address.
                        descriptor.base += PAGE_SIZE;
                        --descriptor.page_count;
                }
                if (descriptor.page_count == 0) {
                        continue;
                }
                uintptr_t region_end_addr =
                        descriptor.base + descriptor.page_count * PAGE_SIZE;
                uintptr_t apic_code_end_addr =
                        SMPBOOT_AP_BOOT_CODE_PHYS_BASE +
                        smpboot_ap_boot_code_page_count() * PAGE_SIZE;

                if ((descriptor.base <= SMPBOOT_AP_BOOT_CODE_PHYS_BASE) ||
                    (region_end_addr <= apic_code_end_addr)) {
                        if (descriptor.base < SMPBOOT_AP_BOOT_CODE_PHYS_BASE) {
                                struct PhysPage_Descriptor descriptor_before = {
                                        .base = descriptor.base,
                                        .page_count =
                                                (SMPBOOT_AP_BOOT_CODE_PHYS_BASE -
                                                 descriptor.base) /
                                                PAGE_SIZE,
                                };
                                if (descriptor_before.page_count != 0) {
                                        physpage_register(&descriptor_before);
                                        registered_page_count +=
                                                descriptor_before.page_count;
                                }
                        }
                        if (apic_code_end_addr < region_end_addr) {
                                uintptr_t base =
                                        align_up(PAGE_SIZE, apic_code_end_addr);
                                struct PhysPage_Descriptor descriptor_after = {
                                        .base = base,
                                        .page_count = (region_end_addr - base) /
                                                      PAGE_SIZE,
                                };
                                if (descriptor_after.page_count != 0) {
                                        physpage_register(&descriptor_after);
                                        registered_page_count +=
                                                descriptor_after.page_count;
                                }
                        }
                } else {
                        physpage_register(&descriptor);
                        registered_page_count += descriptor.page_count;
                }
        }
        s_total_mem_size_in_mb =
                (registered_page_count * PAGE_SIZE) / (1024UL * 1024UL);
}

static uint32_t make_bitmask(int size, int lshift) {
        return (((uint32_t)(~0)) >> (32 - size)) << lshift;
}

static void init_videoconsole(bool vmmalloc_available) {
        struct limine_file *file = search_for_module("/yjk/font.bin");
        if (!file) {
                panic("Couldn't locate /yjk/font.bin");
        }
        if (!framebuffer_request.response) {
                panic("Bootloader didn't provide response to framebuffer request");
        }
        if (!framebuffer_request.response->framebuffer_count) {
                panic("Bootloader didn't provide any framebuffers");
        }
        struct limine_framebuffer *fbinfo = framebuffer_request.response->framebuffers[0];
        ASSERT(fbinfo->bpp == 32);
        videoconsole_init(
                fbinfo->address,
                (int)fbinfo->width,
                (int)fbinfo->height,
                (int)fbinfo->pitch / (fbinfo->bpp / 8),
                make_bitmask(fbinfo->red_mask_size, fbinfo->red_mask_shift),
                make_bitmask(fbinfo->green_mask_size, fbinfo->green_mask_shift),
                make_bitmask(fbinfo->blue_mask_size, fbinfo->blue_mask_shift),
                file->address,
                vmmalloc_available
        );
}

static void init_acpi(void) {
        if (!rsdp_request.response) {
                panic("Bootloader didn't provide ACPI RSDP");
        }
        acpi_load_root_sdt(rsdp_request.response->address);
}

char *kstrchr(char const *s, int c) {
        while (1) {
                if (*s == c) {
                        return (char *)s;
                }
                if (!*s) {
                        return NULL;
                }
                s++;
        }
}


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
                console_printf("[objpath] %s\n", *opath);
                opath++;
        }
}

char **objpath_new(char const *path) {
        char **out = NULL;
        size_t path_count = 0;
        while (path[0]) {
                char *slash_pos = kstrchr(path, '/');
                size_t len;
                if (slash_pos) {
                        len = slash_pos - path;
                } else {
                        len = kstrlen(path);
                }
                if (!len) {
                        continue;
                }
                char *segment = vmmalloc(len + 1);
                if (!segment) {
                        goto oom;
                }
                kmemcpy(segment, path, len);
                segment[len] = '\0';
                char **new_out = vmrealloc(out, sizeof(void *) * (path_count + 1));
                if (!new_out) {
                        vmfree(segment);
                        goto oom;
                }
                new_out[path_count] = segment;
                path += len;
                out = new_out;
                path_count++;
        }
        char **new_out = vmrealloc(out, sizeof(void *) * (path_count + 1));
        if (!new_out) {
                goto oom;
        }
        new_out[path_count] = NULL;
        return out;
oom:
        for (size_t i = 0; i < path_count; i++) {
                vmfree(out[i]);
        }
        vmfree(out);
        return NULL;
}


static void boot_stage2_bsp() {
        static char const *LOG_TAG = "boot-stage2(bsp)";
        // init_videoconsole(true);
        init_acpi();

        struct MADT *madt = acpi_locate_table("APIC");
        if (!madt) {
                panic("MADT not found");
        }
        madt_init(madt);
        idt_use_ist1();
        mmu_nuke_non_kernel_pages();
        i8259pic_init();
        lapic_init_for_bsp();
        ioapic_init();
        console_printf(
                ""
                "------------------------------------------------------------\n"
                "                    Welcome back, Sensei\n"
                "         Kernel image timestamp: " BUILDDATE "\n"
                "      Number of processors: %u    Size of memory: %uMiB\n"
                "------------------------------------------------------------\n",
                lapic_count(),
                s_total_mem_size_in_mb
        );
        syscall_init_tables();
        syscall_init_msrs();
        lapic_enable();
        // APIC timer must be calibrated before we start other processors
        i8254timer_stop();
        lapic_timer_reset_to_1ms();
        smpboot_start();
        interrupts_enable();

        while(0) {
                char **p = objpath_new("hello/world/sensei");
                objpath_print(p);
                objpath_free(p);
                console_printf("-----------------\n");
        }

        LOGI(LOG_TAG, "Kernel boot complete. Starting userspace software...");
        int64_t err = exec_module("/yjk/hellosvc");
        if (err < 0) {
                LOGE(LOG_TAG, "Failed to launch executable (%u)", -err);
        }

        LOGI(LOG_TAG, "The system is ready for use");
        thread_spawn(process_running(), "kernel cli", cli_run);
        scheduler_run_idle_loop();
}

static void doomed() {
        __asm__ volatile("cli");
        while (true) {
                __asm__ volatile("hlt");
        }
}

noreturn void kernel_entry(void) {
        if (!LIMINE_BASE_REVISION_SUPPORTED) {
                doomed();
        }
        uartconsole_init();
        console_printf("Kernel is starting up\n");
#ifdef YJK_ULTRA_PARANOID_MODE
        console_printf("YJK_ULTRA_PARANOID_MODE is ON\n");
#endif
        init_videoconsole(false);
        kmalloc_init();
        processor_init_for_bsp();
        idt_init_bsp();
        register_physpages();
        if (!hhdm_request.response) {
                panic("Requested HHDM to bootloader, but got no response");
        }
        void *direct_mapped_base = (void *)hhdm_request.response->offset;
        mmu_addrspace_t kernel_vm_addrspace_handle = mmu_init_for_bsp(direct_mapped_base);
        process_spawn_kernel(kernel_vm_addrspace_handle);
        scheduler_init_for_bsp(boot_stage2_bsp);
}

static void boot_stage2_ap() {
        idt_use_ist1();
        lapic_init_for_ap();
        lapic_enable();
        lapic_timer_reset_to_1ms();
        syscall_init_msrs();
        interrupts_enable();
        smpboot_ap_did_boot();
        scheduler_run_idle_loop();
}

noreturn void kernel_entry_ap(unsigned ap_index) {
        processor_init_for_ap(ap_index);
        idt_init_ap();
        mmu_init_for_ap(ap_index);
        scheduler_init_for_ap(boot_stage2_ap);
}
