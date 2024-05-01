add_library(c STATIC
        assert.c errno.c
        unistd/write.c unistd/read.c
        sched/sched_yield.c
        stdio/stdio.c stdio/fputs.c stdio/fputc.c stdio/fwrite.c stdio/fflush.c stdio/fprintf.c
        string/memcpy.c string/strlen.c
        yjk/syscall.c yjk/dprint.c
)

add_custom_command(
        TARGET c
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:c> ${YJK_SYSROOT}/usr/lib/libc.a
)

add_library(crt0 STATIC crt/crt0.c)
add_custom_command(
        TARGET crt0
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_OBJECTS:crt0> ${YJK_SYSROOT}/usr/lib/crt0.o
)
