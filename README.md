# osdev
My osdev adventure. This is continuation of old "isos" project.

## Building (On Ubuntu)

```sh
# Install emulator
sudo apt install qemu-system-x86
# Install dependencies
sudo apt install build-essential bison flex libgmp3-dev libmpc-dev libmpfr-dev texinfo python3 make xorriso
./support/tools/get_limine.sh
# Setup cross-compiler
./support/tools/build_toolchain.sh x86
# Then you can build
make YJK_ARCH=x86
# And run it under QEMU!
make YJK_ARCH=x86 run
```
