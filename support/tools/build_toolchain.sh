#!/bin/sh
#
# SPDX-FileCopyrightText: 2023, Inseo Oh (dhdlstjtr@gmail.com)
#
# SPDX-License-Identifier: BSD-2-Clause
set -ue

ARCH=$1

case $ARCH in
  "x86")
    TARGET=x86_64-yjk;
    ;;
  *)
    echo "Unknown arch $ARCH";
    exit 1;
    ;;
esac

# sysroot *must* be absolute path, or toolchain build will fail with cryptic error messages.
SYSROOT=$(realpath build/$ARCH/out/sysroot)
SUPPORT_DIR=$PWD/support
PREFIX=$PWD/toolchain/$ARCH

mkdir -p $PREFIX

echo " >>> Prepare libc headers"
# Before we can build GCC, we need to prepare libc header files.
# Normally this is done during OS build, but we need working GCC for that first!
# So that means can't rely on normal build procedure, and we have to copy those manually.
$SUPPORT_DIR/tools/copy_libc_headers.sh $ARCH $PWD

PATH="$PREFIX/bin:$PATH"
OLDPWD=$PWD

DOWNLOAD_DIR=$PREFIX/toolchain_downloads
SRC_DIR=$PREFIX/toolchain_source
BUILD_DIR=$PREFIX/toolchain_build
mkdir -p $DOWNLOAD_DIR
mkdir -p $SRC_DIR
mkdir -p $BUILD_DIR

MAKEJOBS=$(nproc)

BINUTILS_VERSION=2.40
BINUTILS_TARFILE=binutils-$BINUTILS_VERSION.tar.gz
BINUTILS_LOCAL_TARPATH=$DOWNLOAD_DIR/$BINUTILS_TARFILE
BINUTILS_SRC_DIR=$SRC_DIR/binutils-$BINUTILS_VERSION
BINUTILS_BUILD_DIR=$BUILD_DIR/binutils
echo " >>> Blow away existing binutils directories"
rm -rf $BINUTILS_SRC_DIR $BINUTILS_BUILD_DIR
mkdir -p $BINUTILS_BUILD_DIR

GCC_VERSION=12.2.0
GCC_DIRNAME=gcc-$GCC_VERSION
GCC_TARFILE=gcc-$GCC_VERSION.tar.gz
GCC_LOCAL_TARPATH=$DOWNLOAD_DIR/$GCC_TARFILE
GCC_SRC_DIR=$SRC_DIR/gcc-$GCC_VERSION
GCC_BUILD_DIR=$BUILD_DIR/gcc
echo " >>> Blow away existing gcc directories"
rm -rf $GCC_SRC_DIR $GCC_BUILD_DIR
mkdir -p $GCC_BUILD_DIR

echo " >>> Download binutils"
if [ ! -f $BINUTILS_LOCAL_TARPATH ]; then
        wget https://ftp.gnu.org/gnu/binutils/$BINUTILS_TARFILE -O $BINUTILS_LOCAL_TARPATH
fi
echo " >>> Download gcc"
if [ ! -f $GCC_LOCAL_TARPATH ]; then
        wget http://ftp.tsukuba.wide.ad.jp/software/gcc/releases/$GCC_DIRNAME/$GCC_TARFILE -O $GCC_LOCAL_TARPATH
fi

cd $SRC_DIR
echo " >>> Extract binutils"
tar -xf $BINUTILS_LOCAL_TARPATH
echo " >>> Extract gcc"
tar -xf $GCC_LOCAL_TARPATH
echo " >>> Patch GCC"
cd $GCC_SRC_DIR
patch -p0 < $SUPPORT_DIR/patches/gcc.patch
echo " >>> Patch binutils"
cd $BINUTILS_SRC_DIR
patch -p0 < $SUPPORT_DIR/patches/binutils.patch

cd $BINUTILS_BUILD_DIR
echo " >>> Configure binutils"
$BINUTILS_SRC_DIR/configure --target=$TARGET --prefix="$PREFIX" \
        --with-sysroot=$SYSROOT --disable-nls --disable-werror
echo " >>> Compile binutils"
make -j$(MAKEJOBS)
echo " >>> Install binutils"
make install

cd $GCC_BUILD_DIR
echo " >>> Configure GCC"
$GCC_SRC_DIR/configure --target=$TARGET --prefix="$PREFIX" --with-sysroot=$SYSROOT --disable-nls --disable-default-pie --enable-languages=c,c++
echo " >>> Compile gcc"
make all-gcc -j$(MAKEJOBS)
echo " >>> Compile libgcc"
# https://wiki.osdev.org/Building_libgcc_for_mcmodel%3Dkernel
make all-target-libgcc CFLAGS_FOR_TARGET='-g -O2 -mcmodel=large -mno-red-zone'
echo " >>> Install gcc"
make install-gcc install-target-libgcc
cd $GCC_BUILD_DIR
echo " >>> Toolchain build successful"
cd $OLDPWD
