#!/bin/sh
# Tests the installed BIMP inside the Flatpak GIMP, without a window: each
# manipulation set in tests/sets is applied to test images by
# plug-in-bimp-batch, then tests/check.py checks the results.
#   tests/run.sh           all sets
#   tests/run.sh resize    only sets whose name starts with "resize"
# Install BIMP first (README), and close GIMP.
set -e
here=$(cd "$(dirname "$0")" && pwd)
out="$here/output"
rm -rf "$out"
mkdir -p "$out/images"
python3 "$here/make-images.py" "$out/images"

flatpak run --filesystem="$here" --env=BIMP_TESTS="$here" --env=BIMP_OUT="$out" \
  --env=BIMP_ONLY="${1:-}" \
  --command=gimp-console-3.2 org.gimp.GIMP --no-interface --no-data \
  --batch-interpreter python-fu-eval \
  -b "exec(open('$here/batch.py').read())" --quit > "$out/gimp.log" 2>&1 || true
grep -E "^CASE|BIMP:|Error|error" "$out/gimp.log" | grep -v "^$" || true

python3 "$here/check.py" "$out" ${1:-}
