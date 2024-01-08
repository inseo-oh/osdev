ENTRY(kernel_entry)

VIRTUAL_BASE = 0xffffffff80000000;

SECTIONS
{
    . = VIRTUAL_BASE;

    __virtaddr_begin__ = .;
    __physaddr_begin__ = (. - VIRTUAL_BASE);
    .text : AT(ADDR(.text) - VIRTUAL_BASE) ALIGN(4096) {
        __text_begin__ = .;
        *(.text)
        *(.text.*)
        . = ALIGN(4096);
        __text_end__ = .;
    }
    .ctors : AT(ADDR(.ctors) - VIRTUAL_BASE) ALIGN(4096) {
        __initarray_begin__ = .;
        KEEP(*(.ctors))
        *(.ctors)
        KEEP(*(.init_array*))
        *(.init_array*)
        __initarray_end__ = .;
    }
    .data : AT(ADDR(.data) - VIRTUAL_BASE) ALIGN(4096) {
        *(.data)
        *(.data.*)
    }
    .rodata : AT(ADDR(.rodata) - VIRTUAL_BASE) ALIGN(4096) {
        *(.rodata)
        *(.rodata.*)
    }
    .bss : AT(ADDR(.bss) - VIRTUAL_BASE) ALIGN(4096) {
        *(.bss)
        *(.bss.*)
    }
    __virtaddr_end__ = .;
    __physaddr_end__ = (. - VIRTUAL_BASE);
}
