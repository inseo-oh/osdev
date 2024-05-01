#!/usr/bin/env python3
#
# SPDX-FileCopyrightText: 2023, Inseo Oh (dhdlstjtr@gmail.com)
#
# SPDX-License-Identifier: BSD-2-Clause
import subprocess
import os
import re
import sys

def get_windows_reg(key, name):
        result = subprocess.run(["reg.exe", "QUERY", key, "/v", name], capture_output=True)
        for x in result.stdout.decode('utf-8').split('\r\n'):
                match_result=re.match(f'^\s*{name}\s+REG_[A-Z_]+\s+(.*)', x)
                if match_result:
                        return match_result[1]

def windows_path_to_wsl(path):
        result = subprocess.run(["wslpath", "-a", path], capture_output=True)
        return result.stdout.decode('utf-8').splitlines()[0]

def wsl_path_to_windows(path):
        result = subprocess.run(["wslpath", "-a", "-w", path], capture_output=True)
        return result.stdout.decode('utf-8').splitlines()[0]

IS_WSL2 = os.uname().release.find("WSL2") != -1

use_gdbstub = False
if os.getenv('YJK_USE_GDBSTUB') == '1':
        use_gdbstub = True

cdrom_path="boot.iso"

qemu_args = [
        "qemu-system-x86_64",
        "-m", "128M",
        "-serial", "mon:stdio",
        "-net", "none",
        "-cpu", "qemu64,smap,smep",
        "-display", "sdl",
        "-d", "guest_errors",
        "-smp", "8",
        # "-no-reboot",
        # "-nographic",
]

if use_gdbstub:
        qemu_args.append("-s")

if IS_WSL2:
        # As of QEMU 8.1.94, QEMU doesn't like loading CD-ROM images from WSL side:
        # ERROR:../../../block.c:1699:bdrv_open_driver: assertion failed: (is_power_of_2(bs->bl.request_alignment))
        cdrom_path = wsl_path_to_windows("build/x86/out/boot.iso")

        sys.stderr.write(" * WSL2 detected. Using Windows QEMU...\n")
        qemu_base_dir_win = get_windows_reg("HKLM\\SOFTWARE\\QEMU", "Install_Dir")
        if qemu_base_dir_win == None:
                sys.stderr.write("WARNING: QEMU installation directory cannot be determined.\n")
                sys.stderr.write("WARNING: Assuming QEMU is in Windows PATH.\n")
                qemu_base_dir = ""
        else:
                sys.stderr.write(f" * Windows QEMU installation found: {qemu_base_dir_win}\n")
                qemu_base_dir = windows_path_to_wsl(qemu_base_dir_win)
                sys.stderr.write(f" * UNIX-style path of the QEMU directory: {qemu_base_dir}\n")

        if not use_gdbstub:
                qemu_args.append('-accel')
                # kernel-irqchip=off was added because otherwise it infinitely shows:
                # "whpx: injection failed, MSI (0, 0) delivery: 0, dest_mode: 0, trigger mode: 0, vector: 0, lost (c0350005)"
                #
                # This seems to happen around when we setup I/O APIC redirections, so it might be related to that.
                qemu_args.append('whpx,kernel-irqchip=off')
        else:
                sys.stderr.write("NOTE: As of QEMU 8.1.94, WHPX accel doesn't support guest debugging\n")
                sys.stderr.write("NOTE: Disabling WHPX support\n")
        qemu_args[0] = f"{qemu_base_dir}/qemu-system-x86_64.exe"
else:
        qemu_args.append('-enable-kvm')

qemu_args.append("-cdrom")
qemu_args.append(cdrom_path)

sys.stderr.write(f" * QEMU executable: {qemu_args[0]}\n")
sys.stderr.write(f" * CD-ROM image path: {cdrom_path}\n")
print(qemu_args)
subprocess.run(qemu_args)