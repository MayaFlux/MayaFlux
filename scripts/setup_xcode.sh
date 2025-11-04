#!/bin/zsh

echo "MayaFlux Xcode Project Setup"
echo "============================"
echo

SCRIPT_DIR="${0:a:h}"
PROJECT_ROOT=$(dirname "$SCRIPT_DIR")

if [ ! -x "$0" ]; then
    echo "Warning: This script doesn't have execute permissions."
    echo "You may need to run: chmod +x $0"
    echo
fi

MACOS_VERSION=$(sw_vers -productVersion)
MACOS_MAJOR=$(echo $MACOS_VERSION | cut -d. -f1)

if [ "$MACOS_MAJOR" -lt 14 ]; then
    echo "❌ Error: MayaFlux requires macOS 14 (Sonoma) or later."
    echo "Current macOS version: $MACOS_VERSION"
    echo "Please upgrade to macOS 14+ or run './scripts/setup_macos.sh' first."
    exit 1
fi

echo "Checking system dependencies..."
MISSING_DEPS=()

if ! command -v cmake &>/dev/null; then
    MISSING_DEPS+=("cmake")
fi

FOUND_LLVM_JIT=false
for version in llvm@21 llvm@20; do
    if brew list --formula 2>/dev/null | grep -q "^${version}$"; then
        LLVM_PREFIX=$(brew --prefix $version 2>/dev/null)
        if [[ -f "$LLVM_PREFIX/lib/cmake/llvm/LLVMConfig.cmake" ]]; then
            FOUND_LLVM_JIT=true
            LLVM_JIT_VERSION=$version
            break
        fi
    fi
done

if ! $FOUND_LLVM_JIT; then
    MISSING_DEPS+=("llvm@21 or llvm@20 (required for Lila JIT)")
fi

if [[ ! -d "$HOME/VulkanSDK" ]]; then
    MISSING_DEPS+=("Vulkan SDK")
else
    VULKAN_VERSION=$(ls "$HOME/VulkanSDK" 2>/dev/null | sort -V | tail -1)
    if [[ -z "$VULKAN_VERSION" ]] || [[ ! -d "$HOME/VulkanSDK/$VULKAN_VERSION/macOS" ]]; then
        MISSING_DEPS+=("Vulkan SDK (corrupted installation)")
    fi
fi

for pkg in rtaudio ffmpeg googletest eigen onedpl fmt glfw glm; do
    if ! brew list --formula 2>/dev/null | grep -q "^${pkg}$"; then
        MISSING_DEPS+=("$pkg")
    fi
done

if ! brew list --formula 2>/dev/null | grep -q "^magic_enum$"; then
    echo "⚠️  magic_enum not found (will use FetchContent)"
fi

