#!/usr/bin/env python3
"""Reproducible build of the Galaxy Z Fold3 (q2q) UEFI image.

Clones Project Silicium's Mu-Silicium at a pinned commit, applies the q2q patches
and ACPI tables kept in this folder and produces a boot image ready to be written
to the phone's `boot` partition.

Run it on Linux or inside WSL. The work directory must not contain spaces (EDK2
limitation). Build dependencies: see Mu-Silicium's `setup_env.sh` (git, clang,
lld, llvm, nasm, mono, gcc-aarch64-linux-gnu, uuid-dev, python3-venv) plus `iasl`
(package `acpica-tools`).
"""

import argparse
import hashlib
import os
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
MU_URL = 'https://github.com/Project-Silicium/Mu-Silicium.git'
MU_COMMIT = 'aed54edbb704c62493a1a27bbbc00aa51e5bd179'
MU_PATCH = HERE / 'patches' / 'mu-silicium-q2q.patch'
DSDT_SOURCE = HERE / 'acpi' / 'DSDT.asl'
Q2Q_ACPI = Path('Silicium-ACPI/Platforms/Samsung/q2q')
LAHAINA_APIC = Path('Silicium-ACPI/Silicon/Qualcomm/Lahaina/APIC.aml')
# Upstream Lahaina MADT and the q2q variant: the GICv3 MSI frame is reduced to
# SPI base 0x320 / count 0x40, matching both PCIe MSI windows of the device tree.
APIC_UPSTREAM_SHA256 = '55172d6653343ca62d53c05fbe6a66f735f328c3d4a9525557b5dd5f94fe3704'
APIC_Q2Q_SHA256 = '2a62102e26dbb59ace61675554c5b1d2875441084e9896b1e0304847861833b7'
APIC_MSI_SPI_FIELDS = slice(0x2E9, 0x2ED)  # GIC MSI frame: SPI count (u16), SPI base (u16)
BOOT_PARTITION_SIZE = 100663296


def run(command, cwd=None, env=None):
    print('+', ' '.join(str(part) for part in command), flush=True)
    subprocess.run([str(part) for part in command], cwd=cwd, env=env, check=True)


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def git(workdir, *args, check=True):
    return subprocess.run(['git', '-C', str(workdir), *args], check=check, capture_output=True, text=True)


def prepare_checkout(workdir):
    if not workdir.exists():
        run(['git', 'clone', MU_URL, workdir])
    if git(workdir, 'rev-parse', 'HEAD').stdout.strip() != MU_COMMIT:
        if git(workdir, 'status', '--porcelain', '--untracked-files=no').stdout.strip():
            sys.exit(f'{workdir} has local changes and is not at {MU_COMMIT}; use a clean work directory')
        run(['git', '-C', workdir, 'fetch', 'origin', MU_COMMIT])
        run(['git', '-C', workdir, 'checkout', '--detach', MU_COMMIT])
    if git(workdir, 'apply', '--reverse', '--check', MU_PATCH, check=False).returncode == 0:
        print('q2q patch already applied')
    else:
        run(['git', '-C', workdir, 'apply', '--check', MU_PATCH])
        run(['git', '-C', workdir, 'apply', MU_PATCH])


def python_env(workdir):
    venv = workdir / '.venv'
    if not (venv / 'bin' / 'python').exists():
        run([sys.executable, '-m', 'venv', venv])
        run([venv / 'bin' / 'python', '-m', 'pip', 'install', '-r', workdir / 'pip-requirements.txt'])
    env = dict(os.environ)
    env['PATH'] = f'{venv / "bin"}:{env.get("PATH", "")}'
    env.setdefault('CLANGPDB_AARCH64_PREFIX', 'aarch64-linux-gnu-')
    return venv / 'bin' / 'python', env


def install_acpi(workdir):
    target = workdir / Q2Q_ACPI
    build = workdir / 'Build' / 'q2q-acpi'
    shutil.rmtree(build, ignore_errors=True)
    build.mkdir(parents=True)
    for source in DSDT_SOURCE.parent.glob('*.asl'):
        shutil.copy2(source, build / source.name)
    run(['iasl', '-p', build / 'DSDT', build / 'DSDT.asl'])
    shutil.copy2(build / 'DSDT.aml', target / 'DSDT.aml')

    apic = workdir / LAHAINA_APIC
    data = bytearray(apic.read_bytes())
    if hashlib.sha256(data).hexdigest() != APIC_Q2Q_SHA256:
        if hashlib.sha256(data).hexdigest() != APIC_UPSTREAM_SHA256:
            sys.exit('Unexpected upstream Lahaina APIC.aml; refusing to patch it')
        data[APIC_MSI_SPI_FIELDS] = bytes.fromhex('40002003')
        data[9] = 0
        data[9] = (-sum(data[:int.from_bytes(data[4:8], 'little')])) & 0xFF
        if hashlib.sha256(data).hexdigest() != APIC_Q2Q_SHA256:
            sys.exit('APIC.aml transformation did not produce the expected q2q table')
        apic.write_bytes(data)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--workdir', type=Path, default=Path.home() / 'Mu-Silicium-q2q', help='Mu-Silicium checkout (no spaces)')
    parser.add_argument('--target', choices=['DEBUG', 'RELEASE'], default='DEBUG')
    parser.add_argument('--output', type=Path, default=HERE / 'out')
    args = parser.parse_args()

    if sys.version_info < (3, 11):
        sys.exit('Python 3.11 or newer is required (Mu-Silicium uses tomllib)')
    workdir = args.workdir.expanduser().resolve()
    if ' ' in str(workdir):
        sys.exit('The work directory path must not contain spaces (EDK2 limitation)')
    if shutil.which('iasl') is None:
        sys.exit('iasl not found (install acpica-tools)')

    prepare_checkout(workdir)
    python, env = python_env(workdir)
    device_build = Path('Platforms/Samsung/q2qPkg/DeviceBuild.py')
    run([python, device_build, '--setup', '-t', args.target], cwd=workdir, env=env)
    # Mu_Basecore needs its nested submodules (e.g. MdePkg mipisyst) even if it was first
    # initialised non-recursively.
    run(['git', '-C', workdir, 'submodule', 'update', '--init', '--recursive', 'Mu_Basecore'])
    run([python, device_build, '--update', '-t', args.target], cwd=workdir, env=env)
    install_acpi(workdir)
    (workdir / 'Build').mkdir(exist_ok=True)
    run([python, 'build_uefi.py', '-d', 'q2q', '-r', args.target], cwd=workdir, env=env)

    image = (workdir / 'Mu-q2q.img').read_bytes()
    if len(image) >= BOOT_PARTITION_SIZE:
        sys.exit('Boot image does not fit in the boot partition')
    args.output.mkdir(parents=True, exist_ok=True)
    plain = args.output / f'q2q-uefi-{args.target}.img'
    padded = args.output / f'q2q-uefi-{args.target}-boot-partition.img'
    plain.write_bytes(image)
    padded.write_bytes(image + bytes(BOOT_PARTITION_SIZE - len(image)))
    for path in (plain, padded):
        print(f'{path}  {path.stat().st_size} bytes  sha256 {sha256(path)}')


if __name__ == '__main__':
    main()
