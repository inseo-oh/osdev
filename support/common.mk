# SPDX-FileCopyrightText: (c) 2024 Inseo Oh <dhdlstjtr@gmail.com>
#
# SPDX-License-Identifier: BSD-2-Clause

# Variables that needs to be set:
# - YJK_TARGET_NAME     : Name of the target.
# - YJK_OBJS            : List of object files(Not includding CRT files).
#                        Build system will build necessary 
# - YJK_TARGET_TYPE     : Specifies type of the target. Valid options are: EXEC, STATIC_LIB, MANUAL_LINK
#     If you specify MANUAL_LINK, it will disable built-in target link recipe, so that you can write your own.
# - YJK_INSTALL_DIR     : Installation directory
#
# These are optional:
# - YJK_HOSTBUILD       : This tells build system that we are compiling for host
#
# Optionally, you may include C runtime objects if necessary:
# - YJK_EXPLICIT_CRT    : *Must be set to use below options*
# - YJK_CRTI_OBJ        : crti.o file path.
# - YJK_CRTN_OBJ        : crtn.o file path.
#
# These are set by common.mk, but is also safe to modify after including this file:
# - YJK_COMMON_FLAGS : Common flags used by C, C++, and also during link.
# - YJK_LINK_FLAGS   : Flags used exclusively for linking.
#
# Also see env.mk, which defines some other useful variables.

ifndef YJK_TARGET_NAME
$(error YJK_TARGET_NAME is not set! Check the subproject Makefile)
endif

ifndef YJK_ARCH
$(error YJK_ARCH is not set! Make sure you run the top-level Makefile)
endif

ifndef YJK_PROJECT_ROOT
$(error YJK_PROJECT_ROOT is not set! Make sure you run the top-level Makefile)
endif

ifndef YJK_OBJS
$(error YJK_OBJS is not set! Check the subproject Makefile)
endif

ifndef YJK_INSTALL_DIR
$(error YJK_INSTALL_DIR is not set! Check the subproject Makefile)
endif

ifeq ($(YJK_TARGET_TYPE), EXEC)
else ifeq ($(YJK_TARGET_TYPE), STATIC_LIB)
else ifeq ($(YJK_TARGET_TYPE), MANUAL_LINK)
else
$(error YJK_TARGET_TYPE is not set or is invalid! Check the subproject Makefile)
endif

ifdef YJK_EXPLICIT_CRT
ifndef YJK_CRTI_OBJ
$(error YJK_CRTI_OBJ must be set when YJK_EXPLICIT_CRT is set! Check the subproject Makefile)
else ifndef YJK_CRTN_OBJ
$(error YJK_CRTN_OBJ must be set when YJK_EXPLICIT_CRT is set! Check the subproject Makefile)
endif
else ifdef YJK_CRTI_OBJ
$(error YJK_CRTI_OBJ is set, but YJK_EXPLICIT_CRT is not set! Check the subproject Makefile)
else ifdef YJK_CRTN_OBJ
$(error YJK_CRTN_OBJ is set, but YJK_EXPLICIT_CRT is not set! Check the subproject Makefile)
endif

include $(YJK_PROJECT_ROOT)/support/env.mk

YJK_OBJ_DIR            = $(YJK_INTERMEDIATES_DIR)/obj/$(YJK_TARGET_NAME)
YJK_TARGET             = $(YJK_INTERMEDIATES_DIR)/out/$(YJK_TARGET_NAME)

# $(info [$(YJK_TARGET_NAME)] Arch is $(YJK_ARCH))

ifndef YJK_HOSTBUILD
CC      = $(YJK_PROJECT_ROOT)/toolchain/$(YJK_ARCH)/bin/x86_64-yjk-gcc
CXX     = $(YJK_PROJECT_ROOT)/toolchain/$(YJK_ARCH)/bin/x86_64-yjk-g++
AR      = $(YJK_PROJECT_ROOT)/toolchain/$(YJK_ARCH)/bin/x86_64-yjk-ar
endif

