#!/usr/bin/env bash
set -euo pipefail

REPO="https://github.com/dmlc/dlpack.git"
TAG="v1.4"
DEST="$(cd "$(dirname "$0")/.." && pwd)/third_party/dlpack"

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

git clone --depth 1 --branch "$TAG" "$REPO" "$TMP/dlpack"
SHA=$(git -C "$TMP/dlpack" rev-parse HEAD)

rm -rf "$DEST"
mkdir -p "$DEST"

cp "$TMP/dlpack/include/dlpack/dlpack.h" "$DEST/"
cp "$TMP/dlpack/LICENSE" "$DEST/"
if [ -f "$TMP/dlpack/NOTICE" ]; then
    cp "$TMP/dlpack/NOTICE" "$DEST/"
fi

{
    echo "name:     dlpack"
    echo "repo:     $REPO"
    echo "ref:      $TAG"
    echo "commit:   $SHA"
    echo "license:  Apache-2.0"
    echo "vendored: $(date -u +%Y-%m-%d)"
} >"$DEST/VENDOR"

echo "Vendored dlpack ($TAG @ ${SHA:0:8}) -> $DEST"
