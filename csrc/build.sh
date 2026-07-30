#!/usr/bin/env bash
# Build the native backend into a static archive for Wyn's [ffi] to link.
#
# A static archive rather than loose objects, because Wyn's [ffi] table accepts
# only `libs`, `lib_dirs` and `include_dirs` - there is no `objects` key.
set -euo pipefail
cd "$(dirname "$0")/.."

CC="${CC:-cc}"
OUT="libgui"
mkdir -p "$OUT"

if ! pkg-config --exists sdl3; then
    echo "gui: SDL3 not found." >&2
    echo "  macOS:  brew install sdl3" >&2
    echo "  Debian: apt install libsdl3-dev" >&2
    exit 1
fi
echo "gui: SDL3 $(pkg-config --modversion sdl3)"

# -Wall -Wextra with no exclusions: the backend must stay warning-clean.
# stb_truetype's own -Wunused-function noise is suppressed at its include site
# in gui_sdl3.c, not by weakening the flags here.
$CC -c -Wall -Wextra -std=c11 -O2 \
    $(pkg-config --cflags sdl3) \
    -I src \
    src/gui_sdl3.c -o "$OUT/gui_sdl3.o"

ar rcs "$OUT/libgui.a" "$OUT/gui_sdl3.o"
printf 'built %s (%8d bytes)\n' "$OUT/libgui.a" "$(wc -c < "$OUT/libgui.a")"

echo
echo "Link a Wyn program against it with, in wyn.toml:"
echo "  [ffi]"
echo "  libs        = [\"gui\", \"SDL3\"]"
echo "  lib_dirs    = [\"libgui\", \"$(pkg-config --variable=libdir sdl3)\"]"
echo "  include_dirs = [\"src\"]"
