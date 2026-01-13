#!/bin/bash
# Homebrew Dependency Installation Script for GTK Wave Cleaner
# This script installs all required dependencies for building GWC on macOS

set -e  # Exit on any error

echo "🍺 Installing GTK Wave Cleaner dependencies via Homebrew..."
echo "=================================================="

# Check if Homebrew is installed
if ! command -v brew &> /dev/null; then
    echo "❌ Homebrew is not installed. Please install it first:"
    echo "   /bin/bash -c \"\$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\""
    exit 1
fi

echo "✅ Homebrew found at: $(which brew)"

# Update Homebrew
echo "📦 Updating Homebrew..."
brew update

# Core build tools
echo "🔧 Installing build tools..."
brew install autoconf automake libtool pkg-config

# GTK2 and related libraries
echo "🖼️  Installing GTK2 and GUI libraries..."
brew install gtk+

# macOS GTK integration
echo "🍎 Installing GTK macOS integration..."
brew install gtk-mac-integration

# Audio libraries
echo "🎵 Installing audio libraries..."
brew install libsndfile

# Math and signal processing
echo "📊 Installing FFTW3 for signal processing..."
brew install fftw

# Optional audio format support
echo "🎧 Installing optional audio format libraries..."
brew install vorbis-tools lame

# Optional: PulseAudio (commented out since CoreAudio is working and preferred)
# echo "🔊 Installing PulseAudio (optional)..."
# brew install pulseaudio

echo ""
echo "✅ All dependencies installed successfully!"
echo ""
echo "📋 Summary of installed packages:"
echo "   • autoconf, automake, libtool, pkg-config (build tools)"
echo "   • gtk+ (GTK2 GUI framework)"
echo "   • gtk-mac-integration (native macOS integration)"
echo "   • libsndfile (audio file I/O)"
echo "   • fftw (FFT calculations)"
echo "   • vorbis-tools, lame (additional audio formats)"
echo ""
echo "🚀 Ready to build GTK Wave Cleaner!"
echo "   Next steps:"
echo "   1. Run ./contrib/macosx/scripts/build.sh to configure and build"
echo "   2. Or manually: ./configure && make"
echo "   3. Create app bundle: ./contrib/macosx/scripts/app.sh"
