#!/usr/bin/env bash
set -euo pipefail

# ---------------------------------------------
# Extract version from CMakeLists.txt
# ---------------------------------------------
VERSION=$(grep -Po '(?<=VERSION )\d+\.\d+\.\d+' CMakeLists.txt)
if [[ -z "$VERSION" ]]; then
    echo "ERROR: Could not extract version from CMakeLists.txt"
    exit 1
fi

# ---------------------------------------------
# Detect current git branch or tag
# ---------------------------------------------
BRANCH=$(git rev-parse --abbrev-ref HEAD)

# If in detached HEAD (e.g., building from tag), detect tag
if [[ "$BRANCH" == "HEAD" ]]; then
    TAG=$(git describe --tags --exact-match 2>/dev/null || true)
    if [[ -n "$TAG" ]]; then
        BRANCH="$TAG"
    fi
fi

# ---------------------------------------------
# Append -dev for non-main branches
# ---------------------------------------------
if [[ "$BRANCH" == "main" || "$BRANCH" == v* ]]; then
    # main or a tagged release -> use VERSION as-is
    FINAL_VERSION="$VERSION"
else
    FINAL_VERSION="${VERSION}-dev"
fi

echo "[INFO] MayaFlux packaging version: $FINAL_VERSION"

# ---------------------------------------------
# Directories
# ---------------------------------------------
PREFIX="packaging/arch_linux"
BUILD_DIR="${PREFIX}/build"
INSTALL_DIR="${PREFIX}/install_root"
ARCHIVE_ROOT="${PREFIX}/MayaFlux-${FINAL_VERSION}"
ARCHIVE_NAME="MayaFlux-${FINAL_VERSION}-linux-arch.tar.gz"

# ---------------------------------------------
# Clean + prepare
# ---------------------------------------------
rm -rf "$BUILD_DIR" "$INSTALL_DIR" "$ARCHIVE_ROOT"
mkdir -p "$BUILD_DIR" "$INSTALL_DIR"

# ---------------------------------------------
# Configure & build
# ---------------------------------------------
cmake -S . -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR"

cmake --build "$BUILD_DIR" --parallel

# ---------------------------------------------
# Install to install_root
# ---------------------------------------------
cmake --install "$BUILD_DIR"

# Sanity check
if [[ ! -d "$INSTALL_DIR/bin" ]]; then
    echo "ERROR: install root missing bin/"
    exit 1
fi

# ---------------------------------------------
# Prepare archive content
# ---------------------------------------------
mkdir -p "$ARCHIVE_ROOT"
cp -r "$INSTALL_DIR"/* "$ARCHIVE_ROOT"/
cp LICENSE "$ARCHIVE_ROOT" 2>/dev/null || true
cp README.md "$ARCHIVE_ROOT" 2>/dev/null || true

# ---------------------------------------------
# Create tarball
# ---------------------------------------------
tar -czf "${PREFIX}/${ARCHIVE_NAME}" -C "$PREFIX" "MayaFlux-${FINAL_VERSION}"

echo "[DONE] Created ${PREFIX}/${ARCHIVE_NAME}"

# ---------------------------------------------
# Generate SHA256 checksum file
# ---------------------------------------------
echo "[INFO] Generating SHA256 checksum..."
sha256sum "${PREFIX}/${ARCHIVE_NAME}" | cut -d' ' -f1 >"${PREFIX}/${ARCHIVE_NAME}.sha256"
echo "[DONE] SHA256: $(cat "${PREFIX}/${ARCHIVE_NAME}.sha256")"

# ---------------------------------------------
# OPTIONAL: GPG signing
# ---------------------------------------------
if [[ "${SIGN:-0}" == "1" ]]; then
    echo "[INFO] Signing archive using GPG…"
    gpg --armor --detach-sign "${PREFIX}/${ARCHIVE_NAME}"
    echo "[DONE] Signature created: ${PREFIX}/${ARCHIVE_NAME}.asc"
fi

# ---------------------------------------------
# Upload to GitHub (if using gh CLI)
# ---------------------------------------------
if [[ "${UPLOAD:-0}" == "1" ]]; then
    echo "[INFO] Uploading to GitHub releases..."
    gh release upload "v${FINAL_VERSION}" \
        "${PREFIX}/${ARCHIVE_NAME}" \
        "${PREFIX}/${ARCHIVE_NAME}.sha256" \
        "${PREFIX}/${ARCHIVE_NAME}.asc" \
        --clobber
    echo "[DONE] Files uploaded to GitHub release v${FINAL_VERSION}"
fi
