#!/bin/bash

# Modern macOS App Bundle Creation Script for GTK Wave Cleaner
# Updated for Homebrew paths and current dependencies

set -e  # Exit on any error

echo "🎵 Creating GTK Wave Cleaner macOS App Bundle..."

# Configuration
APP_NAME="Gtk Wave Cleaner"
APP_DIR="osx_packaging/${APP_NAME}.app"
RESOURCES_DIR="${APP_DIR}/Contents/Resources"
MACOS_DIR="${APP_DIR}/Contents/MacOS"
LIB_DIR="${RESOURCES_DIR}/lib"

# Detect Homebrew prefix (supports both Intel and Apple Silicon)
if [[ -d "/opt/homebrew" ]]; then
    BREW_PREFIX="/opt/homebrew"
elif [[ -d "/usr/local" ]]; then
    BREW_PREFIX="/usr/local"
else
    echo "❌ Error: Homebrew not found in expected locations"
    exit 1
fi

echo "📍 Using Homebrew prefix: ${BREW_PREFIX}"

# Check for gtk-mac-integration availability
echo "🔍 Checking for gtk-mac-integration..."
if ! pkg-config --exists gtk-mac-integration-gtk2; then
    echo "📦 Building gtk-mac-integration from source..."
    
    # Create temporary build directory
    GTK_MAC_BUILD_DIR=$(mktemp -d)
    ORIGINAL_DIR=$(pwd)
    cd "$GTK_MAC_BUILD_DIR"
    
    # Download gtk-mac-integration source
    curl -L -o gtk-mac-integration.tar.xz "https://download.gnome.org/sources/gtk-mac-integration/3.0/gtk-mac-integration-3.0.1.tar.xz"
    tar -xf gtk-mac-integration.tar.xz
    cd gtk-mac-integration-*/
    
    # Configure and build for GTK2
    export PKG_CONFIG_PATH="${BREW_PREFIX}/lib/pkgconfig:${PKG_CONFIG_PATH}"
    export CFLAGS="-I${BREW_PREFIX}/include"
    export LDFLAGS="-L${BREW_PREFIX}/lib"
    
    ./configure --prefix="${BREW_PREFIX}" --with-gtk2 --disable-python --disable-gtk3
    make -j$(sysctl -n hw.ncpu)
    make install
    
    # Return to original directory
    cd "$ORIGINAL_DIR"
    rm -rf "$GTK_MAC_BUILD_DIR"
    
    echo "✅ gtk-mac-integration built and installed"
else
    echo "✅ gtk-mac-integration already available"
fi

# Step 1: Clean and recreate app structure (preserve existing icon)
echo "🧹 Cleaning old app bundle..."

# Backup existing AppIcon.icns if it exists
ICON_BACKUP=""
if [[ -f "${APP_DIR}/Contents/Resources/AppIcon.icns" ]]; then
    ICON_BACKUP=$(mktemp)
    cp "${APP_DIR}/Contents/Resources/AppIcon.icns" "$ICON_BACKUP"
    echo "💾 Backed up existing AppIcon.icns"
fi

rm -rf "${APP_DIR}"
mkdir -p "${RESOURCES_DIR}" "${MACOS_DIR}" "${LIB_DIR}"

# Restore the icon if we backed it up
if [[ -n "$ICON_BACKUP" && -f "$ICON_BACKUP" ]]; then
    cp "$ICON_BACKUP" "${RESOURCES_DIR}/AppIcon.icns"
    rm "$ICON_BACKUP"
    echo "🎨 Restored AppIcon.icns"
fi

# Generate fresh icon from data/icons if icon.sh exists
if [[ -f "contrib/macosx/scripts/icon.sh" && ! -f "${RESOURCES_DIR}/AppIcon.icns" ]]; then
    echo "🎨 Generating fresh AppIcon.icns from data/icons..."
    ./contrib/macosx/scripts/icon.sh >/dev/null 2>&1
    if [[ -f "AppIcon-new.icns" ]]; then
        mv "AppIcon-new.icns" "${RESOURCES_DIR}/AppIcon.icns"
        echo "✅ Fresh AppIcon.icns created from data/icons"
    fi
