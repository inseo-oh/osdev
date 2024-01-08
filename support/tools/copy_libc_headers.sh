#!/bin/sh
#
# SPDX-FileCopyrightText: 2023, Inseo Oh (dhdlstjtr@gmail.com)
#
# SPDX-License-Identifier: BSD-2-Clause
set -ue

ARCH=$1
SOURCE_ROOT=$2

case $ARCH in
  "x86")
    ;;
  *)
    echo "Unknown arch $ARCH";
    exit 1;
    ;;
esac


SYSROOT=$SOURCE_ROOT/build/$ARCH/out/sysroot

for LIBC_DIR in $SOURCE_ROOT/userland/libc $SOURCE_ROOT/kernel/api; do
        LIBC_HEADER_FILES=$(find $LIBC_DIR -name *.h -print)
        for FILE in $LIBC_HEADER_FILES; do
                DEST_PATH=$(echo $FILE | sed "s:$LIBC_DIR:$SYSROOT/usr/include:")
                mkdir -p $(dirname $DEST_PATH)
                cp -v $FILE $DEST_PATH
        done
done
