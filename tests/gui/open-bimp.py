# Runs inside GIMP (tests/gui/start.sh): opens the BIMP window, as the menu
# entry does. Blocks until the window is closed.
import gi
gi.require_version('Gimp', '3.0')
from gi.repository import Gimp
proc = Gimp.get_pdb().lookup_procedure('plug-in-bimp')
config = proc.create_config()
config.set_property('run-mode', Gimp.RunMode.INTERACTIVE)
proc.run(config)
