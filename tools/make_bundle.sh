#!/usr/bin/env bash
# =============================================================================
# make_bundle.sh — build a self-contained Linux release bundle
#
# Usage: tools/make_bundle.sh <build_dir> [version]
#   version defaults to the CMake project version (CMAKE_PROJECT_VERSION).
#
# Produces dist/FUMBBL-Reborn-<version>-linux-x86_64/ with:
#   FUMBBL-Reborn   the executable (RUNPATH=$ORIGIN, Qt libs resolve next to it)
#   assets/         game assets (faces, stadium, dice, ...)
#   libQt6*.so.6    the Qt libraries the executable links (from ldd)
#   plugins/        platforms/libqxcb.so, imageformats/, styles/
# and dist/FUMBBL-Reborn-<version>-linux-x86_64.tar.gz
#
# Qt prefix: $QT_PREFIX if set, otherwise derived from the CMake cache
# (Qt6_DIR). The target machine needs a recent desktop Linux (glibc >=
# the build host's) — for broader reach, build on an older base (e.g.
# Ubuntu 22.04) or ship an AppImage.
# =============================================================================
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:?usage: make_bundle.sh <build_dir> [version]}"
VERSION="${2:-$(grep -E '^CMAKE_PROJECT_VERSION:' "$BUILD_DIR/CMakeCache.txt" | head -1 | cut -d= -f2-)}"
[[ -n "$VERSION" ]] || { echo "error: could not determine the version" >&2; exit 1; }
EXE="FUMBBL-Reborn"

# --- Qt prefix ---------------------------------------------------------------
# Qt6_DIR is <root>/lib/cmake/Qt6 (official installs) or <root>/lib/cmake/Qt6
# (distros); libdir = the directory with libQt6*.so, root = <libdir>/.. .
libdir=""
root=""
if [[ -n "${QT_PREFIX:-}" ]]; then
    libdir="$QT_PREFIX"
    root="$(dirname "$QT_PREFIX")"
else
    qt6_dir="$(grep -E '^Qt6_DIR:' "$BUILD_DIR/CMakeCache.txt" | head -1 | cut -d= -f2-)"
    libdir="$(cd "$(dirname "$(dirname "$qt6_dir")")" && pwd)"   # strip cmake/Qt6
    root="$(dirname "$libdir")"
fi
QT_PLUGINS=""
for cand in "$root/plugins" "$libdir/qt6/plugins" "$root/lib/qt6/plugins"; do
    [[ -d "$cand/platforms" ]] && { QT_PLUGINS="$cand"; break; }
done
if [[ -z "$QT_PLUGINS" || ! -f "$QT_PLUGINS/platforms/libqxcb.so" ]]; then
    echo "error: Qt platforms plugin not found (set QT_PREFIX to the Qt lib dir)" >&2
    exit 1
fi
echo "Qt libs:   $libdir"
echo "Qt plugins: $QT_PLUGINS"

# --- Stage the app (install applies RUNPATH=$ORIGIN + assets) ----------------
STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT
cmake --install "$BUILD_DIR" --prefix "$STAGE"

NAME="$EXE-$VERSION-linux-x86_64"
DIST="$ROOT/dist"
rm -rf "$DIST/$NAME"
mkdir -p "$DIST/$NAME"
cp -a "$STAGE/." "$DIST/$NAME/"
EXE_PATH="$DIST/$NAME/$EXE"

# --- Copy the Qt libraries the executable needs (from ldd) -------------------
copy_qt_libs() {
    # $1 = file to inspect; copies every libQt6*.so it depends on that lives
    # under the Qt prefix (or is resolvable there by name).
    local file="$1" dep base found
    while IFS= read -r dep; do
        [[ -z "$dep" ]] && continue
        base="$(basename "$dep")"
        if [[ "$dep" = /* && -f "$dep" ]]; then
            found="$dep"
        else
            found="$(find "$libdir" -maxdepth 2 -name "$base" -print -quit 2>/dev/null || true)"
        fi
        [[ -z "$found" ]] && { echo "error: missing Qt lib: $base" >&2; exit 1; }
        cp -n "$found" "$DIST/$NAME/"
    done < <(ldd "$file" 2>/dev/null | awk '
        $1 ~ /^libQt6/ {
            if ($3 != "not found" && $3 != "") print $3; else print $1
        }')
}

copy_qt_libs "$EXE_PATH"
# The XCB platform plugin pulls in more Qt libs than the executable does.
if [[ -f "$QT_PLUGINS/platforms/libqxcb.so" ]]; then
    mkdir -p "$DIST/$NAME/plugins/platforms"
    cp -n "$QT_PLUGINS/platforms/libqxcb.so" "$DIST/$NAME/plugins/platforms/"
    copy_qt_libs "$DIST/$NAME/plugins/platforms/libqxcb.so"
fi
# Image formats + widget styles (small; keeps PNG/JPEG loading and the
# default style working without system Qt).
for d in imageformats styles; do
    if [[ -d "$QT_PLUGINS/$d" ]]; then
        cp -rn "$QT_PLUGINS/$d" "$DIST/$NAME/plugins/" 2>/dev/null || true
    fi
done

# --- Sanity: the bundle must be self-contained (no system Qt left) -----------
remaining="$(ldd "$EXE_PATH" 2>/dev/null | awk '
    $1 ~ /^libQt6/ && $3 == "not found" { print $1 }')"
if [[ -n "$remaining" ]]; then
    echo "error: bundle still misses: $remaining" >&2
    exit 1
fi

tar czf "$DIST/$NAME.tar.gz" -C "$DIST" "$NAME"
echo "bundle ready: $DIST/$NAME.tar.gz"
find "$DIST/$NAME" -maxdepth 1 -type f | sort | head -20
