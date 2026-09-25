# Runs inside GIMP (tests/run.sh): applies each set in tests/sets to the test
# images with plug-in-bimp-batch, one output folder per set.
import os

import gi
gi.require_version('Gimp', '3.0')
from gi.repository import Gimp, Gio

here = os.environ['BIMP_TESTS']
out = os.environ['BIMP_OUT']
only = os.environ.get('BIMP_ONLY', '')
pdb = Gimp.get_pdb()
proc = pdb.lookup_procedure('plug-in-bimp-batch')

for name in sorted(os.listdir(os.path.join(here, 'sets'))):
    if not name.endswith('.bimp') or (only and not name.startswith(only)):
        continue
    case = name[:-5]
    folder = os.path.join(out, case)
    os.makedirs(folder, exist_ok=True)
    text = open(os.path.join(here, 'sets', name)).read()
    # sets may name files of the test folder as @IMAGES@
    set_file = os.path.join(out, name)
    open(set_file, 'w').write(text.replace('@IMAGES@', os.path.join(out, 'images')))
    inputs = ['photo.png']
    for line in text.splitlines():
        if line.startswith('# inputs:'):
            inputs = line.split(':', 1)[1].split()
    config = proc.create_config()
    config.set_property('set-file', Gio.File.new_for_path(set_file))
    config.set_property('files', [os.path.join(out, 'images', f) for f in inputs])
    config.set_property('output-folder', Gio.File.new_for_path(folder))
    config.set_property('overwrite', True)
    config.set_property('keep-folder-hierarchy', 'hierarchy' in case)
    result = proc.run(config)
    print('CASE', case, result.index(0), 'processed', result.index(1), 'errors', result.index(2))
