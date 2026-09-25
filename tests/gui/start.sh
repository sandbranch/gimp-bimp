#!/bin/sh
# Opens the BIMP window in the Flatpak GIMP on a Broadway display, so it can
# be looked at and clicked with gimp-plugin-devtools/gui/cdp.mjs:
#   tests/gui/start.sh &           # BIMP window on http://127.0.0.1:8085/
#   google-chrome --headless=new --remote-debugging-port=9333 \
#     --user-data-dir=/tmp/cdp-chrome about:blank &
#   node ../gimp-plugin-devtools/gui/cdp.mjs size:1400,900 \
#     nav:http://127.0.0.1:8085/ wait:5000 shot:bimp.png
# BIMP_OPEN_IMAGE=<file> opens that image in GIMP first. Close GIMP first.
here=$(cd "$(dirname "$0")" && pwd)
tests=$(dirname "$here")
flatpak run --filesystem="$tests" --env=GDK_BACKEND=broadway --env=BROADWAY_DISPLAY=:5 \
  --env=BIMP_OPEN_IMAGE="${BIMP_OPEN_IMAGE:-}" \
  --command=sh org.gimp.GIMP -c \
  "broadwayd --port 8085 :5 & sleep 2; gimp-3.2 --no-splash \
   --batch-interpreter python-fu-eval -b \"exec(open('$here/open-bimp.py').read())\""