fi

# Step 2: Install GWC into the app bundle
echo "📦 Installing GTK Wave Cleaner binary..."

# Configure with gtk-mac-integration if available
if pkg-config --exists gtk-mac-integration-gtk2; then
    echo "🔧 Configuring build with GTK Mac Integration..."
    export PKG_CONFIG_PATH="${BREW_PREFIX}/lib/pkgconfig:${PKG_CONFIG_PATH}"
    if [[ -f "configure" ]]; then
        ./configure --prefix=/usr/local
    fi
else
    echo "⚠️  Building without GTK Mac Integration"
    if [[ -f "configure" ]]; then
        ./configure --prefix=/usr/local
    fi
fi

make DESTDIR="$(pwd)/${RESOURCES_DIR}" install

# Move binary to correct location
if [[ -f "${RESOURCES_DIR}/usr/local/bin/gtk-wave-cleaner" ]]; then
    mv "${RESOURCES_DIR}/usr/local/bin/gtk-wave-cleaner" "${MACOS_DIR}/gtk-wave-cleaner"
    rm -rf "${RESOURCES_DIR}/usr"
elif [[ -f "${RESOURCES_DIR}/bin/gtk-wave-cleaner" ]]; then
    mv "${RESOURCES_DIR}/bin/gtk-wave-cleaner" "${MACOS_DIR}/gtk-wave-cleaner"
    rm -rf "${RESOURCES_DIR}/bin"
else
    echo "❌ Error: gtk-wave-cleaner binary not found after installation"
    exit 1
fi

echo "✅ Binary installed at: ${MACOS_DIR}/gtk-wave-cleaner"

# Step 3: Copy required dylibs
echo "📚 Copying required dynamic libraries..."

# Core GTK/GLib libraries
cp "${BREW_PREFIX}/lib/libgtk-quartz-2.0.0.dylib" "${LIB_DIR}/"
cp "${BREW_PREFIX}/lib/libgdk-quartz-2.0.0.dylib" "${LIB_DIR}/"
cp "${BREW_PREFIX}/lib/libgio-2.0.0.dylib" "${LIB_DIR}/"
cp "${BREW_PREFIX}/lib/libgobject-2.0.0.dylib" "${LIB_DIR}/"
cp "${BREW_PREFIX}/lib/libglib-2.0.0.dylib" "${LIB_DIR}/"

# Pango libraries
cp "${BREW_PREFIX}/lib/libpangocairo-1.0.0.dylib" "${LIB_DIR}/"
cp "${BREW_PREFIX}/lib/libpango-1.0.0.dylib" "${LIB_DIR}/"

# Cairo and rendering
cp "${BREW_PREFIX}/lib/libcairo.2.dylib" "${LIB_DIR}/"
cp "${BREW_PREFIX}/lib/libgdk_pixbuf-2.0.0.dylib" "${LIB_DIR}/"

# ATK accessibility
cp "${BREW_PREFIX}/lib/libatk-1.0.0.dylib" "${LIB_DIR}/"

# Audio libraries
cp "${BREW_PREFIX}/lib/libsndfile.1.dylib" "${LIB_DIR}/"

# Math libraries
cp "${BREW_PREFIX}/lib/libfftw3.3.dylib" "${LIB_DIR}/"

# Support libraries
cp "${BREW_PREFIX}/lib/libintl.8.dylib" "${LIB_DIR}/"

# Optional: Additional format support
if [[ -f "${BREW_PREFIX}/lib/libFLAC.12.dylib" ]]; then
    cp "${BREW_PREFIX}/lib/libFLAC.12.dylib" "${LIB_DIR}/"
fi

if [[ -f "${BREW_PREFIX}/lib/libogg.0.dylib" ]]; then
    cp "${BREW_PREFIX}/lib/libogg.0.dylib" "${LIB_DIR}/"
fi

if [[ -f "${BREW_PREFIX}/lib/libvorbis.0.dylib" ]]; then
    cp "${BREW_PREFIX}/lib/libvorbis.0.dylib" "${LIB_DIR}/"
fi

echo "✅ Dynamic libraries copied"

