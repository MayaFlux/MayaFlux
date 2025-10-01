#!/bin/zsh

echo "MayaFlux Xcode Project Setup"
echo "============================"
echo

# Get the project root directory (one level up from the script location)
SCRIPT_DIR="${0:a:h}"
PROJECT_ROOT=$(dirname "$SCRIPT_DIR")

# Check if script has execute permissions
if [ ! -x "$0" ]; then
    echo "Warning: This script doesn't have execute permissions."
    echo "You may need to run: chmod +x $0"
    echo
fi

# Check macOS version requirement first
MACOS_VERSION=$(sw_vers -productVersion)
MACOS_MAJOR=$(echo $MACOS_VERSION | cut -d. -f1)

if [ "$MACOS_MAJOR" -lt 14 ]; then
    echo "❌ Error: MayaFlux requires macOS 14 (Sonoma) or later."
    echo "Current macOS version: $MACOS_VERSION"
    echo "Please upgrade to macOS 14+ or run './scripts/setup_macos.sh' first."
    exit 1
fi

# Verify dependencies are installed
echo "Checking system dependencies..."
MISSING_DEPS=""

# Check for CMake
if ! command -v cmake &>/dev/null; then
    MISSING_DEPS="$MISSING_DEPS cmake"
fi

# Check for key Homebrew packages
for pkg in rtaudio ffmpeg googletest eigen onedpl magic_enum fmt glfw; do
    if ! brew list --formula | grep -q "^${pkg}$" 2>/dev/null; then
        MISSING_DEPS="$MISSING_DEPS $pkg"
    fi
done

if [ -n "$MISSING_DEPS" ]; then
    echo "❌ Error: Missing dependencies:$MISSING_DEPS"
    echo "Please run './scripts/setup_macos.sh' first to install system dependencies."
    exit 1
fi

# Check if Xcode command line tools are installed
if ! xcode-select -p &>/dev/null; then
    echo "❌ Error: Xcode command line tools are not installed."
    echo "Please install by running: xcode-select --install"
    exit 1
fi

echo "✅ All dependencies verified for macOS $MACOS_VERSION"
echo

echo "Creating build directory..."
mkdir -p "$PROJECT_ROOT/build"

echo "Generating Xcode project..."
cd "$PROJECT_ROOT" || exit 1
cmake -B build -S . -G Xcode \
    -DCMAKE_INSTALL_PREFIX="$PROJECT_ROOT/install"

if [ $? -ne 0 ]; then
    echo
    echo "❌ Failed to generate Xcode project."
    echo "Common issues:"
    echo "1. Missing CMakeLists.txt in project root"
    echo "2. CMake configuration errors"
    echo "3. Missing dependencies"
    exit 1
fi

# Create convenience scripts
echo "Creating convenience scripts..."

# Script to open Xcode project
cat >"$PROJECT_ROOT/open_xcode.sh" <<EOF
#!/bin/bash
# Open MayaFlux project in Xcode
PROJECT_ROOT="\$(dirname "\$0")"
if [ -f "\$PROJECT_ROOT/build/MayaFlux.xcodeproj/project.pbxproj" ]; then
    open "\$PROJECT_ROOT/build/MayaFlux.xcodeproj"
else
    echo "Xcode project not found. Run './scripts/setup_xcode.sh' first."
    exit 1
fi
EOF

chmod +x "$PROJECT_ROOT/open_xcode.sh"

# Script to build from command line
cat >"$PROJECT_ROOT/build_macos.sh" <<EOF
#!/bin/bash
# Build MayaFlux from command line
PROJECT_ROOT="\$(dirname "\$0")"
echo "Building MayaFlux..."
cmake --build "\$PROJECT_ROOT/build" --config Release
if [ \$? -eq 0 ]; then
    echo "✅ Build successful!"
    echo "Binary location: \$PROJECT_ROOT/build/bin/Release/MayaFlux"
else
    echo "❌ Build failed. See error messages above."
    exit 1
fi
EOF

chmod +x "$PROJECT_ROOT/build_macos.sh"

# Script to clean and regenerate project
cat >"$PROJECT_ROOT/clean_xcode.sh" <<EOF
#!/bin/bash
# Clean and regenerate Xcode project
PROJECT_ROOT="\$(dirname "\$0")"
echo "Cleaning build directory..."
rm -rf "\$PROJECT_ROOT/build"
echo "Regenerating Xcode project..."
cd "\$PROJECT_ROOT"
cmake -B build -S . -G Xcode -DCMAKE_INSTALL_PREFIX="\$PROJECT_ROOT/install"
if [ \$? -eq 0 ]; then
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
echo "Available commands:"
echo "  ./open_xcode.sh      - Open project in Xcode"
echo "  ./build_macos.sh     - Build from command line"
echo "  ./clean_xcode.sh     - Clean and regenerate project"
echo
echo "Manual options:"
echo "  open $PROJECT_ROOT/build/MayaFlux.xcodeproj"
echo "  cmake --build build --config Release"
echo

# Add a note about macOS security
echo "Note: If you encounter security warnings when running scripts,"
echo "you may need to adjust your security settings or right-click and select 'Open'."
echo "For more info: https://support.apple.com/guide/mac-help/open-a-mac-app-from-an-unidentified-developer-mh40616/mac"
echo
