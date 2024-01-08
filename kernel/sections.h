// SPDX-FileCopyrightText: (c) 2023 Inseo Oh <dhdlstjtr@gmail.com>
//
// SPDX-License-Identifier: BSD-2-Clause

#define USER_VM_VIRTBASE 0x1000ULL
#define USER_VM_VIRTEND  0x8000000000ULL

#define SCRATCH_PAGE_VIRTBASE      0xffffffff90000000ULL
#define SCRATCH_PAGE_PML1_VIRTBASE 0xffffffffa0000000ULL

#define KERNEL_VM_VIRTBASE 0xffffffffb0000000ULL
#define KERNEL_VM_VIRTEND  0xffffffffbffff000ULL
