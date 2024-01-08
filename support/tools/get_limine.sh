#!/bin/sh
#
# SPDX-FileCopyrightText: 2023, Inseo Oh (dhdlstjtr@gmail.com)
#
# SPDX-License-Identifier: BSD-2-Clause
set -ue

PREFIX=$PWD/support/thirdparty/limine

rm -rf $PREFIX
git clone https://github.com/limine-bootloader/limine.git $PREFIX --branch=v5.x-branch-binary --depth=1
make -C $PREFIX
