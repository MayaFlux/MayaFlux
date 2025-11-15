#!/bin/zsh

echo "========================================"
echo "  MayaFlux macOS Distribution Verify"
echo "========================================"
echo ""

ERROR_LEVEL=0

echo "[1] Checking MayaFluxLib (Core Engine)..."
if find include/MayaFlux -name "*.hpp" | head -1 >/dev/null; then
    HEADER_COUNT=$(find include/MayaFlux -name "*.hpp" | wc -l | tr -d ' ')
    echo "   ✅ Headers: $HEADER_COUNT files found"
else
    echo "   ❌ ERROR: No MayaFlux headers found"
    ERROR_LEVEL=1
fi

if find lib -name "*MayaFluxLib*" | head -1 >/dev/null; then
    LIB_FILE=$(find lib -name "*MayaFluxLib*" | head -1)
    echo "   ✅ Library: $(basename $LIB_FILE)"
else
    echo "   ❌ ERROR: No MayaFluxLib library found"
    ERROR_LEVEL=1
fi

echo ""
echo "[2] Checking Lila (JIT Interface)..."
if find include/Lila -name "*.hpp" | head -1 >/dev/null; then
    HEADER_COUNT=$(find include/Lila -name "*.hpp" | wc -l | tr -d ' ')
    echo "   ✅ Headers: $HEADER_COUNT files found"
else
    echo "   ❌ ERROR: No Lila headers found"
    ERROR_LEVEL=1
fi

if find lib -name "*Lila*" | head -1 >/dev/null; then
    LIB_FILE=$(find lib -name "*Lila*" | head -1)
    echo "   ✅ Library: $(basename $LIB_FILE)"
else
    echo "   ❌ ERROR: No Lila library found"
    ERROR_LEVEL=1
fi

echo ""
echo "[3] Checking lila_server (Application)..."
if find bin -name "*lila_server*" | head -1 >/dev/null; then
    EXE_FILE=$(find bin -name "*lila_server*" | head -1)
    if [ -x "$EXE_FILE" ]; then
        FILE_SIZE=$(stat -f%z "$EXE_FILE" 2>/dev/null || stat -c%s "$EXE_FILE" 2>/dev/null)
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
if find share/lila/runtime -name "*.h" | head -1 >/dev/null; then
    RUNTIME_COUNT=$(find share/lila/runtime -type f | wc -l | tr -d ' ')
    echo "   ✅ Runtime files: $RUNTIME_COUNT files found"
else
    echo "   ❌ ERROR: No runtime files found"
    ERROR_LEVEL=1
fi

echo ""
echo "[5] Checking Dependency Compatibility..."
# Check for basic system libraries
if which clang++ >/dev/null 2>&1; then
    CLANG_VERSION=$(clang++ --version | head -1)
    echo "   ✅ Compiler: $CLANG_VERSION"
else
    echo "   ⚠️  WARNING: clang++ not found in PATH"
fi

# Check for Homebrew
if which brew >/dev/null 2>&1; then
    echo "   ✅ Homebrew: installed"
else
    echo "   ⚠️  WARNING: Homebrew not installed (required for dependencies)"
fi

echo ""
echo "========================================"
if [ $ERROR_LEVEL -ne 0 ]; then
    echo "❌ VERIFICATION FAILED"
    exit 1
else
    echo "✅ VERIFICATION SUCCESSFUL"
fi
echo "========================================"
