if (YJK_ARCH STREQUAL "x86")
        set(YJK_KERNELLDS ${CMAKE_CURRENT_SOURCE_DIR}/arch/x86/link.ld)
        set(YJK_ARCH_CFLAGS -mno-red-zone -mgeneral-regs-only -mno-mmx -mno-sse -mno-sse2 -masm=intel)
        set(YJK_ARCH_SOURCES
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
        message(FATAL_ERROR "${YJK_ARCH} isn't a valid value")
endif()

add_executable(kernel
        # Root sources
        console.c panic.c ticktime.c syscall.c acpi.c videoconsole.c objpath.c
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

        ${YJK_ARCH_SOURCES}
        )

if (YJK_PARANOID STREQUAL "ON")
        set(UBSAN_FLAGS -fsanitize=undefined)
        target_compile_definitions(kernel PRIVATE YJK_ULTRA_PARANOID_MODE)
else()
        set(OPTIMIZE_FLAGS -flto -O3)
endif()

target_compile_options(kernel PRIVATE
        -ffreestanding -fno-stack-protector -mcmodel=large
        ${UBSAN_FLAGS} ${OPTIMIZE_FLAGS}
        ${YJK_ARCH_CFLAGS}
)

target_include_directories(kernel PRIVATE
        ../
)
target_link_options(kernel PRIVATE
        -nostdlib -T ${YJK_KERNELLDS} ${OPTIMIZE_FLAGS}
)

install(TARGETS kernel DESTINATION ${YJK_BOOTROOT}/yjk)
