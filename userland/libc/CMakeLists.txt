add_library(c STATIC
        errno.c
        unistd/write.c unistd/read.c
        sched/sched_yield.c
        stdio/stdio.c stdio/fputs.c stdio/fputc.c stdio/fwrite.c
        string.c
        isos/syscall.c
)

add_custom_command(
        TARGET c
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:c> ${ISOS_SYSROOT}/usr/lib/libc.a
)

add_library(crt0 STATIC crt/crt0.c)
add_custom_command(
        TARGET crt0
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_OBJECTS:crt0> ${ISOS_SYSROOT}/usr/lib/crt0.o
)
