#!/usr/bin/env python3
# Checks the results of tests/run.sh: what plug-in-bimp-batch returned for
# each set (results.json), against the "# expect:" comment of the set, and
# one function per set, which looks at the files BIMP wrote into <set>/.
#   # expect: status=execution-error     the procedure fails (default success)
#   # expect: errors=1 processed=2       its return values (default: no
#                                        errors, every input processed)
# Prints PASS or FAIL per set and exits with 1 if any failed.
import glob
import json
import os
import shlex
import sys

import numpy as np
from PIL import Image

out = sys.argv[1]
only = sys.argv[2] if len(sys.argv) > 2 else ''
here = os.path.dirname(os.path.abspath(__file__))
images = os.path.join(out, 'images')
src = np.asarray(Image.open(os.path.join(images, 'photo.png')).convert('RGB')).astype(float)
failures = []
OLD_DATE = 978307200  # the date of the files tests/batch.py puts there


def load(case, name):
    path = os.path.join(out, case, name)
    if not os.path.exists(path):
        raise AssertionError('missing ' + name + ' (found: ' + ', '.join(sorted(os.listdir(os.path.join(out, case)))) + ')')
    try:
        return Image.open(path)
    except Exception as e:
        raise AssertionError('cannot read %s: %s' % (name, e))


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


# edge cases: set files


def t_set_no_header(c):
    t_resize_percent(c)


def t_set_unknown_group(c):
    t_resize_percent(c)


def nothing(c):
    pass


# edge cases: files

def t_files_bad_inputs(c):
    load(c, 'photo.png')
    load(c, 'photo.jpg')
    names = sorted(os.listdir(os.path.join(out, c)))
    check(names == ['photo.jpg', 'photo.png'], 'wrote ' + ', '.join(names))


def flipped(im):
    return abs(rgb(im) - src[:, ::-1]).max() < 40


def t_files_names_keep(c):
    for name, fmt_name in [('with space.png', 'PNG'), ('unicode åäö ü.png', 'PNG'), ('UPPER.JPG', 'JPEG')]:
        im = load(c, name)
        check(im.format == fmt_name and flipped(im), '%s: %s, not the flipped photo' % (name, im.format))
    names = sorted(os.listdir(os.path.join(out, c)))
    check(len(names) == 3, 'wrote ' + ', '.join(names))


def t_files_names_format(c):
    for name in ['with space.png', 'unicode åäö ü.png', 'UPPER.png', 'noext.png']:
        im = load(c, name)
        check(im.format == 'PNG' and flipped(im), '%s: %s, not the flipped photo' % (name, im.format))


def t_files_dup_hierarchy(c):
    a = rgb(load(c, os.path.join('a', 'dup.png'))).mean()
    b = rgb(load(c, os.path.join('b', 'dup.png'))).mean()
    check(abs(a - 40) < 2 and abs(b - 200) < 2, 'a/dup.png %.0f, b/dup.png %.0f: expected 40 and 200' % (a, b))


def t_output_missing(c):
    check(flipped(load(c, os.path.join('new', 'deeper', 'photo.png'))), 'not flipped')


def t_overwrite_false(c):
    im = load(c, 'photo.png')
    check(im.size == (1, 1), 'the existing file was replaced')


def t_overwrite_keep_dates(c):
    im = load(c, 'photo.png')
    check(im.size == (640, 480), 'the existing file was not replaced')
    mtime = os.path.getmtime(os.path.join(out, c, 'photo.png'))
    check(abs(mtime - OLD_DATE) < 2, 'date %d, expected %d' % (mtime, OLD_DATE))


# edge cases: pixel formats

TYPES = ['gray', 'gray-alpha', 'indexed', 'rgb16', 'float', 'alpha', 'multilayer']
SIZES = {'multilayer': (200, 100)}


