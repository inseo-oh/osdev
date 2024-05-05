cmake_minimum_required(VERSION 3.8)
set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_CXX_COMPILER_WORKS 1)
project(yjk C CXX ASM)

set(YJK_BOOT_ROOT ${CMAKE_INSTALL_PREFIX}/bootroot)
set(YJK_SYS_ROOT ${CMAKE_INSTALL_PREFIX}/sysroot)
set(YJK_SUPPORT_DIR ${CMAKE_SOURCE_DIR}/support)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_CLANG_TIDY clang-tidy --extra-arg-before=--sysroot=${YJK_SYS_ROOT} --extra-arg-before=-isystem=${YJK_SYS_ROOT}/usr/include)

if (YJK_ARCH STREQUAL "x86")
        # FIXME: Remove these flags from userland when we are ready
        set(YJK_ARCH_CFLAGS "-mno-red-zone -mgeneral-regs-only -mno-mmx -mno-sse -mno-sse2 -masm=intel")
else ()
        message(FATAL_ERROR "${YJK_ARCH} isn't a valid value")
endif()
set(
        CMAKE_C_FLAGS
        ${CMAKE_C_FLAGS} "-Wall -Werror -Wextra -fno-stack-protector ${YJK_ARCH_CFLAGS}"
)
set(
        CMAKE_CXX_FLAGS
        ${CMAKE_CXX_FLAGS} "-Wall -Werror -Wextra -fno-stack-protector -fno-exceptions -fno-rtti -std=c++17 ${YJK_ARCH_CFLAGS}"
)

include_directories(BEFORE ${CMAKE_SOURCE_DIR})

add_subdirectory(kernel)
add_subdirectory(userland)

set(YJK_LIMINE_DIR ${YJK_SUPPORT_DIR}/thirdparty/limine)
install(FILES
        ${YJK_SUPPORT_DIR}/res/limine.cfg
        ${YJK_LIMINE_DIR}/limine-bios.sys
        ${YJK_LIMINE_DIR}/limine-bios-cd.bin
        ${YJK_LIMINE_DIR}/limine-uefi-cd.bin
        DESTINATION ${YJK_BOOT_ROOT})
install(FILES
        ${YJK_LIMINE_DIR}/BOOTX64.EFI
        ${YJK_LIMINE_DIR}/BOOTIA32.EFI
        DESTINATION ${YJK_BOOT_ROOT}/EFI/BOOT/)