ifdef YJK_EXPLICIT_CRT
YJK_CRTBEGIN_OBJ := $(shell $(CXX) $(YJK_COMMON_FLAGS) -print-file-name=crtbegin.o)
YJK_CRTEND_OBJ   := $(shell $(CXX) $(YJK_COMMON_FLAGS) -print-file-name=crtend.o)
YJK_INTERNAL_OBJS = $(addprefix $(YJK_OBJ_DIR)/, $(YJK_CRTI_OBJ) $(YJK_CRTN_OBJ) $(YJK_OBJS))
# CRT files *must* be linked in very specific order.  
YJK_LINK_OBJS     = $(addprefix $(YJK_OBJ_DIR)/, $(YJK_CRTI_OBJ))
YJK_LINK_OBJS     += $(YJK_CRTBEGIN_OBJ)
YJK_LINK_OBJS     += $(addprefix $(YJK_OBJ_DIR)/, $(YJK_OBJS))
YJK_LINK_OBJS     += $(YJK_CRTEND_OBJ)
YJK_LINK_OBJS     += $(addprefix $(YJK_OBJ_DIR)/, $(YJK_CRTN_OBJ))

else
YJK_INTERNAL_OBJS = $(addprefix $(YJK_OBJ_DIR)/, $(YJK_OBJS))
YJK_LINK_OBJS     = $(YJK_INTERNAL_OBJS)
endif

YJK_DEPS     = $(patsubst %.o, %.d, $(YJK_INTERNAL_OBJS))
# sort removes duplicates
YJK_OBJ_DIRS   = $(sort $(dir $(YJK_INTERNAL_OBJS)))

# TODO: Remove -fno-stack-protector when SSP is ready
YJK_COMMON_FLAGS     = -Wall -Werror -Wextra -fno-stack-protector -I$(YJK_PROJECT_ROOT)

ifdef YJK_DEBUGBUILD
YJK_COMMON_FLAGS += -g -fsanitize=undefined
else
YJK_COMMON_FLAGS += -O3 -DNDEBUG
# LTO should be disabled for static libraries, at least for now.
ifneq ($(YJK_TARGET_TYPE), STATIC_LIB)
YJK_COMMON_FLAGS += -flto=auto
endif
endif

YJK_LINK_FLAGS = 
YJK_ARCH_FLAGS = 

########## Arch-specific options ##########
# - X86
ifeq ($(YJK_ARCH), x86)
YJK_ARCH_FLAGS += -mgeneral-regs-only -mno-mmx -mno-sse -mno-sse2 -masm=intel
# - Invalid architecture
else
$(error $(YJK_ARCH) is not a valid architecture)
endif

########## Common options for non-host targets ##########
ifndef YJK_HOSTBUILD
YJK_COMMON_FLAGS += $(YJK_ARCH_FLAGS)
endif

CFLAGS         = $(YJK_COMMON_FLAGS) -std=c11 
CXXFLAGS       = $(YJK_COMMON_FLAGS) -std=c++17 -fno-exceptions -fno-rtti

# $(info [$(YJK_TARGET_NAME)] CFLAGS         : $(CFLAGS))
# $(info [$(YJK_TARGET_NAME)] CXXFLAGS       : $(CXXFLAGS))
# $(info [$(YJK_TARGET_NAME)] YJK_LINK_FLAGS : $(YJK_LINK_FLAGS))

all: prepare $(YJK_TARGET)
	cp $(YJK_TARGET) $(YJK_INSTALL_DIR)

clean:
	rm -f $(YJK_INTERNAL_OBJS) $(YJK_DEPS) $(YJK_TARGET)

cleandep:
	rm -f $(YJK_DEPS)

$(YJK_OBJ_DIR)/%.o: %.S
	$(CC) -o $@ -c -MMD $< $(YJK_COMMON_FLAGS)

$(YJK_OBJ_DIR)/%.o: %.c
	$(CC) -o $@ -c -MMD $< $(CFLAGS)

$(YJK_OBJ_DIR)/%.o: %.cc
	$(CXX) -o $@ -c -MMD $< $(CXXFLAGS)

$(YJK_TARGET): prepare
$(YJK_TARGET): $(YJK_LINK_OBJS)

ifeq ($(YJK_TARGET_TYPE), STATIC_LIB)
$(YJK_TARGET):
	$(AR) rcs $@ $(YJK_LINK_OBJS)
else ifeq ($(YJK_TARGET_TYPE), EXEC)
$(YJK_TARGET):
	$(CC) -o $@ $(YJK_LINK_OBJS) $(YJK_LINK_FLAGS) $(YJK_COMMON_FLAGS)
else ifeq ($(YJK_TARGET_TYPE), MANUAL_LINK)
else
	$(error Invalid YJK_TARGET_TYPE $(YJK_TARGET_TYPE))
endif

prepare:
	mkdir -p $(dir $(YJK_TARGET)) $(YJK_OBJ_DIRS) $(YJK_INSTALL_DIR)

-include $(YJK_DEPS)

.PHONY: all prepare clean cleandep

