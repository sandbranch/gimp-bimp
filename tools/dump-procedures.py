# Prints the arguments of the GIMP procedures BIMP calls, from the running
# GIMP's PDB: name, type, default and range. Run with tools/dump-procedures.sh.
import gi
gi.require_version('Gimp', '3.0')
from gi.repository import Gimp, GObject

pdb = Gimp.get_pdb()
names = [n for n in pdb.query_procedures('file-.*-export', '', '', '', '', '', '', '')]
names += ['plug-in-sharpen', 'plug-in-gauss', 'plug-in-unsharp-mask']
for name in sorted(set(names)):
    proc = pdb.lookup_procedure(name)
    if proc is None:
        print(f'== {name}: not found')
        continue
    print(f'== {name}  ext={proc.get_extensions() if hasattr(proc, "get_extensions") else ""}')
    for spec in proc.get_arguments():
        extra = ''
        if hasattr(spec, 'minimum'):
            extra = f' [{spec.minimum}..{spec.maximum}]'
        default = ''
        try:
            default = spec.get_default_value()
        except Exception:
            pass
        if GObject.type_name(spec.value_type) == 'gchararray':
            try:
                choice = Gimp.param_spec_choice_get_choice(spec)
                extra = ' choices=' + ','.join(choice.list_nicks())
            except Exception:
                pass
        print(f'   {spec.name:28s} {GObject.type_name(spec.value_type):22s} default={default}{extra}')
