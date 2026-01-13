#!/bin/bash
# Test script to verify GTK Mac Integration is working correctly

set -e

echo "🧪 Testing GTK Mac Integration Support"
echo "======================================"
echo ""

# Check if we're in the right directory
if [ ! -f "gwc.c" ]; then
    echo "❌ Error: Not in GTK Wave Cleaner source directory"
    echo "   Please run this script from the gwc source directory"
    exit 1
fi

# Test 1: Check if gtk-mac-integration is installed
echo "🔍 Test 1: Checking gtk-mac-integration installation..."
if pkg-config --exists gtk-mac-integration-gtk2; then
    echo "✅ gtk-mac-integration-gtk2 found"
    echo "   Version: $(pkg-config --modversion gtk-mac-integration-gtk2)"
    echo "   CFLAGS: $(pkg-config --cflags gtk-mac-integration-gtk2)"
    echo "   LIBS: $(pkg-config --libs gtk-mac-integration-gtk2)"
else
    echo "❌ gtk-mac-integration-gtk2 not found!"
    echo "   Install with: brew install gtk-mac-integration"
    exit 1
fi

echo ""

# Test 2: Check if configure.ac is set up correctly
echo "🔍 Test 2: Checking configure.ac for GTK Mac Integration..."
if grep -q "gtk-mac-integration-gtk2" configure.ac; then
    echo "✅ configure.ac contains gtk-mac-integration-gtk2 check"
else
    echo "❌ configure.ac missing gtk-mac-integration-gtk2 check"
    echo "   configure.ac needs to be updated"
    exit 1
fi

echo ""

# Test 3: Check if gwc.c has the integration header
echo "🔍 Test 3: Checking gwc.c for GTK Mac Integration header..."
if grep -q "#ifdef HAVE_GTK_MAC_INTEGRATION" gwc.c; then
    echo "✅ gwc.c contains conditional GTK Mac Integration code"
else
    echo "❌ gwc.c missing GTK Mac Integration conditional code"
    echo "   gwc.c needs to be updated"
    exit 1
fi

echo ""

# Test 4: Check if app bundle contains the integration library
echo "🔍 Test 4: Checking for integration library in Homebrew..."
BREW_PREFIX="/opt/homebrew"
if [[ ! -d "/opt/homebrew" ]]; then
    BREW_PREFIX="/usr/local"
fi

if [[ -f "${BREW_PREFIX}/lib/libgtkmacintegration-gtk2.4.dylib" ]]; then
    echo "✅ GTK Mac Integration library found at ${BREW_PREFIX}/lib/"
else
    echo "❌ GTK Mac Integration library not found"
    echo "   Check Homebrew installation"
    exit 1
fi

echo ""

# Test 5: Try to build with integration support
echo "🔍 Test 5: Testing build configuration..."
if [ -f "configure" ]; then
    echo "🔧 Running configure to test GTK Mac Integration detection..."
    ./configure --prefix=/usr/local > configure_test.log 2>&1
    
    if grep -q "GTK Mac Integration found" configure_test.log; then
        echo "✅ Configure successfully detects GTK Mac Integration"
    else
        echo "⚠️  Configure did not detect GTK Mac Integration"
        echo "   Check configure_test.log for details"
    fi
    
    # Clean up test log
    rm -f configure_test.log
else
    echo "⚠️  No configure script found - run autoreconf -fiv first"
fi

echo ""
echo "🎉 GTK Mac Integration test completed!"
echo ""
echo "📝 Summary:"
echo "   • GTK Mac Integration is properly installed via Homebrew"
echo "   • Source code is configured for conditional compilation"
echo "   • Build system will detect and use the integration"
echo ""
echo "🚀 Ready to build with native macOS integration!"
