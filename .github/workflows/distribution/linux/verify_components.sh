#!/bin/bash

echo "========================================"
echo "  MayaFlux Linux Distribution Verify"
echo "========================================"
echo ""

ERROR_LEVEL=0

echo "[1] Checking MayaFluxLib (Core Engine)..."
if find include/MayaFlux -name "*.hpp" | head -1 >/dev/null 2>&1; then
    HEADER_COUNT=$(find include/MayaFlux -name "*.hpp" | wc -l | tr -d ' ')
    echo "   ✅ Headers: $HEADER_COUNT files found"
else
    echo "   ❌ ERROR: No MayaFlux headers found"
    ERROR_LEVEL=1
fi

if find lib -name "*MayaFluxLib*" | head -1 >/dev/null 2>&1; then
    LIB_FILE=$(find lib -name "*MayaFluxLib*" | head -1)
    echo "   ✅ Library: $(basename $LIB_FILE)"
else
    echo "   ❌ ERROR: No MayaFluxLib library found"
    ERROR_LEVEL=1
fi

echo ""
echo "[2] Checking Lila (JIT Interface)..."
if find include/Lila -name "*.hpp" | head -1 >/dev/null 2>&1; then
    HEADER_COUNT=$(find include/Lila -name "*.hpp" | wc -l | tr -d ' ')
    echo "   ✅ Headers: $HEADER_COUNT files found"
else
    echo "   ❌ ERROR: No Lila headers found"
    ERROR_LEVEL=1
fi

if find lib -name "*Lila*" | head -1 >/dev/null 2>&1; then
    LIB_FILE=$(find lib -name "*Lila*" | head -1)
    echo "   ✅ Library: $(basename $LIB_FILE)"
else
    echo "   ❌ ERROR: No Lila library found"
    ERROR_LEVEL=1
fi

echo ""
echo "[3] Checking lila_server (Application)..."
if find bin -name "*lila_server*" | head -1 >/dev/null 2>&1; then
    EXE_FILE=$(find bin -name "*lila_server*" | head -1)
    if [ -x "$EXE_FILE" ]; then
        FILE_SIZE=$(stat -c%s "$EXE_FILE" 2>/dev/null)
        SIZE_MB=$((FILE_SIZE / 1024 / 1024))
        echo "   ✅ Executable: $(basename $EXE_FILE) (${SIZE_MB} MB)"
    else
        echo "   ⚠️  WARNING: lila_server not executable (run: chmod +x $EXE_FILE)"
    fi
else
    echo "   ❌ ERROR: No lila_server executable found"
    ERROR_LEVEL=1
fi

echo ""
echo "[4] Checking Runtime (JIT Context)..."
if find share/MayaFlux/runtime -name "*.h" | head -1 >/dev/null 2>&1; then
    RUNTIME_COUNT=$(find share/MayaFlux/runtime -type f | wc -l | tr -d ' ')
    echo "   ✅ Runtime files: $RUNTIME_COUNT files found"
else
    echo "   ❌ ERROR: No runtime files found"
    ERROR_LEVEL=1
fi

echo ""
echo "[5] Checking System Compatibility..."

# Check GCC version
if command -v gcc >/dev/null 2>&1; then
    GCC_VERSION=$(gcc -dumpversion | cut -d. -f1)
    if [ "$GCC_VERSION" -ge 15 ]; then
        echo "   ✅ GCC: version $(gcc -dumpversion) (compatible)"
    else
        echo "   ⚠️  WARNING: GCC version $(gcc -dumpversion) may be incompatible (need 15+)"
        echo "      Consider building from source for your distribution"
    fi
else
    echo "   ⚠️  WARNING: GCC not found"
fi

# Check LLVM
if command -v llvm-config >/dev/null 2>&1; then
    LLVM_VERSION=$(llvm-config --version)
    echo "   ✅ LLVM: version $LLVM_VERSION"
else
    echo "   ⚠️  WARNING: LLVM not found in PATH"
fi

# Check key runtime libraries
echo ""
echo "[6] Checking Runtime Dependencies..."
MISSING_DEPS=()

for lib in libLLVM libglfw libvulkan libavcodec; do
    if ldconfig -p 2>/dev/null | grep -q "$lib"; then
        echo "   ✅ $lib found"
    else
        echo "   ❌ $lib not found"
        MISSING_DEPS+=("$lib")
    fi
done

if [ ${#MISSING_DEPS[@]} -gt 0 ]; then
    echo ""
    echo "   Missing dependencies detected. Install with:"
    echo "   sudo dnf install -y llvm-libs glfw vulkan-loader ffmpeg-free-libs"
    ERROR_LEVEL=1
fi

echo ""
echo "========================================"
if [ $ERROR_LEVEL -ne 0 ]; then
    echo "❌ VERIFICATION FAILED"
    exit 1
else
    echo "✅ VERIFICATION SUCCESSFUL"
    echo ""
    echo "MayaFlux is ready to use!"
    echo "Run: ./bin/lila_server"
fi
echo "========================================"
