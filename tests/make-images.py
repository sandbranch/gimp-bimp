#!/usr/bin/env python3
# Test images for tests/run.sh, written to the folder given:
# photo.png (640x480 RGB, gradients and a white square), photo.jpg (the same
# as JPEG), alpha.png (RGBA with transparent corners), logo.png (a small
# watermark image) and sub/deep.png (for keep folder hierarchy).
# For the edge cases: other pixel formats (gray, gray with alpha, indexed,
# 16-bit RGB, 32-bit float), odd file names, files that do not load, the
# same name in two folders, a JPEG with Exif data and a curves file.
# (multilayer.xcf is made by tests/batch.py, inside GIMP.)
import os
import struct
import sys
import zlib

import numpy as np
from PIL import Image

out = sys.argv[1]
for d in ['sub', 'a', 'b']:
    os.makedirs(os.path.join(out, d), exist_ok=True)

y, x = np.mgrid[0:480, 0:640]
img = np.zeros((480, 640, 3), np.uint8)
img[..., 0] = (x * 255 / 639).astype(np.uint8)
img[..., 1] = (y * 255 / 479).astype(np.uint8)
img[..., 2] = 96
img[200:280, 280:360] = 255
Image.fromarray(img).save(os.path.join(out, 'photo.png'))
Image.fromarray(img).save(os.path.join(out, 'photo.jpg'), quality=95)
Image.fromarray(img[::2, ::2]).save(os.path.join(out, 'sub', 'deep.png'))

rgba = np.dstack([img, np.full((480, 640), 255, np.uint8)])
rgba[:100, :100, 3] = 0
Image.fromarray(rgba, 'RGBA').save(os.path.join(out, 'alpha.png'))

logo = np.zeros((40, 80, 4), np.uint8)
logo[..., 2] = 255
logo[..., 3] = 255
Image.fromarray(logo, 'RGBA').save(os.path.join(out, 'logo.png'))

# pixel formats
gray = img[..., 0]
Image.fromarray(gray, 'L').save(os.path.join(out, 'gray.png'))
Image.fromarray(np.dstack([gray, rgba[..., 3]]), 'LA').save(os.path.join(out, 'gray-alpha.png'))
Image.fromarray(img).quantize(64).save(os.path.join(out, 'indexed.png'))
Image.fromarray(img).quantize(64).save(os.path.join(out, 'indexed.gif'))
Image.fromarray((img[..., 0].astype(np.float32) / 255.0), 'F').save(os.path.join(out, 'float.tif'))


def png16(path, rgb16):
    """a 16-bit RGB PNG (PIL cannot write one)"""
    h, w, _ = rgb16.shape
    raw = b''.join(b'\0' + rgb16[r].astype('>u2').tobytes() for r in range(h))

    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff)

    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n')
        f.write(chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 16, 2, 0, 0, 0)))
        f.write(chunk(b'IDAT', zlib.compress(raw)))
        f.write(chunk(b'IEND', b''))


png16(os.path.join(out, 'rgb16.png'), img.astype(np.uint16) * 257)

# file names
Image.fromarray(img).save(os.path.join(out, 'with space.png'))
Image.fromarray(img).save(os.path.join(out, 'unicode åäö ü.png'))
Image.fromarray(img).save(os.path.join(out, 'UPPER.JPG'), format='JPEG', quality=95)
Image.fromarray(img).save(os.path.join(out, 'noext'), format='PNG')
Image.fromarray(np.full((100, 100, 3), 40, np.uint8)).save(os.path.join(out, 'a', 'dup.png'))
Image.fromarray(np.full((100, 100, 3), 200, np.uint8)).save(os.path.join(out, 'b', 'dup.png'))

# files that do not load
with open(os.path.join(out, 'broken.png'), 'wb') as f:
    f.write(b'\x89PNG\r\n\x1a\n this is not really a PNG file')
open(os.path.join(out, 'empty.png'), 'wb').close()

# Exif data, to see what the exporters keep
exif = Image.Exif()
exif[0x010F] = 'BIMPTEST'  # Make
exif[0x0110] = 'Model 1'   # Model
Image.fromarray(img).save(os.path.join(out, 'exif.jpg'), quality=95, exif=exif.tobytes())

# a curves file as GIMP's curves tool exports it: the value curve takes
# 0.5 to 0.25, the other channels stay


def channel(name, points):
    return ('(channel %s)\n(curve\n    (curve-type smooth)\n    (n-points %d)\n    (points %d %s)\n'
            '    (point-types %d %s)\n    (n-samples 256)\n    (samples 256 %s))\n' % (
                name, len(points) // 2, len(points), ' '.join('%g' % p for p in points),
                len(points) // 2, ' '.join(['smooth'] * (len(points) // 2)),
                ' '.join('%.17g' % (i / 255) for i in range(256))))


with open(os.path.join(out, 'darken.curves'), 'w') as f:
    f.write('# GIMP curves tool settings\n\n(time 0)\n(trc non-linear)\n')
    f.write(channel('value', [0, 0, 0.5, 0.25, 1, 1]))
    for name in ['red', 'green', 'blue', 'alpha']:
        f.write(channel(name, [0, 0, 1, 1]))
    f.write('\n# end of curves tool settings\n')

print('wrote test images to', out)
