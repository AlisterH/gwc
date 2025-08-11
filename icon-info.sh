#!/bin/bash
# Compare and manage Mac icons for GTK Wave Cleaner

echo "🎨 GTK Wave Cleaner macOS Icon Management"
echo "========================================"

# Show what we have
echo "📁 Available icon sources:"
echo "   1. data/icons/ - Project PNG icons (8 sizes + SVG)"
echo "   2. Current AppIcon.icns - $(ls -lh "osx_packaging/Gtk Wave Cleaner.app/Contents/Resources/AppIcon.icns" 2>/dev/null | awk '{print $5}' || echo 'Not found')"
echo "   3. Generated AppIcon-new.icns - $(ls -lh AppIcon-new.icns 2>/dev/null | awk '{print $5}' || echo 'Not found')"

echo ""
echo "📊 data/icons PNG sizes available:"
find data/icons -name "gtk-wave-cleaner.png" | sort -V | while read icon; do
    size=$(echo "$icon" | sed 's/.*\/\([0-9]*x[0-9]*\)\/.*/\1/')
    filesize=$(ls -lh "$icon" | awk '{print $5}')
    echo "   • $size - $filesize"
done

echo ""
echo "🔧 Actions available:"
echo "   • ./create-mac-icon.sh - Create new .icns from data/icons"
echo "   • ./create_macos_app.sh - Rebuild app bundle (preserves or generates icon)"
echo ""
echo "💡 The data/icons are high-quality and perfect for Mac icons!"
echo "   They provide all standard macOS icon sizes and maintain transparency."
