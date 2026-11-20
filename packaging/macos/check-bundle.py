#!/usr/bin/env python3
"""Reject macOS release bundles that depend on the build machine."""

import argparse
import platform
import plistlib
import re
import subprocess
from pathlib import Path


# The Mac it runs on: make-zip.sh builds for that one.
ARCH = platform.machine()


def output(*command):
    return subprocess.check_output(command, text=True, stderr=subprocess.STDOUT)


def version(value):
    parts = tuple(map(int, value.split('.')))
    return parts + (0,) * (3 - len(parts))


def check(app):
    contents = app / 'Contents'
    with (contents / 'Info.plist').open('rb') as source:
        info = plistlib.load(source)
    if info.get('LSMinimumSystemVersion') != '14.2':
        raise ValueError('The release bundle must declare macOS 14.2')
    if not info.get('NSMicrophoneUsageDescription'):
        raise ValueError('Missing microphone usage description')
    executable = contents / 'MacOS' / info['CFBundleExecutable']
    frameworks = contents / 'Frameworks'
    for name in ('libsotto.0.dylib', 'libcodec2.1.2.dylib'):
        if not (frameworks / name).is_file():
            raise ValueError(f'Missing voice codec: {name}')
    for name in ('wxWidgets', 'Hamlib', 'OpenJPEG', 'libwebp', 'libde265',
                 'libheif', 'RNNoise', 'Cool-chic', 'sotto', 'codec2'):
        if not (contents / 'Resources' / 'licences' / f'{name}.txt').is_file():
            raise ValueError(f'Missing licence: {name}')
    if not executable.is_file():
        raise ValueError('Missing app executable')

    binaries = [executable] + sorted(frameworks.glob('*.dylib'))
    for binary in binaries:
        if not binary.resolve().is_relative_to(app.resolve()):
            raise ValueError(f'Library symlink leaves the bundle: {binary.name}')
        if output('lipo', '-archs', str(binary)).strip() != ARCH:
            raise ValueError(f'Expected an {ARCH} binary: {binary.name}')
        loads = output('otool', '-l', str(binary))
        minimum = re.search(r'\bminos\s+(\S+)', loads)
        if minimum is None or version(minimum[1]) > version('14.2'):
            raise ValueError(f'Binary requires a newer macOS: {binary.name}')
        for path in re.findall(r'\bpath (.*?) \(offset \d+\)', loads):
            if not path.startswith(('@loader_path', '@executable_path')):
                raise ValueError(f'Nonrelocatable rpath in {binary.name}: {path}')
        for line in output('otool', '-L', str(binary)).splitlines()[1:]:
            dependency = line.strip().split(' (', 1)[0]
            if dependency.startswith(('/System/Library/', '/usr/lib/')):
                continue
            if dependency.startswith('@rpath/'):
                target = frameworks / dependency.removeprefix('@rpath/')
            elif dependency.startswith('@loader_path/'):
                target = binary.parent / dependency.removeprefix('@loader_path/')
            elif dependency.startswith('@executable_path/'):
                target = executable.parent / dependency.removeprefix('@executable_path/')
            else:
                raise ValueError(f'External dependency in {binary.name}: {dependency}')
            if not target.resolve().is_relative_to(app.resolve()) or not target.is_file():
                raise ValueError(f'Unresolved dependency in {binary.name}: {dependency}')
    subprocess.run(['codesign', '--verify', '--deep', '--strict', str(app)], check=True)
    print(f'Bundle verified: {ARCH}, macOS 14.2, bundled codecs and licences, no external libraries.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('app', type=Path)
    args = parser.parse_args()
    try:
        check(args.app)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'Bundle check failed: {error}\n')
