# SPDX-FileCopyrightText: (c) 2024 Inseo Oh <dhdlstjtr@gmail.com>
#
# SPDX-License-Identifier: BSD-2-Clause

# YJK_MAKE_FLAGS        : Intended to be used when calling Make recursively:
# -> Usage: $(MAKE) ... $(YJK_MAKE_FLAGS)
# YJK_SUPPORT_DIR       : 'support' directory
# YJK_BUILD_DIR         : Root directory of build directory for given YJK_ARCH
# YJK_INTERMEDIATES_DIR : Where all intermediate build files go
# YJK_INSTALL_ROOT      : Install root folder, containing both boot disk root and sysroot
# YJK_BOOT_ROOT         : Install root for the boot disk
# YJK_SYS_ROOT          : Sysroot for the OS

ifndef YJK_PROJECT_ROOT
$(error YJK_PROJECT_ROOT is not set! Make sure you run the top-level Makefile)
endif

ifndef YJK_ARCH
$(error YJK_ARCH is not set! ! Make sure you run the top-level Makefile)
endif


YJK_MAKE_FLAGS_TEMP   = YJK_PROJECT_ROOT='$(YJK_PROJECT_ROOT)' YJK_ARCH=$(YJK_ARCH)
YJK_MAKE_FLAGS        = $(YJK_MAKE_FLAGS_TEMP) YJK_MAKE_FLAGS="$(YJK_MAKE_FLAGS_TEMP)"

YJK_SUPPORT_DIR        = $(YJK_PROJECT_ROOT)/support
YJK_BUILD_DIR          = $(YJK_PROJECT_ROOT)/build/$(YJK_ARCH)
YJK_INTERMEDIATES_DIR  = $(YJK_BUILD_DIR)/intermediates

YJK_INSTALL_ROOT  = $(YJK_BUILD_DIR)/out
YJK_BOOT_ROOT     = $(YJK_INSTALL_ROOT)/bootroot
YJK_SYS_ROOT      = $(YJK_INSTALL_ROOT)/sysroot
