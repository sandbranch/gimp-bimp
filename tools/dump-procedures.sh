#!/bin/sh
# Lists the arguments of the export procedures (and filters) of the Flatpak
# GIMP, which BIMP maps its settings onto. GIMP runs with a throwaway
# profile (tests/output/dump-profile), isolated from your own folders
# (tests/isolate.sh), so a GIMP of yours does not matter.
here=$(cd "$(dirname "$0")" && pwd)
src=$(dirname "$here")
out=$src/tests/output
mkdir -p "$out/dump-profile"
GIMP_RUN_HOME=${GIMP_RUN_HOME:-$out/gimp-home}
# shellcheck source=SCRIPTDIR/../tests/isolate.sh
. "$src/tests/isolate.sh"
gimp_run --flatpak --filesystem="$here" --env=GIMP3_DIRECTORY="$out/dump-profile" -- \
  gimp-console-3.2 --no-interface --no-data --batch-interpreter python-fu-eval \
  -b "exec(open('$here/dump-procedures.py').read())" --quit 2>&1 | grep -E '^(==|   )'
