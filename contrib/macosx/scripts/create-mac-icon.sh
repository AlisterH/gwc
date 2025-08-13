#!/bin/bash
# Create macOS .icns file from data/icons PNG files

set -e

echo "🎨 Creating macOS .icns file from data/icons..."

# Create temporary iconset directory
ICONSET_DIR="$(mktemp -d)/AppIcon.iconset"
mkdir -p "$ICONSET_DIR"

# Map the available PNG files to macOS iconset naming convention
# macOS expects specific filenames for different resolutions

echo "📋 Copying icon files with proper macOS naming..."

# 16x16
cp "data/icons/hicolor/16x16/apps/gtk-wave-cleaner.png" "$ICONSET_DIR/icon_16x16.png"

# 32x32 (also used as 16x16@2x)
cp "data/icons/hicolor/32x32/apps/gtk-wave-cleaner.png" "$ICONSET_DIR/icon_16x16@2x.png"
cp "data/icons/hicolor/32x32/apps/gtk-wave-cleaner.png" "$ICONSET_DIR/icon_32x32.png"

# 64x64 (used as 32x32@2x)
cp "data/icons/hicolor/64x64/apps/gtk-wave-cleaner.png" "$ICONSET_DIR/icon_32x32@2x.png"

# 128x128 (also used as 64x64@2x)
cp "data/icons/hicolor/128x128/apps/gtk-wave-cleaner.png" "$ICONSET_DIR/icon_128x128.png"
cp "data/icons/hicolor/128x128/apps/gtk-wave-cleaner.png" "$ICONSET_DIR/icon_64x64@2x.png"

# 256x256 (also used as 128x128@2x)
cp "data/icons/hicolor/256x256/apps/gtk-wave-cleaner.png" "$ICONSET_DIR/icon_128x128@2x.png"
cp "data/icons/hicolor/256x256/apps/gtk-wave-cleaner.png" "$ICONSET_DIR/icon_256x256.png"

# For 512x512, we'll scale up the 256x256 using sips (macOS built-in)
echo "🔍 Creating 512x512 icon from 256x256..."
sips -z 512 512 "data/icons/hicolor/256x256/apps/gtk-wave-cleaner.png" --out "$ICONSET_DIR/icon_256x256@2x.png" >/dev/null
cp "$ICONSET_DIR/icon_256x256@2x.png" "$ICONSET_DIR/icon_512x512.png"

# For 1024x1024, scale up further
echo "🔍 Creating 1024x1024 icon from 256x256..."
sips -z 1024 1024 "data/icons/hicolor/256x256/apps/gtk-wave-cleaner.png" --out "$ICONSET_DIR/icon_512x512@2x.png" >/dev/null

echo "📁 Iconset contents:"
ls -la "$ICONSET_DIR"

# Create the .icns file
echo "🔨 Creating AppIcon.icns..."
iconutil -c icns "$ICONSET_DIR" -o "AppIcon-new.icns"

echo "✅ New AppIcon.icns created!"
echo "📊 File comparison:"
echo "   Original: $(ls -lh "osx_packaging/Gtk Wave Cleaner.app/Contents/Resources/AppIcon.icns" | awk '{print $5}')"
echo "   New:      $(ls -lh AppIcon-new.icns | awk '{print $5}')"

echo ""
echo "🔄 To use the new icon:"
echo "   cp AppIcon-new.icns \"osx_packaging/Gtk Wave Cleaner.app/Contents/Resources/AppIcon.icns\""
echo "   ./create_macos_app.sh  # Rebuild app bundle"

# Cleanup
rm -rf "$(dirname "$ICONSET_DIR")"
