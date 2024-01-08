# ISOS

```
------------------------------------------------------------
                      Welcome to ISOS.
         Kernel image timestamp: Jan  8 2024 23:48:31
      Number of processors: 2    Size of memory: 123MiB
------------------------------------------------------------
I [0.000] smpboot | APIC 0 is BSP. Skipping...
I [0.091] lapic | [LAPIC 1] Configuring LINT1 as NMI
I [0.092] smpboot | AP boot complete
I [0.093] boot-stage2(bsp) | Launch userspace service
I [0.094] boot-stage2(bsp) | The system is ready for use
> 
```

My personal, small hobby OS for learning how x86-64 PCs work, and also a playground for playing with various dumb ideas(at least for now). 

**WARNING**: This OS is basically useless to anyone. There are almost zero user commands, as I was focusing on fun debugging with SMP and memory management recently. Pain and suffering is my life.

But despite its dumbness, it still has:

- SMP support up to 16 processors (That also means it uses Local APIC and I/O APIC as interrupt controller)
- Inter-processor interrupt for TLB synchronization and halting other processors on kernel panic.
- Preemptive multitasking(though scheduler is not really optimized for SMP)
- Multithreading
- Virtual memory and basic memory management
- Graphical console output, with my favorite light-pink as text color!
- Kernel CLI (which you can't really do anything fun with yet)
- UBSan
- 

And it also has support for userspace, but I'm currently not focusing on that(and might even get dropped in the future depending on what I decide to do with my OS):

- Kernel memory protected from userspace
- System calls(using SYSCALL/SYSRET)
- `x86_64-isos-gcc` toolchain
- One userspace application

**NOTE:** None of the above are guranteed to work, of course.

## Build instructions

In case you want to try out this mess yourself, here are instructions(which may or may not work haha):

Under Ubuntu:
```sh
# Install dependencies
sudo apt install build-essential bison flex libgmp3-dev libmpc-dev libmpfr-dev texinfo python3 cmake ninja-build
# (This is proably incomplete list of packages)
# And you probably also want an emulator:
sudo apt install qemu-system-x86
# Prior to build, you need to download Limine:
./support/tools/get_limine.sh
# and build GCC cross-compiler:
./support/tools/build_toolchain.sh x86
# Then you can build and run it
./build.sh x86 run
# If you want to only build and not run, replace 'run' with 'build':
./build.sh x86 build
```
(Obviously, you should ignore line starting with #. That's called comments!)
