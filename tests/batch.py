# Runs inside GIMP (tests/run.sh): applies each set in tests/sets to the test
# images with plug-in-bimp-batch, one output folder per set, and writes what
# the procedure returned to results.json for tests/check.py.
#
# Comments at the top of a set say how to run it:
#   # inputs: a.png "with space.png"   input files, in the images folder
#                                      (default photo.png)
#   # overwrite: false                 (default true)
#   # keep-dates: true                 (default false)
#   # keep-folder-hierarchy: true      (default false)
#   # output: some/folder              output folder, below the set's folder
#   # output-state: missing | readonly | file
#                                      the output folder does not exist, is
#                                      read-only, or is a file
#   # existing: photo.png              files already in the output folder:
#                                      1x1 pixel PNGs dated 2001-01-01
#   # set-file: missing                give a set file that does not exist
#   # locale: en_DK.UTF-8              run only in the pass with this locale
#   # expect: ...                      what tests/check.py expects (see there)
# Sets may name the test images folder as @IMAGES@.
import json
import os
import shlex
import stat

import gi
gi.require_version('Gimp', '3.0')
gi.require_version('Gegl', '0.4')
from gi.repository import Gimp, Gegl, Gio

here = os.environ['BIMP_TESTS']
out = os.environ['BIMP_OUT']
only = os.environ.get('BIMP_ONLY', '')
locale = os.environ.get('BIMP_LOCALE', '')
images = os.path.join(out, 'images')
pdb = Gimp.get_pdb()
proc = pdb.lookup_procedure('plug-in-bimp-batch')
results_file = os.path.join(out, 'results.json')
results = json.load(open(results_file)) if os.path.exists(results_file) else {}
OLD_DATE = 978307200  # 2001-01-01


def directives(text):
    d = {}
    for line in text.splitlines():
        if not line.startswith('#') or ':' not in line:
            continue
        key, value = line[1:].split(':', 1)
        d[key.strip()] = value.strip()
    return d


def make_multilayer(path):
    """an XCF with a white background and a red layer smaller than the image"""
    if os.path.exists(path):
        return
    img = Gimp.Image.new(200, 100, Gimp.ImageBaseType.RGB)
    bg = Gimp.Layer.new(img, 'bg', 200, 100, Gimp.ImageType.RGB_IMAGE, 100, Gimp.LayerMode.NORMAL)
    img.insert_layer(bg, None, 0)
    fg = Gimp.Layer.new(img, 'fg', 100, 50, Gimp.ImageType.RGBA_IMAGE, 100, Gimp.LayerMode.NORMAL)
    img.insert_layer(fg, None, 0)
    fg.set_offsets(50, 25)
    Gimp.context_push()
    Gimp.context_set_background(Gegl.Color.new('white'))
    bg.fill(Gimp.FillType.BACKGROUND)
    Gimp.context_set_background(Gegl.Color.new('red'))
    fg.fill(Gimp.FillType.BACKGROUND)
    Gimp.context_pop()
    Gimp.file_save(Gimp.RunMode.NONINTERACTIVE, img, Gio.File.new_for_path(path), None)
    img.delete()


def existing_file(path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    # a 1x1 gray PNG
    open(path, 'wb').write(bytes.fromhex(
        '89504e470d0a1a0a0000000d4948445200000001000000010800000000'
        '3a7e9b550000000a49444154789c63600000000200015f5ae56f0000000049454e44ae426082'))
    os.utime(path, (OLD_DATE, OLD_DATE))


make_multilayer(os.path.join(images, 'multilayer.xcf'))

for name in sorted(os.listdir(os.path.join(here, 'sets'))):
    if not name.endswith('.bimp') or (only and not name.startswith(only)):
        continue
    case = name[:-5]
    text = open(os.path.join(here, 'sets', name), encoding='utf-8').read()
    d = directives(text)
    if d.get('locale', '') != locale:
        continue

    folder = os.path.join(out, case)
    os.makedirs(folder, exist_ok=True)
    set_file = os.path.join(out, name)
    open(set_file, 'w', encoding='utf-8').write(text.replace('@IMAGES@', images))
    if d.get('set-file') == 'missing':
        set_file = os.path.join(out, 'no-such-set.bimp')

    output = os.path.join(folder, d.get('output', ''))
    state = d.get('output-state', '')
    if state == 'missing':
        pass
    elif state == 'file':
        os.makedirs(os.path.dirname(output), exist_ok=True)
        open(output, 'w').write('not a folder')
    else:
        os.makedirs(output, exist_ok=True)
    for existing in shlex.split(d.get('existing', '')):
        existing_file(os.path.join(output, existing))
    if state == 'readonly':
        os.chmod(output, stat.S_IRUSR | stat.S_IXUSR)

    inputs = shlex.split(d.get('inputs', 'photo.png'))
    config = proc.create_config()
    config.set_property('set-file', Gio.File.new_for_path(set_file))
    config.set_property('files', [os.path.join(images, f) for f in inputs])
    config.set_property('output-folder', Gio.File.new_for_path(output))
    config.set_property('overwrite', d.get('overwrite', 'true') == 'true')
    config.set_property('keep-folder-hierarchy', d.get('keep-folder-hierarchy', 'false') == 'true')
    config.set_property('keep-dates', d.get('keep-dates', 'false') == 'true')
    print('CASE-START', case, flush=True)
    result = proc.run(config)

    status = result.index(0).value_nick
    r = {'status': status, 'inputs': len(inputs)}
    if status == 'success':
        r['processed'] = result.index(1)
        r['errors'] = result.index(2)
    elif result.length() > 1:
        r['message'] = str(result.index(1))
    results[case] = r
    print('CASE', case, json.dumps(r), flush=True)

    if state == 'readonly':
        os.chmod(output, stat.S_IRWXU)

json.dump(results, open(results_file, 'w'), indent=1, sort_keys=True)
