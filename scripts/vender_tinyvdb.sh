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
# modification. The vendored tree is kept pristine: if a patch ever becomes
# necessary, add it to patches/tinyvdb/ and stamp the touched file here rather
# than editing in place.
{
    echo "name:     tinyvdb"
    echo "repo:     $REPO"
    echo "ref:      $REF"
    echo "commit:   $SHA"
    echo "license:  Apache-2.0"
    echo "vendored: $(date -u +%Y-%m-%d)"
    echo "excluded: miniz (built from third_party/tinyexr)"
} >"$DEST/VENDOR"

echo "Vendored tinyvdb ($REF @ ${SHA:0:8}) -> $DEST"
