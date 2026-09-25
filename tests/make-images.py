#!/usr/bin/env python3
# Test images for tests/run.sh, written to the folder given:
# photo.png (640x480 RGB, gradients and a white square), photo.jpg (the same
# as JPEG), alpha.png (RGBA with transparent corners), logo.png (a small
# watermark image) and sub/deep.png (for keep folder hierarchy).
import os
import sys

import numpy as np
from PIL import Image

out = sys.argv[1]
os.makedirs(os.path.join(out, 'sub'), exist_ok=True)

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
print('wrote test images to', out)
