# osdev
My osdev adventure. This is continuation of old "isos" project.

## Building (On Ubuntu)

```sh
# Install emulator
sudo apt install qemu-system-x86
# Install dependencies
sudo apt install build-essential bison flex libgmp3-dev libmpc-dev libmpfr-dev texinfo python3 cmake ninja-build xorriso clang-tidy
./support/tools/get_limine.sh
# Setup cross-compiler
./support/tools/copy_libc_headers.sh x86 .
./support/tools/build_toolchain.sh x86
# Then you can build and run it inside QEMU
./build.sh x86 run
# If you want to only build and not run, replace 'run' with 'build':
./build.sh x86 build
```
