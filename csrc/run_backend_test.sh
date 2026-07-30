#!/usr/bin/env bash
# Backend smoke test. Runs HEADLESS via SDL's dummy video driver, so it works in
# CI and over ssh - a GUI test that needs a display is a test nobody runs.
#
# It asserts return values and invalid-handle behaviour rather than appearance.
# What it cannot check is whether anything looked right: a text renderer happily
# passes a width test while drawing mojibake. Eyeball examples/ for that.
set -uo pipefail
cd "$(dirname "$0")/.."

if ! pkg-config --exists sdl3; then
    echo "SKIP: SDL3 not installed"; exit 0
fi

CC="${CC:-cc}"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT

$CC -Wall -Wextra -std=c11 -I src \
    $(pkg-config --cflags sdl3) \
    csrc/test_backend.c src/gui_sdl3.c \
    $(pkg-config --libs sdl3) \
    -o "$TMP/test_backend" || { echo "FAIL: backend test did not compile"; exit 1; }

SDL_VIDEODRIVER=dummy "$TMP/test_backend"