def types_check(ext, fmt_name, scale):
    def f(c):
        bad = []
        for t in TYPES:
            try:
                im = load(c, t + '.' + ext)
                w, h = SIZES.get(t, (640, 480))
                size = (w // scale, h // scale)
                if im.format != fmt_name or im.size != size:
                    bad.append('%s.%s: %s %s, expected %s %s' % (t, ext, im.format, im.size, fmt_name, size))
                elif rgb(im).std() < 1:
                    bad.append('%s.%s is blank' % (t, ext))
            except AssertionError as e:
                bad.append(str(e))
        check(not bad, '; '.join(bad))
    return f


def png_bit_depth(path):
    return open(path, 'rb').read(25)[24]


def t_types_keep(c):
    bad = []
    expected = {'gray.png': ('PNG', 'L'), 'gray-alpha.png': ('PNG', 'LA'), 'indexed.png': ('PNG', 'P'),
                'indexed.gif': ('GIF', 'P'), 'alpha.png': ('PNG', 'RGBA')}
    for name, (fmt_name, mode) in expected.items():
        try:
            im = load(c, name)
            if im.format != fmt_name or (mode and im.mode != mode):
                bad.append('%s: %s %s, expected %s %s' % (name, im.format, im.mode, fmt_name, mode))
        except AssertionError as e:
            bad.append(str(e))
    path = os.path.join(out, c, 'rgb16.png')
    if not os.path.exists(path) or png_bit_depth(path) != 16 or open(path, 'rb').read(26)[25] != 2:
        bad.append('rgb16.png missing or not 16-bit RGB')
    # PIL cannot read float TIFFs
    path = os.path.join(out, c, 'float.tif')
    if not os.path.exists(path) or open(path, 'rb').read(4) != b'II*\0':
        bad.append('float.tif missing or not a TIFF')
    path = os.path.join(out, c, 'multilayer.xcf')
    if not os.path.exists(path) or open(path, 'rb').read(8) != b'gimp xcf':
        bad.append('multilayer.xcf missing or not an XCF')
    check(not bad, '; '.join(bad))


def t_types_padded(c):
    bad = []
    for t in TYPES:
        try:
            im = load(c, t + '.png')
            if im.size != (300, 300):
                bad.append('%s.png: %s' % (t, im.size))
            elif t in ('alpha', 'rgb16', 'multilayer'):
                px = rgb(im)[2, 150]
                if abs(px - [0, 255, 0]).max() > 3:
                    bad.append('%s.png: padding %s, expected green' % (t, px))
        except AssertionError as e:
            bad.append(str(e))
    check(not bad, '; '.join(bad))


def t_types_userdef(c):
    bad = []
    for t in TYPES:
        try:
            im = load(c, t + '.png')
            if t == 'multilayer':
                continue
            orig = Image.open(os.path.join(images, t + ('.tif' if t == 'float' else '.png')))
            if im.size != orig.size:
                bad.append('%s.png: size %s' % (t, im.size))
            # inverted: dark where the source is bright (the left edge is dark in every test image)
            elif rgb(im)[240, 5].mean() < 128:
                bad.append('%s.png not inverted' % t)
        except AssertionError as e:
            bad.append(str(e))
    a = rgb(load(c, 'multilayer.png'))
    if not (a[50, 100] == [0, 255, 255]).all() or not (a[5, 5] == [0, 0, 0]).all():
        bad.append('multilayer.png not inverted: %s %s' % (a[50, 100], a[5, 5]))
    check(not bad, '; '.join(bad))


# edge cases: "Other GIMP procedure..." with arguments of several types

def t_userdef_offset(c):
    a = rgb(load(c, 'photo.png'))
    check((a[:, :50] == [255, 0, 0]).all(), 'not filled with red on the left: %s' % a[240, 10])
    check(abs(a[:, 60:] - src[:, 10:-50]).max() < 3, 'not moved by 50 px')


def t_userdef_threshold(c):
    a = rgb(load(c, 'photo.png'))
    values = np.unique(a)
    check(set(values) <= {0, 255} and len(values) == 2, 'not black and white: %s' % values[:8])
    row = a[100, :, 0]
    # red 0.25 to 0.75 is white; the red gradient runs over the width
    white = np.nonzero(row == 255)[0]
    check(80 < white.min() < 240 and 400 < white.max() < 560, 'white from x=%d to %d' % (white.min(), white.max()))


def t_userdef_flip(c):
    check(abs(rgb(load(c, 'photo.png')) - src[::-1]).max() < 3, 'not flipped vertically')


def t_userdef_chain(c):
    a = rgb(load(c, 'photo.png'))
    check(abs(a[..., 0] - a[..., 2]).max() < 2, 'not gray')
    check(abs(a[:, :, 0] - src[::-1].mean(axis=2)).max() < 3, 'not flipped vertically and desaturated')


def t_userdef_bad_config(c):
    check(abs(rgb(load(c, 'photo.png')) - src).max() < 3, 'changed')


def t_userdef_missing_procedure(c):
    check(abs(rgb(load(c, 'photo.png')) - src).max() < 3, 'changed')


def t_format_tiff_ccitt(c):
    im = load(c, 'photo.tiff')
    check(im.info.get('compression') == 'group3', 'compression ' + str(im.info.get('compression')))


def t_color_curve(c):
    a = rgb(load(c, 'photo.png'))
    # the curve takes 0.5 to 0.25: white stays, the rest gets darker
    check((a[240, 320] > 250).all(), 'white square changed: %s' % a[240, 320])
    check(a.mean() < src.mean() - 20, 'mean %.1f, source %.1f: not darker' % (a.mean(), src.mean()))


def exif_make(path):
    return Image.open(path).getexif().get(0x010F)


def t_metadata_keep(c):
    load(c, 'exif.jpg')
    make = exif_make(os.path.join(out, c, 'exif.jpg'))
    check(make == 'BIMPTEST', 'Exif Make %r, expected BIMPTEST' % make)


def t_metadata_jpeg(c):
    t_metadata_keep(c)


tests = {
    'format-gif': fmt('gif', 'GIF'), 'format-bmp': fmt('bmp', 'BMP'),
    'format-tga': fmt('tga', 'TGA'), 'format-webp': fmt('webp', 'WEBP'),
    'set-truncated': nothing, 'set-invalid-format': nothing, 'set-invalid-crop': nothing,
    'set-empty': nothing, 'set-missing': nothing, 'output-readonly': nothing, 'output-file': nothing,
    'types-jpeg': types_check('jpg', 'JPEG', 2), 'types-gif': types_check('gif', 'GIF', 2),
    'types-png': types_check('png', 'PNG', 2),
    'locale-userdef-threshold': t_userdef_threshold, 'locale-color-curve': t_color_curve,
    'locale-resize-resolution': t_resize_resolution, 'locale-crop-custom': t_crop_custom,
}
for name, fn in list(globals().items()):
    if name.startswith('t_'):
        tests[name[2:].replace('_', '-')] = fn


def expectations(case):
    text = open(os.path.join(here, 'sets', case + '.bimp'), encoding='utf-8').read()
    e = {'status': 'success', 'errors': '0', 'processed': None}
    for line in text.splitlines():
        if line.startswith('# expect:'):
            for item in shlex.split(line.split(':', 1)[1]):
                key, value = item.split('=', 1)
                e[key] = value
    return e


def check_result(case, r):
    e = expectations(case)
    check(r['status'] == e['status'], 'returned %s, expected %s%s' % (
        r['status'], e['status'], (': ' + r['message']) if r.get('message') else ''))
    if r['status'] == 'success':
        processed = int(e['processed']) if e['processed'] is not None else r['inputs']
        check(r['processed'] == processed and r['errors'] == int(e['errors']),
              'processed %d with %d errors, expected %d with %s' % (r['processed'], r['errors'], processed, e['errors']))


results = json.load(open(os.path.join(out, 'results.json'))) if os.path.exists(os.path.join(out, 'results.json')) else {}
sets = sorted(f[:-5] for f in os.listdir(os.path.join(here, 'sets')) if f.endswith('.bimp'))
count = 0
for case in sets:
    if only and not case.startswith(only):
        continue
    count += 1
    try:
        if case not in tests:
            raise AssertionError('no check in tests/check.py')
        if case not in results:
            raise AssertionError('not run')
        check_result(case, results[case])
        tests[case](case)
        print('PASS', case)
    except AssertionError as e:
        failures.append(case)
        print('FAIL', case, '-', e)

# the plug-in must not crash, and a sanitizer build must not report anything
log = open(os.path.join(out, 'gimp.log'), errors='replace').read() if os.path.exists(os.path.join(out, 'gimp.log')) else ''
count += 1
crashes = [line for line in log.splitlines() if 'Plug-in crashed' in line or 'fatal error' in line]
if crashes:
    failures.append('no-crash')
    print('FAIL no-crash -', crashes[0])
else:
    print('PASS no-crash')
# a GIMP call BIMP makes must not fail unseen
count += 1
errors = [line for line in log.splitlines() if 'GIMP-Error' in line or 'CRITICAL' in line]
if errors:
    failures.append('no-gimp-errors')
    print('FAIL no-gimp-errors -', errors[0], '(%d in gimp.log)' % len(errors))
else:
    print('PASS no-gimp-errors')
reports = sorted(glob.glob(os.path.join(out, 'sanitizer.*')))
if os.environ.get('BIMP_SANITIZE'):
    count += 1
    if reports:
        failures.append('sanitizer')
        print('FAIL sanitizer - see', ', '.join(reports))
    else:
        print('PASS sanitizer')

print('%d failed of %d' % (len(failures), count))
sys.exit(1 if failures else 0)
