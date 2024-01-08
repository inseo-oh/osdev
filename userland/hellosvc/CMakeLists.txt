add_executable(hellosvc
        main.c strfmt.c
        )
add_dependencies(hellosvc c)

install(TARGETS hellosvc DESTINATION ${ISOS_BOOTROOT}/isos)