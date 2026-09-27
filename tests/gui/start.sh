#!/bin/sh
# Opens the BIMP window in the Flatpak GIMP on a Broadway display, so it can
# be looked at and clicked with gimp-plugin-devtools/gui/cdp.mjs:
#   tests/gui/start.sh &           # BIMP window on http://127.0.0.1:8085/
#   google-chrome --headless=new --remote-debugging-port=9333 \
#     --user-data-dir=/tmp/cdp-chrome about:blank &
#   node ../gimp-plugin-devtools/gui/cdp.mjs size:1400,900 \
#     nav:http://127.0.0.1:8085/ wait:5000 shot:bimp.png
# BIMP_OPEN_IMAGE=<file> opens that image in GIMP first. BIMP is the one
# tests/run.sh built and installed into its throwaway profile
# (tests/output/profile); run that first. GIMP runs isolated from your
# own folders (tests/isolate.sh), with the throwaway home of tests/run.sh,
# so a GIMP of yours does not matter. broadwayd stops when GIMP quits (or
# this script is interrupted).
here=$(cd "$(dirname "$0")" && pwd)
tests=$(dirname "$here")
src=$(dirname "$tests")
[ -d "$tests/output/profile/plug-ins" ] ||
  { echo "run tests/run.sh first: it installs BIMP into tests/output/profile" >&2; exit 1; }
GIMP_RUN_HOME=${GIMP_RUN_HOME:-$tests/output/gimp-home}
# shellcheck source=SCRIPTDIR/../isolate.sh
. "$tests/isolate.sh"
image=${BIMP_OPEN_IMAGE:+$(cd "$(dirname "$BIMP_OPEN_IMAGE")" && pwd)/$(basename "$BIMP_OPEN_IMAGE")}
gimp_run --flatpak --filesystem="$tests" ${image:+--filesystem="$(dirname "$image")":ro} \
  --env=GDK_BACKEND=broadway --env=BROADWAY_DISPLAY=:5 \
  --env=GIMP3_DIRECTORY="$tests/output/profile" --env=BIMP_OPEN_IMAGE="$image" -- sh -c \
  "broadwayd --port 8085 :5 & bw=\$!; trap 'kill \$bw 2>/dev/null' EXIT INT TERM; \
   sleep 2; gimp-3.2 --new-instance --no-splash \
   --batch-interpreter python-fu-eval -b \"exec(open('$here/open-bimp.py').read())\""
