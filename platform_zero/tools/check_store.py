#!/usr/bin/env python3
"""Read-only checks for the Zero listing and an optional CPack Debian package."""
import argparse
import io
import json
from pathlib import Path
import re
import struct
import tarfile

REPO = Path(__file__).resolve().parents[2]
PERMISSIONS = {'camera', 'microphone', 'imu', 'network', 'additional_hardware',
               'background_service', 'external_display'}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def png_size(path):
    header = path.read_bytes()[:24]
    require(len(header) == 24 and header[:8] == b'\x89PNG\r\n\x1a\n' and header[12:16] == b'IHDR',
            f'{path}: expected a PNG')
    return struct.unpack('>II', header[16:24])


def archive(deb, prefix):
    with deb.open('rb') as package:
        require(package.read(8) == b'!<arch>\n', 'Expected a Debian ar archive')
        while True:
            header = package.read(60)
            if not header:
                break
            require(len(header) == 60 and header[-2:] == b'`\n', 'Malformed Debian archive header')
            size = int(header[48:58].strip())
            require(size >= 0, 'Invalid archive member size')
            name = header[:16].decode('ascii').strip().rstrip('/')
            body = package.read(size)
            require(len(body) == size, 'Truncated Debian archive member')
            if size % 2:
                package.read(1)
            if name.startswith('#1/'):
                length = int(name[3:])
                name = body[:length].rstrip(b'\0').decode('ascii')
                body = body[length:]
            if name.startswith(prefix + '.tar'):
                return tarfile.open(fileobj=io.BytesIO(body), mode='r:*')
    raise ValueError(f'Debian package is missing {prefix}.tar')


def check(deb=None):
    manifest = json.loads((REPO / 'app-builder.json').read_text())
    store = manifest['store']
    require(re.fullmatch(r'[a-z0-9][a-z0-9.+-]+', manifest['package_name']), 'Invalid Debian package name')
    require(manifest['app_name'].strip(), 'Application name is required')
    require(0 < len(store['summary']) <= 80 and '\n' not in store['summary'], 'Summary must be one line, at most 80 characters')
    require(store['categories'] == ['Creative & Office'], 'Review the application category')
    require(set(store['permissions']) == PERMISSIONS and
            all(type(value) is bool for value in store['permissions'].values()),
            'Declare all seven permissions as booleans')
    require(store['permissions']['imu'] and store['permissions']['network'],
            'Declare shake undo and Lospec access')
    require(not any(store['permissions'][name] for name in PERMISSIONS - {'imu', 'network'}),
            'Review unexpected Zero permissions')
    require(store['author']['display_name'].strip(), 'Author name is required')
    require(store['license'] == 'MIT', 'Keep the listing license aligned with LICENSE')
    require(re.fullmatch(r'[A-Z0-9]{4}', store['share_code']), 'Share code must be four letters/digits')
    width, height = png_size(REPO / store['icon'])
    require(width == height and 128 <= width <= 512, 'Icon must be a square PNG, 128–512 pixels')
    require(1 <= len(store['screenshots']) <= 6, 'Provide 1–6 screenshots')
    for path in store['screenshots']:
        require(png_size(REPO / path) == (320, 170), f'{path}: screenshot must be exactly 320×170')
    print('Listing metadata, permissions, icon, and screenshots pass local checks.')
    if deb is None:
        return
    require(deb.name == f"{manifest['package_name']}_{manifest['version']}_arm64.deb",
            'Use the versioned store package filename, not the local install alias')
    with archive(deb, 'control') as control:
        members = {m.name.removeprefix('./'): m for m in control.getmembers()}
        fields = dict(line.split(': ', 1) for line in
                      control.extractfile(members['control']).read().decode().splitlines()
                      if ': ' in line and not line.startswith(' '))
        require(fields.get('Package') == manifest['package_name'], 'Package name differs from listing')
        require(fields.get('Version') == manifest['version'], 'Package version differs from listing')
        require(fields.get('Architecture') == 'arm64', 'Store package must be ARM64')
        require(fields.get('Maintainer', '').startswith('Brad McNally'), 'Package maintainer is missing or unexpected')
    with archive(deb, 'data') as data:
        files = {m.name.removeprefix('./'): m for m in data.getmembers() if m.isfile()}
        base = 'usr/share/APPLaunch/'
        expected = [base + 'applications/bitmap16dx.desktop', base + 'bin/bitmap16dx',
                    base + 'apps/bitmap16dx/bitmap16dx_desktop',
                    base + 'share/images/bitmap16dx.png', 'usr/share/doc/bitmap16dx/copyright',
                    'usr/share/doc/bitmap16dx/licenses/picojson-license.h']
        for path in expected:
            require(path in files, f'Package is missing {path}')
        binary = data.extractfile(files[expected[2]]).read(20)
        require(len(binary) >= 20 and binary[:4] == b'\x7fELF' and binary[4:6] == b'\x02\x01' and
                struct.unpack('<H', binary[18:20])[0] == 183,
                'Application binary must be a Linux ARM64 ELF executable')
        require(files[expected[1]].mode & 0o111, 'Launcher must be executable')
        require(files[expected[2]].mode & 0o111, 'Application must be executable')
        for path in files:
            require(path.startswith((base, 'usr/share/doc/bitmap16dx/')),
                    f'Unexpected installed file: {path}')
    print('Package identity, ARM64 binary, launcher paths, and license files pass.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--deb', type=Path)
    args = parser.parse_args()
    try:
        check(args.deb)
    except (ValueError, KeyError, OSError, StopIteration, tarfile.TarError) as error:
        parser.exit(1, f'Store check failed: {error}\n')
