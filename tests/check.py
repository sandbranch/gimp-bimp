#!/usr/bin/env python3
# Checks the results of tests/run.sh: one function per manipulation set,
# which looks at the files BIMP wrote into output/<set>/.
import os
import sys

import numpy as np
from PIL import Image

out = sys.argv[1]
only = sys.argv[2] if len(sys.argv) > 2 else ''
src = np.asarray(Image.open(os.path.join(out, 'images', 'photo.png')).convert('RGB')).astype(float)
failures = []


def load(case, name):
    path = os.path.join(out, case, name)
    if not os.path.exists(path):
        raise AssertionError('missing ' + name + ' (found: ' + ', '.join(sorted(os.listdir(os.path.join(out, case)))) + ')')
    return Image.open(path)


def check(cond, message):
    if not cond:
        raise AssertionError(message)


def rgb(im):
    return np.asarray(im.convert('RGB')).astype(float)


def t_resize_percent(c):
    im = load(c, 'photo.png')
    check(im.size == (320, 240), 'size %s, expected 320x240' % (im.size,))


def t_resize_aspect(c):
    im = load(c, 'photo.png')
    check(im.size == (200, 150), 'size %s, expected 200x150' % (im.size,))


def t_resize_pad(c):
    im = load(c, 'photo.png')
    check(im.size == (300, 300), 'size %s, expected 300x300' % (im.size,))
    a = rgb(im)
    check((a[5, 150] == [255, 0, 0]).all(), 'top padding %s, expected red' % a[5, 150])
    check(abs(a[150, 150] - src[240, 320]).max() < 40, 'center %s, expected the photo' % a[150, 150])


def t_resize_resolution(c):
    im = load(c, 'photo.png')
    dpi = im.info.get('dpi', (0, 0))
    check(im.size == (640, 480), 'size changed to %s' % (im.size,))
    check(abs(dpi[0] - 300.5) < 0.5, 'dpi %s, expected 300.5' % (dpi,))


def t_crop_169(c):
    im = load(c, 'photo.png')
    check(im.size == (640, 360), 'size %s, expected 640x360' % (im.size,))
    check(abs(rgb(im)[0, 0] - src[60, 0]).max() < 3, 'not centered')


def t_crop_manual(c):
    im = load(c, 'photo.png')
    check(im.size == (100, 50), 'size %s, expected 100x50' % (im.size,))
    check(abs(rgb(im)[0, 0] - src[0, 0]).max() < 3, 'not from the top left')


def t_crop_custom(c):
    im = load(c, 'photo.png')
    check(im.size == (640, 256), 'size %s, expected 640x256 (2.5:1)' % (im.size,))


def t_fliprotate(c):
    im = load(c, 'photo.png')
    check(im.size == (480, 640), 'size %s, expected 480x640' % (im.size,))
    expected = np.rot90(src[:, ::-1], k=-1)
    check(abs(rgb(im) - expected).max() < 3, 'not flipped and rotated 90 degrees clockwise')


def t_color_brightness(c):
    a = rgb(load(c, 'photo.png'))
    check(a.mean() > src.mean() + 20, 'mean %.1f, source %.1f: not brighter' % (a.mean(), src.mean()))


def t_color_grayscale(c):
    im = load(c, 'photo.png')
    a = rgb(im)
    check(abs(a[..., 0] - a[..., 1]).max() < 2 and abs(a[..., 1] - a[..., 2]).max() < 2, 'not gray')
    check(a.min() < 5 and a.max() > 250, 'levels not stretched (%d..%d)' % (a.min(), a.max()))


def t_sharpblur_blur(c):
    a = rgb(load(c, 'photo.png'))
    edge = abs(np.diff(a[240, 270:290, 0])).max()
    check(edge < 60, 'square edge still sharp (%d)' % edge)


def t_sharpblur_sharpen(c):
    a = rgb(load(c, 'photo.png'))
    check(abs(a - src).max() > 10, 'unchanged')
    inside = a[240, 282:284, 0].mean()
    outside = a[240, 276:278, 0].mean()
    check(inside >= 250 and outside < src[240, 277, 0] + 1, 'no sharpening halo at the square')


def t_watermark_text(c):
    a = rgb(load(c, 'photo.png'))
    diff = abs(a - src).max(axis=2)
    ys, xs = np.nonzero(diff > 30)
    check(len(xs) > 100, 'no text drawn')
    check(xs.min() > 400 and ys.min() > 380, 'text not at the bottom right (from %d,%d)' % (xs.min(), ys.min()))
    check(xs.max() <= 630 and ys.max() <= 470, 'text not 10 px from the edge (to %d,%d)' % (xs.max(), ys.max()))
    red = a[ys, xs]
    check((red[:, 0] > red[:, 1] + 50).mean() > 0.5, 'text not red')


