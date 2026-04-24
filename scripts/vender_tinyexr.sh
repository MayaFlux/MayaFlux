#!/usr/bin/env bash
set -euo pipefail

REPO="https://github.com/syoyo/tinyexr.git"
BRANCH="release"
DEST="$(cd "$(dirname "$0")/.." && pwd)/third_party/tinyexr"

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

git clone --depth 1 --branch "$BRANCH" "$REPO" "$TMP/tinyexr"

rm -rf "$DEST"
mkdir -p "$DEST"

# Flat layout matching system install conventions (mirrors AUR package)
cp "$TMP/tinyexr/tinyexr.h"          "$DEST/"
cp "$TMP/tinyexr/exr_reader.hh"      "$DEST/"
cp "$TMP/tinyexr/streamreader.hh"    "$DEST/"
cp "$TMP/tinyexr/deps/miniz/miniz.h" "$DEST/"
cp "$TMP/tinyexr/deps/miniz/miniz.c" "$DEST/"
cp "$TMP/tinyexr/LICENSE"            "$DEST/"

echo "Vendored tinyexr ($BRANCH) -> $DEST"
