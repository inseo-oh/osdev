#!/bin/sh
set -ue
BUILD_DIR_BASE=build

ARCH=$1
case $ARCH in
        "x86")
                TOOLCHAIN_PREFIX=toolchain/x86/bin/x86_64-yjk-
                BUILD_DIR=$BUILD_DIR_BASE/x86
                ;;
        *)
                echo "Unknown ARCH $ARCH"
                exit 1
                ;;
esac

case $2 in
        "build")
                RUN=0
                ;;
        "run")
                RUN=1
                ;;
        *)
                echo "Unknown ARCH $ARCH"
                exit 1
        ;;
esac

OUT_DIR=$BUILD_DIR/out
BOOTROOT=$OUT_DIR/bootroot

################################################################################
# Build OS
################################################################################
OS_CC=$TOOLCHAIN_PREFIX"gcc"
BUILD_INTERMEDIATES_DIR=$BUILD_DIR/intermediates
PARANOID=OFF
# PARANOID=ON

support/tools/copy_libc_headers.sh $ARCH $PWD
mkdir -p $BUILD_INTERMEDIATES_DIR
touch kernel/builddate.h
echo ">>> Compile system"
CC=$OS_CC cmake -B $BUILD_INTERMEDIATES_DIR . -G Ninja \
        -DCMAKE_BUILD_TYPE=Debug -DYJK_ARCH=$ARCH -DYJK_PARANOID=$PARANOID \
        -DCMAKE_INSTALL_PREFIX=$BUILD_DIR/out 
ninja -C $BUILD_INTERMEDIATES_DIR install

################################################################################
# Create font file
################################################################################
echo ">>> Create font file"
support/tools/fontgen.py support/res/unifont-15.1.04.hex $BOOTROOT/yjk/font.bin

################################################################################
# Install bootloader and start the OS
################################################################################

LIMINE_DIR="support/thirdparty/limine"
case $ARCH in
        "x86")
                xorriso -as mkisofs -b limine-bios-cd.bin \
                        -no-emul-boot -boot-load-size 4 -boot-info-table \
                        --efi-boot limine-uefi-cd.bin \
                        -efi-boot-part --efi-boot-image \
                        --protective-msdos-label \
                        $BOOTROOT -o $OUT_DIR/boot.iso
                $LIMINE_DIR/limine bios-install $OUT_DIR/boot.iso

                if [ $RUN -eq 1 ]; then
                        support/tools/run.py
                fi
esac
