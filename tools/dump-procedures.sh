#!/bin/sh
# Lists the arguments of the export procedures (and filters) of the Flatpak
# GIMP, which BIMP maps its settings onto. Close GIMP first.
here=$(cd "$(dirname "$0")" && pwd)
flatpak run --filesystem="$here" --command=gimp-console-3.2 org.gimp.GIMP \
  --no-interface --no-data --batch-interpreter python-fu-eval \
  -b "exec(open('$here/dump-procedures.py').read())" --quit 2>&1 | grep -E '^(==|   )'
