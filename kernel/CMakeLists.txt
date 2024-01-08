if (ISOS_ARCH STREQUAL "x86")
        set(ISOS_KERNELLDS ${CMAKE_CURRENT_SOURCE_DIR}/arch/x86/link.ld)
        set(ISOS_ARCH_CFLAGS -mno-red-zone -mgeneral-regs-only -mno-mmx -mno-sse -mno-sse2 -masm=intel)
        set(ISOS_ARCH_SOURCES
                arch/x86/entry.c
                arch/x86/i8254timer.c arch/x86/i8259pic.c arch/x86/idt.c
                arch/x86/ioapic.c arch/x86/ioport.c arch/x86/isr.c
                arch/x86/interrupts.c
                arch/x86/lapic.c
                arch/x86/madt.c 
                arch/x86/mmu.c 
                arch/x86/processor.c 
                arch/x86/smpboot.c arch/x86/stacktrace.c arch/x86/syscall.c
                arch/x86/uartconsole.c arch/x86/uaccess.c
        )
else ()
        message(FATAL_ERROR "${ISOS_ARCH} isn't a valid value")
endif()

add_executable(kernel
        # Root sources
        console.c panic.c ticktime.c syscall.c acpi.c videoconsole.c
        # Multitasking
        tasks/scheduler.c tasks/thread.c tasks/process.c tasks/exec.c
        # Memory management
        memory/virtzone.c memory/physpage.c memory/physzone.c
        # Interrupts
        interrupt/interrupts.c
        # Locking support
        lock/spinlock.c lock/mutex.c
        # Kernel Heap
        heap/kmalloc.c heap/vmmalloc.c
        # Kernel CLI
        cli/cli.c cli/cliarg.c
        cli/clicmd_testmalloc.c
        cli/clicmd_testpagealloc.c
        # Kernel internal utilities
        utility/list.c utility/ubsan.c utility/avltree.c utility/strutil.c utility/queue.c utility/bitmap.c

        ${ISOS_ARCH_SOURCES}
        )

if (ISOS_PARANOID STREQUAL "ON")
        set(UBSAN_FLAGS -fsanitize=undefined)
        target_compile_definitions(kernel PRIVATE ISOS_ULTRA_PARANOID_MODE)
endif()

target_compile_options(kernel PRIVATE
        -ffreestanding -fno-stack-protector -mcmodel=large
        -fsanitize=undefined
        # -O3
        ${ISOS_ARCH_CFLAGS}
        )


target_include_directories(kernel PRIVATE
        ../
)
target_link_options(kernel PRIVATE
        -nostdlib -T ${ISOS_KERNELLDS}
        )
install(TARGETS kernel DESTINATION ${ISOS_BOOTROOT}/isos)