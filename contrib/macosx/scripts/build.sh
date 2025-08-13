#!/bin/bash
# Homebrew Build Script for GTK Wave Cleaner
# This script configures and builds GWC with Homebrew dependencies

set -e  # Exit on any error

echo "🔨 Building GTK Wave Cleaner with Homebrew dependencies..."
echo "========================================================"

# Check if we're in the right directory
if [ ! -f "gwc.c" ]; then
    echo "❌ Error: Not in GTK Wave Cleaner source directory"
    echo "   Please run this script from the gwc source directory"
    exit 1
fi

# Check if dependencies are installed
echo "🔍 Checking Homebrew dependencies..."

MISSING_DEPS=()

if ! brew list gtk+ &>/dev/null; then
    MISSING_DEPS+=("gtk+")
fi

if ! brew list libsndfile &>/dev/null; then
    MISSING_DEPS+=("libsndfile")
fi

if ! brew list fftw &>/dev/null; then
    MISSING_DEPS+=("fftw")
fi

if ! brew list autoconf &>/dev/null; then
    MISSING_DEPS+=("autoconf")
fi

if [ ${#MISSING_DEPS[@]} -ne 0 ]; then
    echo "❌ Missing dependencies: ${MISSING_DEPS[*]}"
    echo "   Run ./contrib/macosx/scripts/install.sh first to install them"
    exit 1
fi

echo "✅ All dependencies found"

# Set up environment for Homebrew paths
echo "🔧 Setting up Homebrew environment..."
export PKG_CONFIG_PATH="$(brew --prefix)/lib/pkgconfig:$PKG_CONFIG_PATH"
export CPPFLAGS="-I$(brew --prefix)/include $CPPFLAGS"
export LDFLAGS="-L$(brew --prefix)/lib $LDFLAGS"

# For Apple Silicon Macs, ensure we have the right paths
if [ "$(uname -m)" = "arm64" ]; then
    export PKG_CONFIG_PATH="/opt/homebrew/lib/pkgconfig:$PKG_CONFIG_PATH"
    export CPPFLAGS="-I/opt/homebrew/include $CPPFLAGS"
    export LDFLAGS="-L/opt/homebrew/lib $LDFLAGS"
    echo "📱 Detected Apple Silicon - using /opt/homebrew paths"
else
    echo "💻 Detected Intel Mac - using /usr/local paths"
fi

# Clean previous builds
echo "🧹 Cleaning previous build artifacts..."
make distclean 2>/dev/null || true
rm -f config.cache

# Generate configure script if needed
if [ ! -f "configure" ]; then
    echo "⚙️  Generating configure script..."
    autoreconf -fiv
fi

# Configure with Homebrew paths and CoreAudio backend
echo "🔧 Configuring build..."
./configure \
    --prefix=/usr/local \
    --enable-coreaudio \
    --disable-pulseaudio \
    PKG_CONFIG_PATH="$PKG_CONFIG_PATH" \
    CPPFLAGS="$CPPFLAGS" \
    LDFLAGS="$LDFLAGS"

echo "✅ Configuration complete"

# Build the application
echo "🔨 Building GTK Wave Cleaner..."
make -j$(sysctl -n hw.ncpu)

echo ""
echo "🎉 Build completed successfully!"
echo ""
echo "📋 What's built:"
echo "   • gtk-wave-cleaner (main executable)"
echo "   • All required libraries compiled"
echo "   • CoreAudio backend enabled for native macOS audio"
echo ""
echo "🚀 Next steps:"
echo "   • Test: ./gtk-wave-cleaner"
echo "   • Create app bundle: ./contrib/macosx/scripts/app.sh"
echo "   • Install system-wide: sudo make install"
echo ""
echo "💡 Tips:"
echo "   • Audio output uses CoreAudio (no additional setup needed)"
echo "   • App bundle includes all dependencies for distribution"
echo "   • Use 'otool -L gtk-wave-cleaner' to check library dependencies"
