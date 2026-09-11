#!/usr/bin/env bash
set -euo pipefail

REPO="https://github.com/syoyo/tinyvdb.git"
REF="main"
DEST="$(cd "$(dirname "$0")/.." && pwd)/third_party/tinyvdb"

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

git clone --depth 1 --branch "$REF" "$REPO" "$TMP/tinyvdb"
SHA=$(git -C "$TMP/tinyvdb" rev-parse HEAD)
SRC="$TMP/tinyvdb/src"

rm -rf "$DEST"
mkdir -p "$DEST"

# Whole src/ tree, subdirectories included: upstream's own vendored
# third_party/nanovdb is reached by a src/-relative include path.
cp -R "$SRC"/. "$DEST/"

# miniz is already built as its own target from third_party/tinyexr. A second
# copy here would define mz_* twice at link.
rm -f "$DEST/miniz.c" "$DEST/miniz.h"

# GLSL compute sources are compiled to SPIR-V by upstream's build via
# glslangValidator and embedded as .inc files. Nothing here runs that step and
# nothing in the .vdb write path reaches the GPU backend.
find "$DEST" -type f \( -name '*.comp' -o -name '*.glsl' -o -name '*.spv' \) -delete

# Upstream fixes applied to the freshly vendored tree. Each one verifies its
# target is still present and fails the run if not, so an upstream bump that
# changes or fixes the code is caught rather than silently skipped.
# Apache-2.0 requires a record of modification, so touched files are stamped
# and listed in VENDOR.
PATCHED=""

fixup_file() {
    local file="$1"
    local reason="$2"
    shift 2

    if [ ! -f "$DEST/$file" ]; then
        echo "fixup target missing: $file ($reason)" >&2
        exit 1
    fi

    "$@" || exit 1

    case " $PATCHED " in
    *" $file "*) ;;
    *) PATCHED="$PATCHED $file" ;;
    esac
}

# tinyvdb_sparse_tree.c declares a function pointer named get_data purely to
# silence an unused warning, then never assigns it. GCC and Clang fold the
# dead reference away; MSVC keeps it and fails the DLL link with an
# unresolved external.
drop_dead_get_data() {
    local f="$DEST/tinyvdb_sparse_tree.c"

    if ! grep -q 'const float \*(get_data)(const leaf_collect_t' "$f"; then
        echo "fixup no longer applies: dead get_data declaration not found" >&2
        echo "  upstream may have fixed it; drop drop_dead_get_data from this script" >&2
        return 1
    fi

    grep -v -e 'const float \*(get_data)(const leaf_collect_t' \
        -e '(void)get_data;' "$f" >"$f.tmp"
    mv "$f.tmp" "$f"
}

fixup_file tinyvdb_sparse_tree.c "dead get_data declaration" drop_dead_get_data

for f in $PATCHED; do
    stamp="/* Modified by the MayaFlux project, $(date -u +%Y-%m-%d). Upstream baseline: $SHA. See scripts/vender_tinyvdb.sh. */"
    printf '%s\n' "$stamp" | cat - "$DEST/$f" >"$DEST/$f.tmp"
    mv "$DEST/$f.tmp" "$DEST/$f"
    echo "  fixup: $f"
done

cp "$TMP/tinyvdb/LICENSE" "$DEST/"
if [ -f "$TMP/tinyvdb/NOTICE" ]; then
    cp "$TMP/tinyvdb/NOTICE" "$DEST/"
fi

# Header-carrying implementations need a translation unit each. Generate one
# per TINYVDB_*_IMPLEMENTATION guard found, skipping any header that already
# ships a matching source upstream.
for h in "$DEST"/*.h; do
    base="$(basename "$h" .h)"
    if [ -f "$DEST/$base.c" ] || [ -f "$DEST/$base.cc" ]; then
        continue
    fi
    guard="$(grep -oE 'TINYVDB_[A-Z0-9_]*_IMPLEMENTATION' "$h" | head -n 1 || true)"
    if [ -n "$guard" ]; then
        printf '#define %s\n#include "%s.h"\n' "$guard" "$base" >"$DEST/$base.c"
        echo "  shim: $base.c (#define $guard)"
    fi
done

# Apache-2.0 requires the LICENSE, the NOTICE if present, and a record of any
# modification. Fixups above are the only modifications; each stamps its file
# and is listed here.
{
    echo "name:     tinyvdb"
    echo "repo:     $REPO"
    echo "ref:      $REF"
    echo "commit:   $SHA"
    echo "license:  Apache-2.0"
    echo "vendored: $(date -u +%Y-%m-%d)"
    echo "excluded: miniz (built from third_party/tinyexr)"
    if [ -n "$PATCHED" ]; then
        echo "patched:  $(printf '%s' "$PATCHED" | tr '\n' ' ')"
    fi
} >"$DEST/VENDOR"

echo "Vendored tinyvdb ($REF @ ${SHA:0:8}) -> $DEST"
