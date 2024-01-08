#!/bin/sh
set -ue

DIRS='kernel userland'
echo ">>> Searching for C files"
C_FILENAMES=$(find $DIRS -iname '*.c')
echo ">>> Searching for header files"
H_FILENAMES=$(find $DIRS -iname '*.h')
FILENAMES="$C_FILENAMES $H_FILENAMES"
echo ">>> Running clang-format"
clang-format -i $FILENAMES