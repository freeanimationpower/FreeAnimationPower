#!/usr/bin/env python3
"""
Post-build script: embed .ico into .exe as native Windows icon resource.
Uses Win32 BeginUpdateResource/UpdateResource/EndUpdateResource.
Usage: embed_icon.py <exe_path> <ico_path>
"""
import sys
import ctypes
from ctypes import wintypes

kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)

BEGIN_UPDATE_RESOURCE_A = kernel32.BeginUpdateResourceA
BEGIN_UPDATE_RESOURCE_A.argtypes = [wintypes.LPCSTR, wintypes.BOOL]
BEGIN_UPDATE_RESOURCE_A.restype = wintypes.HANDLE

UPDATE_RESOURCE_A = kernel32.UpdateResourceA
UPDATE_RESOURCE_A.argtypes = [wintypes.HANDLE, wintypes.LPCSTR, wintypes.LPCSTR, wintypes.WORD, wintypes.LPVOID, wintypes.DWORD]
UPDATE_RESOURCE_A.restype = wintypes.BOOL

END_UPDATE_RESOURCE_A = kernel32.EndUpdateResourceA
END_UPDATE_RESOURCE_A.argtypes = [wintypes.HANDLE, wintypes.BOOL]
END_UPDATE_RESOURCE_A.restype = wintypes.BOOL

RT_ICON = ctypes.c_char_p(3)
RT_GROUP_ICON = ctypes.c_char_p(14)


def read_ico(path):
    """Parse .ico and return list of (width, height, bits_per_pixel, png_data)."""
    with open(path, 'rb') as f:
        data = f.read()

    import struct
    if data[0:4] != b'\x00\x00\x01\x00':
        raise ValueError('Not a valid .ico file')

    count = struct.unpack_from('<H', data, 4)[0]
    images = []
    for i in range(count):
        off = 6 + i * 16
        w, h = data[off], data[off + 1]
        bpp = struct.unpack_from('<H', data, off + 6)[0]
        size = struct.unpack_from('<I', data, off + 8)[0]
        img_off = struct.unpack_from('<I', data, off + 12)[0]
        img_data = data[img_off:img_off + size]
        images.append((w if w != 0 else 256, h if h != 0 else 256, bpp, img_data))

    return images


def build_group_icon(images):
    """Build GRPICONDIR and GRPICONDIRENTRY structures."""
    import struct
    header = struct.pack('<HHH', 0, 1, len(images))  # reserved, type=1(ICO), count

    entries = b''
    id_counter = 1
    for w, h, bpp, img_data in images:
        sz = len(img_data)
        entry = struct.pack('<BBBBHHIH',
            w if w < 256 else 0,
            h if h < 256 else 0,
            0, 0, 1, bpp,
            sz, id_counter)
        entries += entry
        id_counter += 1

    return header + entries


def embed_icon(exe_path, ico_path):
    images = read_ico(ico_path)
    print(f'Read {len(images)} icon entries from {ico_path}')

    handle = BEGIN_UPDATE_RESOURCE_A(exe_path.encode('ascii'), False)
    if not handle:
        err = ctypes.get_last_error()
        raise OSError(f'BeginUpdateResource failed: {err}')

    try:
        # Write each icon image as RT_ICON
        for idx, (w, h, bpp, img_data) in enumerate(images, start=1):
            img_bytes = (ctypes.c_char * len(img_data)).from_buffer_copy(img_data)
            if not UPDATE_RESOURCE_A(handle, RT_ICON, ctypes.c_char_p(str(idx).encode()),
                                    0x0409, img_bytes, len(img_data)):
                raise OSError(f'UpdateResource RT_ICON #{idx} failed')

        # Write RT_GROUP_ICON
        group_data = build_group_icon(images)
        gb = (ctypes.c_char * len(group_data)).from_buffer_copy(group_data)
        if not UPDATE_RESOURCE_A(handle, RT_GROUP_ICON, ctypes.c_char_p(b'IDI_ICON1'),
                                0x0409, gb, len(group_data)):
            raise OSError('UpdateResource RT_GROUP_ICON failed')

        print(f'Group icon written as IDI_ICON1')
    except Exception:
        END_UPDATE_RESOURCE_A(handle, False)
        raise

    if not END_UPDATE_RESOURCE_A(handle, True):
        err = ctypes.get_last_error()
        raise OSError(f'EndUpdateResource failed: {err}')

    print(f'Icon embedded into {exe_path}')


if __name__ == '__main__':
    if len(sys.argv) != 3:
        print(f'Usage: {sys.argv[0]} <exe_path> <ico_path>')
        sys.exit(1)
    embed_icon(sys.argv[1], sys.argv[2])
