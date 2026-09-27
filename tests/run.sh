#!/bin/sh
# Tests BIMP inside the Flatpak GIMP, without a window:
#  - builds BIMP into tests/output/build (with gimp-devtools/gimp-build.sh)
#    and runs the unit tests of the .bimp format (meson test);
#  - runs GIMP with a profile of its own, tests/output/profile
#    (GIMP3_DIRECTORY), which has only this build of BIMP in its plug-in
#    folder: an installed BIMP and the user's GIMP settings are neither used
#    nor changed, and a GIMP that is open does not matter; the build and
#    GIMP run isolated from the user's folders (tests/isolate.sh, with
#    gimp-devtools/gimp-run.sh): HOME and the XDG folders inside
#    the Flatpak point into tests/output/gimp-home, so nothing lands in
#    ~/.var/app/org.gimp.GIMP either;
#  - applies each set in tests/sets to test images with plug-in-bimp-batch
#    (tests/batch.py), once more for the sets that ask for a locale with a
#    decimal comma, and checks the results (tests/check.py).
#
#   tests/run.sh           everything
#   tests/run.sh resize    only sets whose name starts with "resize"
#   BIMP_SANITIZE=1 tests/run.sh
#                          the same with BIMP built with AddressSanitizer
#                          and UndefinedBehaviorSanitizer (in
#                          tests/output/build-sanitize, own profile); any
#                          report fails the run
#
# The first run with a new profile takes about a minute longer: GIMP
# queries all of its plug-ins once. Before and after, it lists the user's
# folders of GIMP and the other apps (gimp-devtools/snapshot.sh)
# and fails if anything there changed. Prints PASS or FAIL per test; the
# exit status is 1 if any failed.
set -e
here=$(cd "$(dirname "$0")" && pwd)
src=$(dirname "$here")
out="$here/output"
devtools=${GIMP_DEVTOOLS:-$(dirname "$src")/gimp-devtools}
if [ ! -x "$devtools/gimp-build.sh" ]; then
    echo "needs gimp-devtools next to this folder, or GIMP_DEVTOOLS=<its folder>" >&2
    exit 2
fi
GIMP_RUN_HOME=$out/gimp-home
export GIMP_RUN_HOME
# shellcheck source=SCRIPTDIR/isolate.sh
. "$here/isolate.sh"
mkdir -p "$out"
snapshot_take "$out/snapshot-before.txt"

if [ -n "$BIMP_SANITIZE" ]; then
    build="$out/build-sanitize"
    profile="$out/profile-sanitize"
    setup_args="-Db_sanitize=address,undefined -Db_lundef=false"
else
    build="$out/build"
    profile="$out/profile"
    setup_args=""
fi
run="$out/run"
failed=0

mkdir -p "$out"
if [ ! -f "$build/build.ninja" ]; then
    "$devtools/gimp-build.sh" "$src" meson setup "$build" $setup_args -Dplugindir="$profile/plug-ins" > "$out/setup.log" 2>&1 ||
        { cat "$out/setup.log"; exit 2; }
fi
"$devtools/gimp-build.sh" "$src" ninja -C "$build" install > "$out/build.log" 2>&1 ||
    { cat "$out/build.log"; exit 2; }

# no leak check: BIMP's manipulations live as long as the plug-in and are
# never freed
if "$devtools/gimp-build.sh" "$src" "ASAN_OPTIONS=detect_leaks=0 meson test -C '$build' --print-errorlogs" > "$out/unit.log" 2>&1; then
    echo "PASS unit tests (meson test)"
else
    grep -E "^(not ok|ok|Bail)|ERROR|Sanitizer" "$out/unit.log" || tail -20 "$out/unit.log"
    echo "FAIL unit tests (meson test), see $out/unit.log"
    failed=1
fi

rm -rf "$run"
mkdir -p "$run/images"
python3 "$here/make-images.py" "$run/images" > /dev/null

# sanitizer reports go to files, which tests/check.py looks for
san_env=""
if [ -n "$BIMP_SANITIZE" ]; then
    san_env="--env=ASAN_OPTIONS=detect_leaks=0:log_path=$run/sanitizer.asan --env=UBSAN_OPTIONS=print_stacktrace=1:log_path=$run/sanitizer.ubsan"
fi

gimp_pass () {
    # $1: the locale of the pass, or empty
    gimp_run --flatpak --filesystem="$here" --env=GIMP3_DIRECTORY="$profile" \
      --env=BIMP_TESTS="$here" --env=BIMP_OUT="$run" --env=BIMP_ONLY="$ONLY" \
      --env=BIMP_LOCALE="$1" ${1:+--env=LC_ALL=$1} $san_env \
      -- gimp-console-3.2 --no-interface --no-data \
      --batch-interpreter python-fu-eval \
      -b "exec(open('$here/batch.py').read())" --quit >> "$run/gimp.log" 2>&1 || true
}

ONLY="${1:-}"
gimp_pass ""
if grep -l "^# locale: en_DK.UTF-8" "$here"/sets/"$ONLY"*.bimp > /dev/null 2>&1; then
    gimp_pass en_DK.UTF-8
fi
grep -E "^CASE |BIMP:|Plug-in crashed|fatal error" "$run/gimp.log" || true

BIMP_SANITIZE="$BIMP_SANITIZE" python3 "$here/check.py" "$run" $ONLY || failed=1
snapshot_check "$out/snapshot-before.txt" "" || failed=1
exit $failed