# Step 4: Copy configuration files
echo "⚙️  Copying configuration files..."
if [[ -d "${RESOURCES_DIR}/usr/local/share" ]]; then
    mv "${RESOURCES_DIR}/usr/local/share" "${RESOURCES_DIR}/share"
fi

# Step 5: Create Info.plist
echo "📄 Creating Info.plist..."
cat > "${APP_DIR}/Contents/Info.plist" << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>gtk-wave-cleaner</string>
    <key>CFBundleIdentifier</key>
    <string>org.gwc.gtk-wave-cleaner</string>
    <key>CFBundleName</key>
    <string>GTK Wave Cleaner</string>
    <key>CFBundleDisplayName</key>
    <string>GTK Wave Cleaner</string>
    <key>CFBundleShortVersionString</key>
    <string>0.22</string>
    <key>CFBundleVersion</key>
    <string>0.22</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleSignature</key>
    <string>????</string>
    <key>CFBundleIconFile</key>
    <string>AppIcon</string>
    <key>LSMinimumSystemVersion</key>
    <string>10.14</string>
    <key>CFBundleDocumentTypes</key>
    <array>
        <dict>
            <key>CFBundleTypeName</key>
            <string>Audio File</string>
            <key>CFBundleTypeExtensions</key>
            <array>
                <string>wav</string>
                <string>aiff</string>
                <string>au</string>
                <string>snd</string>
                <string>flac</string>
                <string>ogg</string>
                <string>mp3</string>
            </array>
            <key>CFBundleTypeRole</key>
            <string>Editor</string>
        </dict>
    </array>
    <key>NSHighResolutionCapable</key>
    <true/>
</dict>
</plist>
EOF

echo "✅ Info.plist created"

# Step 6: Create wrapper script for proper library loading
echo "📝 Creating wrapper script..."
cat > "${MACOS_DIR}/gtk-wave-cleaner-wrapper" << 'EOF'
#!/bin/bash

# Get the directory containing this script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_RESOURCES="$(dirname "$SCRIPT_DIR")/Resources"

# Set library path to use bundled libraries
export DYLD_LIBRARY_PATH="${APP_RESOURCES}/lib:${DYLD_LIBRARY_PATH}"

# Set up GTK/GLib paths
export GTK_PATH="${APP_RESOURCES}"
export GDK_PIXBUF_MODULEDIR="${APP_RESOURCES}/lib/gdk-pixbuf-2.0/loaders"

# Execute the actual binary
exec "${SCRIPT_DIR}/gtk-wave-cleaner" "$@"
EOF

chmod +x "${MACOS_DIR}/gtk-wave-cleaner-wrapper"

# Update Info.plist to use wrapper
sed -i '' 's/<string>gtk-wave-cleaner<\/string>/<string>gtk-wave-cleaner-wrapper<\/string>/' "${APP_DIR}/Contents/Info.plist"

echo "✅ Wrapper script created"

# Step 7: Strip binaries to reduce size
echo "🔧 Stripping binaries..."
strip "${MACOS_DIR}/gtk-wave-cleaner"
for lib in "${LIB_DIR}"/*.dylib; do
    if [[ -f "$lib" ]]; then
        strip -x "$lib" 2>/dev/null || true
    fi
done

echo "✅ Binaries stripped"

# Step 8: Create disk image
echo "💿 Creating disk image..."
if [[ -f "GWC-$(date +%Y%m%d).dmg" ]]; then
    rm "GWC-$(date +%Y%m%d).dmg"
fi

hdiutil create -fs "HFS+" -volname "Gtk Wave Cleaner" -srcfolder "${APP_DIR}" "GWC-$(date +%Y%m%d).dmg"

echo "🎉 SUCCESS! GTK Wave Cleaner app bundle created!"
echo "📁 App bundle: ${APP_DIR}"
echo "💿 Disk image: GWC-$(date +%Y%m%d).dmg"
echo ""
echo "🚀 You can now:"
echo "   1. Test the app: open '${APP_DIR}'"
echo "   2. Install from disk image: open 'GWC-$(date +%Y%m%d).dmg'"
echo "   3. Copy to Applications folder for system-wide access"
