#!/bin/bash

# Simple App Bundle Completion Script
set -e

echo "🔧 Completing GTK Wave Cleaner App Bundle..."

APP_DIR="osx_packaging/Gtk Wave Cleaner.app"
RESOURCES_DIR="${APP_DIR}/Contents/Resources"
MACOS_DIR="${APP_DIR}/Contents/MacOS"
LIB_DIR="${RESOURCES_DIR}/lib"

# Check structure
echo "📁 Current structure:"
ls -la "${APP_DIR}/Contents/"

# Copy essential libraries only
echo "📚 Copying essential libraries..."
cp /opt/homebrew/lib/libsndfile.1.dylib "${LIB_DIR}/" || echo "⚠️  libsndfile copy failed"
cp /opt/homebrew/lib/libfftw3.3.dylib "${LIB_DIR}/" || echo "⚠️  libfftw3 copy failed"

# Create wrapper script
echo "📝 Creating wrapper script..."
cat > "${MACOS_DIR}/gtk-wave-cleaner-wrapper" << 'EOF'
#!/bin/bash

# Get the directory containing this script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_RESOURCES="$(dirname "$SCRIPT_DIR")/Resources"

# Set library path to use bundled libraries
export DYLD_LIBRARY_PATH="${APP_RESOURCES}/lib:${DYLD_LIBRARY_PATH}"

# Execute the actual binary
exec "${SCRIPT_DIR}/gtk-wave-cleaner" "$@"
EOF

chmod +x "${MACOS_DIR}/gtk-wave-cleaner-wrapper"

echo "✅ App bundle structure completed!"
echo "📁 App bundle: ${APP_DIR}"
echo ""
echo "🚀 Test the app:"
echo "   open '${APP_DIR}'"
echo ""
echo "Note: The app will use system GTK libraries where bundled ones are not available."
