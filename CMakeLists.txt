cmake_minimum_required(VERSION 3.8)
set(CMAKE_C_COMPILER_WORKS 1)
project(isos C ASM)

set(ISOS_BOOTROOT ${CMAKE_INSTALL_PREFIX}/bootroot)
set(ISOS_SYSROOT ${CMAKE_INSTALL_PREFIX}/sysroot)
set(ISOS_SUPPORT_DIR ${CMAKE_SOURCE_DIR}/support)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_CLANG_TIDY clang-tidy --extra-arg-before=--sysroot=${ISOS_SYSROOT} --extra-arg-before=-isystem=${ISOS_SYSROOT}/usr/include)

if (ISOS_ARCH STREQUAL "x86")
        # FIXME: Remove these flags from userland when we are ready
        set(ISOS_ARCH_CFLAGS "-mno-red-zone -mgeneral-regs-only -mno-mmx -mno-sse -mno-sse2 -masm=intel")
else ()
        message(FATAL_ERROR "${ISOS_ARCH} isn't a valid value")
endif()

set(
        CMAKE_C_FLAGS
        ${CMAKE_C_FLAGS} "-Wall -Werror -Wextra -fno-stack-protector ${ISOS_ARCH_CFLAGS}"
)

include_directories(BEFORE ${CMAKE_SOURCE_DIR})

add_subdirectory(kernel)
add_subdirectory(userland)

set(ISOS_LIMINE_DIR ${ISOS_SUPPORT_DIR}/thirdparty/limine)
install(FILES
        ${ISOS_SUPPORT_DIR}/res/limine.cfg
        ${ISOS_LIMINE_DIR}/limine-bios.sys
        ${ISOS_LIMINE_DIR}/limine-bios-cd.bin
        ${ISOS_LIMINE_DIR}/limine-uefi-cd.bin
        DESTINATION ${ISOS_BOOTROOT})
install(FILES
        ${ISOS_LIMINE_DIR}/BOOTX64.EFI
        ${ISOS_LIMINE_DIR}/BOOTIA32.EFI
        DESTINATION ${ISOS_BOOTROOT}/EFI/BOOT/)
