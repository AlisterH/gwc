#!/bin/bash
# Simple all-in-one build script for GTK Wave Cleaner on macOS

cd "$(dirname "$0")/../../.."

echo "🚀 GTK Wave Cleaner - Simple macOS Build"
echo "========================================"
echo ""
echo "This will install dependencies, build, and create an app bundle."
echo "Press Enter to continue or Ctrl+C to cancel..."
read

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