if [ ${#MISSING_DEPS[@]} -gt 0 ]; then
    echo "❌ Error: Missing dependencies:"
    for dep in "${MISSING_DEPS[@]}"; do
        echo "  - $dep"
    done
    echo
    echo "Please run './scripts/setup_macos.sh' first to install system dependencies."
    exit 1
fi

if ! xcode-select -p &>/dev/null; then
    echo "❌ Error: Xcode command line tools are not installed."
    echo "Please install by running: xcode-select --install"
    exit 1
fi

echo "✅ All dependencies verified for macOS $MACOS_VERSION"
if $FOUND_LLVM_JIT; then
    echo "✅ Lila JIT LLVM: $LLVM_JIT_VERSION at $LLVM_PREFIX"
fi
if [[ -n "$VULKAN_VERSION" ]]; then
    echo "✅ Vulkan SDK: $VULKAN_VERSION"
fi
echo

if [[ -f "$HOME/.mayaflux_macos_env" ]]; then
    echo "Loading MayaFlux environment..."
    source "$HOME/.mayaflux_macos_env"
fi

echo "Creating build directory..."
mkdir -p "$PROJECT_ROOT/build"

echo "Generating Xcode project..."
cd "$PROJECT_ROOT" || exit 1

cmake -B build -S . -G Xcode \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PROJECT_ROOT/install"

if [ $? -ne 0 ]; then
    echo
    echo "❌ Failed to generate Xcode project."
    echo
    echo "Common issues:"
    echo "1. Missing CMakeLists.txt in project root"
    echo "2. CMake configuration errors"
    echo "3. Missing dependencies"
    echo "4. LLVM_JIT_DIR not set (run: source ~/.mayaflux_macos_env)"
    exit 1
fi

echo "Creating convenience scripts..."

cat >"$PROJECT_ROOT/open_xcode.sh" <<'EOF'
#!/bin/zsh
# Open MayaFlux project in Xcode
PROJECT_ROOT="$(dirname "$0")"

# Load environment
if [[ -f "$HOME/.mayaflux_macos_env" ]]; then
    source "$HOME/.mayaflux_macos_env"
fi

if [ -f "$PROJECT_ROOT/build/MayaFlux.xcodeproj/project.pbxproj" ]; then
    open "$PROJECT_ROOT/build/MayaFlux.xcodeproj"
else
    echo "Xcode project not found. Run './scripts/setup_xcode.sh' first."
    exit 1
fi
EOF

chmod +x "$PROJECT_ROOT/open_xcode.sh"

cat >"$PROJECT_ROOT/build_macos.sh" <<'EOF'
#!/bin/zsh
# Build MayaFlux from command line
PROJECT_ROOT="$(dirname "$0")"

# Load environment
if [[ -f "$HOME/.mayaflux_macos_env" ]]; then
    source "$HOME/.mayaflux_macos_env"
fi

echo "Building MayaFlux..."
cmake --build "$PROJECT_ROOT/build" --config Release --parallel $(sysctl -n hw.ncpu)

if [ $? -eq 0 ]; then
    echo "✅ Build successful!"
    echo "Binary location: $PROJECT_ROOT/build/Release/MayaFlux"
else
    echo "❌ Build failed. See error messages above."
    exit 1
fi
EOF

chmod +x "$PROJECT_ROOT/build_macos.sh"

cat >"$PROJECT_ROOT/clean_xcode.sh" <<'EOF'
#!/bin/zsh
# Clean and regenerate Xcode project
PROJECT_ROOT="$(dirname "$0")"

# Load environment
if [[ -f "$HOME/.mayaflux_macos_env" ]]; then
    source "$HOME/.mayaflux_macos_env"
fi

echo "Cleaning build directory..."
rm -rf "$PROJECT_ROOT/build"
echo "Regenerating Xcode project..."
cd "$PROJECT_ROOT"
cmake -B build -S . -G Xcode \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PROJECT_ROOT/install"

if [ $? -eq 0 ]; then
    echo "✅ Project regenerated successfully!"
else
    echo "❌ Failed to regenerate project."
    exit 1
fi
EOF

chmod +x "$PROJECT_ROOT/clean_xcode.sh"

echo
echo "✅ Xcode setup complete for macOS $MACOS_VERSION!"
echo
echo "Configuration:"
echo "  - Build with system clang (Xcode)"
echo "  - Lila JIT uses: $LLVM_JIT_VERSION"
if [[ -n "$VULKAN_VERSION" ]]; then
    echo "  - Vulkan SDK: $VULKAN_VERSION"
fi
echo
echo "Available commands:"
echo "  ./open_xcode.sh      - Open project in Xcode"
echo "  ./build_macos.sh     - Build from command line (parallel)"
echo "  ./clean_xcode.sh     - Clean and regenerate project"
echo
echo "Manual options:"
echo "  open $PROJECT_ROOT/build/MayaFlux.xcodeproj"
echo "  cmake --build build --config Release --parallel \$(sysctl -n hw.ncpu)"
echo
echo "Note: MayaFlux builds with system clang for ABI compatibility"
echo "      Lila uses $LLVM_JIT_VERSION at runtime for JIT"
echo
