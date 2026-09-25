# Runs inside GIMP (tests/gui/start.sh): opens the BIMP window, as the menu
# entry does. Blocks until the window is closed.
# With BIMP_OPEN_IMAGE set, that image is opened in GIMP first (to test
# "Add all opened images").
import os

import gi
gi.require_version('Gimp', '3.0')
from gi.repository import Gimp, Gio

if os.environ.get('BIMP_OPEN_IMAGE'):
    image = Gimp.file_load(Gimp.RunMode.NONINTERACTIVE, Gio.File.new_for_path(os.environ['BIMP_OPEN_IMAGE']))
    Gimp.Display.new(image)
proc = Gimp.get_pdb().lookup_procedure('plug-in-bimp')
config = proc.create_config()
config.set_property('run-mode', Gimp.RunMode.INTERACTIVE)
proc.run(config)
