#!/bin/bash
# Simple all-in-one build script for GTK Wave Cleaner on macOS

cd "$(dirname "$0")/../../.."

echo "🚀 GTK Wave Cleaner - Simple macOS Build"
echo "========================================"
echo ""
echo "This will install dependencies, build, and create an app bundle."
echo "Features included:"
echo "  • Native macOS menu bar integration"
echo "  • CoreAudio backend for native audio"
echo "  • Complete app bundle with all dependencies"

echo ""
echo "Step 1/3: Installing dependencies..."
./contrib/macosx/scripts/install.sh

echo ""
echo "Step 2/3: Building application..."
./contrib/macosx/scripts/build.sh

echo ""
echo "Step 3/3: Creating app bundle..."
./contrib/macosx/scripts/app.sh

echo ""
echo "🎉 All done! Your app is ready in the 'Gtk Wave Cleaner.app' bundle."
echo ""
echo "✨ Features:"
echo "  • Native macOS menu bar (menus appear in system menu bar)"
echo "  • Native dock integration and window management"
echo "  • CoreAudio backend for optimal macOS audio performance"
echo "  • Complete standalone app bundle"
