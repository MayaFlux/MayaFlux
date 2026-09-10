#!/usr/bin/env bash
set -euo pipefail

REPO="https://github.com/syoyo/tinyvdb.git"
REF="main"
DEST="$(cd "$(dirname "$0")/.." && pwd)/third_party/tinyvdb"

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

git clone --depth 1 --branch "$REF" "$REPO" "$TMP/tinyvdb"
SHA=$(git -C "$TMP/tinyvdb" rev-parse HEAD)

rm -rf "$DEST"
mkdir -p "$DEST"

cp "$TMP/tinyvdb/src/tinyvdb_io.h" "$DEST/"
cp "$TMP/tinyvdb/LICENSE" "$DEST/"
if [ -f "$TMP/tinyvdb/NOTICE" ]; then
    cp "$TMP/tinyvdb/NOTICE" "$DEST/"
fi

# Apache-2.0 requires the NOTICE and a record of modification. Keep the
# vendored header pristine; if a patch ever becomes necessary, add it to
# patches/tinyvdb/ and stamp the file here rather than editing in place.
printf '#define TINYVDB_IO_IMPLEMENTATION\n#include "tinyvdb_io.h"\n' >"$DEST/tinyvdb_io.c"

{
    echo "name:     tinyvdb"
    echo "repo:     $REPO"
    echo "ref:      $REF"
    echo "commit:   $SHA"
    echo "license:  Apache-2.0"
    echo "vendored: $(date -u +%Y-%m-%d)"
} >"$DEST/VENDOR"

echo "Vendored tinyvdb ($REF @ ${SHA:0:8}) -> $DEST"
