# SPDX-FileCopyrightText: (c) 2024 Inseo Oh <dhdlstjtr@gmail.oom>
#
# SPDX-License-Identifier: BSD-2-Clause

ifndef YJK_ARCH
$(error YJK_ARCH is not set)
endif

YJK_PROJECT_ROOT = $(shell pwd)

include $(YJK_PROJECT_ROOT)/support/env.mk

YJK_LIMINE_DIR = $(YJK_SUPPORT_DIR)/thirdparty/limine


all:
	support/tools/copy_libc_headers.sh $(YJK_ARCH) $(YJK_PROJECT_ROOT)
	$(MAKE) -C kernel $(YJK_MAKE_FLAGS)
	$(MAKE) -C userland $(YJK_MAKE_FLAGS)
	support/tools/fontgen.py support/res/unifont-15.1.04.hex $(YJK_BOOT_ROOT)/yjk/font.bin
	mkdir -p $(YJK_BOOT_ROOT)/EFI/BOOT/
	cp  $(YJK_SUPPORT_DIR)/res/limine.cfg \
		$(YJK_LIMINE_DIR)/limine-bios.sys \
		$(YJK_LIMINE_DIR)/limine-bios-cd.bin \
		$(YJK_LIMINE_DIR)/limine-uefi-cd.bin \
		$(YJK_BOOT_ROOT)
	cp  $(YJK_LIMINE_DIR)/BOOTX64.EFI \
		$(YJK_LIMINE_DIR)/BOOTIA32.EFI \
		$(YJK_BOOT_ROOT)/EFI/BOOT/
	xorriso -as mkisofs -b limine-bios-cd.bin \
			-no-emul-boot -boot-load-size 4 -boot-info-table \
			--efi-boot limine-uefi-cd.bin \
			-efi-boot-part --efi-boot-image \
			--protective-msdos-label \
			$(YJK_BOOT_ROOT) -o $(YJK_INSTALL_ROOT)/boot.iso
	$(YJK_LIMINE_DIR)/limine bios-install $(YJK_INSTALL_ROOT)/boot.iso

clean:
	$(MAKE) -C kernel clean $(YJK_MAKE_FLAGS)
	$(MAKE) -C userland clean $(YJK_MAKE_FLAGS)

run: all
	support/tools/run.py

.PHONY: all clean run