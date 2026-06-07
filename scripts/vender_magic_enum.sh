#!/usr/bin/env bash
set -euo pipefail

REPO="https://github.com/Neargye/magic_enum.git"
TAG="v0.9.8"
DEST="$(cd "$(dirname "$0")/.." && pwd)/third_party/magic_enum"

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

git clone --depth 1 --branch "$TAG" "$REPO" "$TMP/magic_enum"

rm -rf "$DEST"
mkdir -p "$DEST"

cp "$TMP/magic_enum/include/magic_enum/magic_enum.hpp" "$DEST/"
cp "$TMP/magic_enum/include/magic_enum/magic_enum_all.hpp" "$DEST/"
cp "$TMP/magic_enum/include/magic_enum/magic_enum_containers.hpp" "$DEST/"
cp "$TMP/magic_enum/include/magic_enum/magic_enum_flags.hpp" "$DEST/"
cp "$TMP/magic_enum/include/magic_enum/magic_enum_format.hpp" "$DEST/"
cp "$TMP/magic_enum/include/magic_enum/magic_enum_fuse.hpp" "$DEST/"
cp "$TMP/magic_enum/include/magic_enum/magic_enum_iostream.hpp" "$DEST/"
cp "$TMP/magic_enum/include/magic_enum/magic_enum_switch.hpp" "$DEST/"
cp "$TMP/magic_enum/include/magic_enum/magic_enum_utility.hpp" "$DEST/"
cp "$TMP/magic_enum/LICENSE" "$DEST/"

echo "Vendored magic_enum ($TAG) -> $DEST"
