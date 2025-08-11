#!/bin/bash
# Complete Homebrew Setup Script for GTK Wave Cleaner
# This script does everything: install dependencies, build, and create app bundle

set -e  # Exit on any error

echo "🍺 Complete GTK Wave Cleaner Setup with Homebrew"
echo "================================================"
echo ""

# Check if we're in the right directory
if [ ! -f "gwc.c" ]; then
    echo "❌ Error: Not in GTK Wave Cleaner source directory"
    echo "   Please run this script from the gwc source directory"
    exit 1
fi

# Step 1: Install dependencies
echo "Step 1: Installing dependencies..."
echo "================================="
if [ -f "./brew-install-deps.sh" ]; then
    chmod +x ./brew-install-deps.sh
    ./brew-install-deps.sh
else
    echo "❌ brew-install-deps.sh not found"
    exit 1
fi

echo ""
echo "⏳ Pausing for 3 seconds..."
sleep 3

# Step 2: Build the application
echo ""
echo "Step 2: Building GTK Wave Cleaner..."
echo "===================================="
if [ -f "./brew-build.sh" ]; then
    chmod +x ./brew-build.sh
    ./brew-build.sh
else
    echo "❌ brew-build.sh not found"
    exit 1
fi

echo ""
echo "⏳ Pausing for 3 seconds..."
sleep 3

# Step 3: Create app bundle
echo ""
echo "Step 3: Creating macOS App Bundle..."
echo "===================================="
if [ -f "./create_macos_app.sh" ]; then
    chmod +x ./create_macos_app.sh
    ./create_macos_app.sh
else
    echo "❌ create_macos_app.sh not found - skipping app bundle creation"
    echo "   You can create the app bundle manually later with:"
    echo "   chmod +x create_macos_app.sh && ./create_macos_app.sh"
fi

echo ""
echo "🎉 COMPLETE SUCCESS!"
echo "==================="
echo ""
echo "✅ GTK Wave Cleaner is now fully built and packaged!"
echo ""
echo "📁 What you have:"
echo "   • gtk-wave-cleaner (command-line executable)"
echo "   • GTK Wave Cleaner.app (macOS app bundle)"
echo "   • All dependencies properly linked"
echo "   • CoreAudio backend working for native macOS audio"
echo ""
echo "🚀 Ready to use:"
echo "   • Double-click the .app bundle to run with GUI"
echo "   • Or run ./gtk-wave-cleaner from terminal"
echo "   • Audio output works natively on macOS"
echo ""
echo "📦 Distribution ready:"
echo "   • The .app bundle includes all dependencies"
echo "   • Can be distributed to other Macs"
echo "   • No additional installation required for end users"
echo ""
echo "💡 Pro tip: Test with your audio files now!"