def t_watermark_image(c):
    a = rgb(load(c, 'photo.png'))
    # logo 80x40 scaled to 25 % of the width: 160x80, at the top left, 50 % opaque blue
    check(a[40, 80, 2] > src[40, 80, 2] + 50, 'no blue logo at the top left')
    check(abs(a[40, 170] - src[40, 170]).max() < 3, 'logo wider than 160 px')
    check(a[40, 80, 2] < 240, 'logo not half transparent')


def t_format_jpeg(c):
    im = load(c, 'photo.jpg')
    check(im.format == 'JPEG', 'format ' + str(im.format))
    check(im.info.get('progressive') or im.info.get('progression'), 'not progressive')
    check(b'made by BIMP' in open(os.path.join(out, c, 'photo.jpg'), 'rb').read(), 'no comment')


def t_format_png(c):
    im = load(c, 'photo.png')
    check(im.format == 'PNG' and im.size == (640, 480), 'format ' + str(im.format))


def fmt(ext, name):
    def f(c):
        im = load(c, 'photo.' + ext)
        check(im.format == name, 'format %s, expected %s' % (im.format, name))
    return f


def t_format_heif(c):
    path = os.path.join(out, c, 'photo.heif')
    check(os.path.exists(path) and os.path.getsize(path) > 1000, 'no photo.heif')
    check(b'ftyp' in open(path, 'rb').read(64), 'not an ISO media (HEIF) file')


def t_format_avif(c):
    path = os.path.join(out, c, 'photo.avif')
    check(os.path.exists(path) and os.path.getsize(path) > 500, 'no photo.avif')
    check(b'avif' in open(path, 'rb').read(64), 'not an AVIF file')


def t_format_exr(c):
    path = os.path.join(out, c, 'photo.exr')
    check(os.path.exists(path) and open(path, 'rb').read(4) == b'\x76\x2f\x31\x01', 'not an OpenEXR file')


def t_format_tiff(c):
    im = load(c, 'photo.tiff')
    check(im.format == 'TIFF', 'format ' + str(im.format))
    check(im.info.get('compression') == 'tiff_lzw', 'compression ' + str(im.info.get('compression')))


def t_format_ico(c):
    im = load(c, 'photo.ico')
    check(im.format == 'ICO' and im.size == (64, 64), 'format %s size %s' % (im.format, im.size))


def t_rename(c):
    load(c, 'out_photo_1.png')


def t_userdef_invert(c):
    a = rgb(load(c, 'photo.png'))
    check(abs(a - (255 - src)).max() < 3, 'not inverted')


def t_keep_original_jpeg(c):
    check(load(c, 'photo.jpg').format == 'JPEG', 'photo.jpg not JPEG')
    im = load(c, 'alpha.png')
    check(im.mode == 'RGBA' and im.getpixel((10, 10))[3] == 0, 'alpha.png lost its transparency')


def t_hierarchy(c):
    load(c, 'photo.png')
    load(c, os.path.join('sub', 'deep.png'))


def t_chain(c):
    for name in ['photo-small.jpg', 'alpha-small.jpg']:
        im = load(c, name)
        check(im.size == (320, 240) and im.format == 'JPEG', '%s: %s %s' % (name, im.format, im.size))
    load(c, 'photo-small.jpg')


def t_bimp2_compat(c):
    im = load(c, 'photo.png')
    check(im.size == (320, 240), 'size %s, expected 320x240' % (im.size,))
    a = rgb(im)
    check(abs(a[..., 0] - a[..., 2]).max() < 2, 'not gray')


tests = {
    'format-gif': fmt('gif', 'GIF'), 'format-bmp': fmt('bmp', 'BMP'),
    'format-tga': fmt('tga', 'TGA'), 'format-webp': fmt('webp', 'WEBP'),
}
for name, fn in list(globals().items()):
    if name.startswith('t_'):
        tests[name[2:].replace('_', '-')] = fn

cases = sorted(d for d in os.listdir(out) if os.path.isdir(os.path.join(out, d)) and d != 'images')
sets = sorted(f[:-5] for f in os.listdir(os.path.join(os.path.dirname(__file__), 'sets')) if f.endswith('.bimp'))
for case in sets:
    if only and not case.startswith(only):
        continue
    if case not in tests:
        print('NO CHECK', case)
        continue
    try:
        if case not in cases:
            raise AssertionError('not run')
        tests[case](case)
        print('ok  ', case)
    except AssertionError as e:
        failures.append(case)
        print('FAIL', case, '-', e)

print('%d failed of %d' % (len(failures), len([s for s in sets if s.startswith(only)])))
sys.exit(1 if failures else 0)
