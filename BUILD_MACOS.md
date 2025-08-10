# Building GTK Wave Cleaner on macOS

This document describes the steps needed to build GTK Wave Cleaner on macOS using Homebrew.

## Overview

GTK Wave Cleaner (GWC) is a GTK2-based audio noise removal application originally designed for Linux. To build it on macOS, we need to:

1. Install required Homebrew packages
2. Adjust build configuration for macOS
3. Fix audio backend selection (use PulseAudio instead of broken CoreAudio)
4. Modernize autotools configuration
5. Fix legacy library compilation issues (Meschach)
6. Resolve header path detection for FFTW3 and PulseAudio

## Prerequisites

- macOS 10.14 or later
- Xcode Command Line Tools: `xcode-select --install`
- Homebrew: https://brew.sh

## Step 1: Install Required Homebrew Packages

```bash
# Core dependencies
brew install autoconf automake libtool pkg-config

# GTK2 and related libraries
brew install gtk+

# Audio libraries
brew install libsndfile pulseaudio

# Math and signal processing libraries
brew install fftw

# Optional dependencies for additional formats
brew install vorbis-tools lame
```

**Note:** `gtk-mac-integration-gtk2` is not available in current Homebrew for GTK2. The build has been configured to work without it for now.

## Step 2: Audio Backend Configuration

The original CoreAudio backend is broken. We use PulseAudio instead:

```bash
# Start PulseAudio daemon (required for audio playback)
brew services start pulseaudio
```

## Step 3: Build Process

### Current Status: ✅ Major compilation fixes completed

```bash
# Generate build files (modernized autotools)
autoconf

# Configure with PulseAudio and macOS-specific settings
./configure --enable-pa

# Build
make

# Install (optional)
make install
```

## Progress Summary

### ✅ Completed Tasks
1. **Autotools Modernization** - Updated configure.in → configure.ac, removed deprecated macros
2. **FFTW3 Detection & Headers** - Fixed pkg-config detection and include path configuration  
3. **Meschach Library Compilation** - Resolved all compilation errors with automatic patching
4. **PulseAudio Headers** - Fixed include path detection for PulseAudio headers
5. **GTK Mac Integration** - Made optional (disabled for now due to unavailable package)
6. **Missing Function Declarations** - Added missing warning() function to tap_reverb.c
7. **Type Compatibility** - Fixed integer/pointer type issues

### 🔄 Current Status
- Main application is compiling successfully
- Only compiler warnings remain (deprecated GTK2 functions)
- Ready for testing and final integration

## Known Issues and Solutions

### ✅ SOLVED: FFTW3 Header Path Issues
- **Issue**: FFTW3 library found but headers not included in compilation
- **Solution**: Modified configure.ac to include FFTW3_CFLAGS in FFTWHDR variable
- **Result**: fftw3.h headers now properly detected and included

### ✅ SOLVED: PulseAudio Header Path Issues  
- **Issue**: PulseAudio library found but headers not included in compilation
- **Solution**: Modified configure.ac to include PULSEAUDIO_CFLAGS in PAHDR variable
- **Result**: pulse/simple.h headers now properly detected and included

### ✅ SOLVED: Meschach Library Build Issues
- **Issue**: The embedded Meschach library has old autotools configuration and compilation errors
- **Solution**: 
  - Updated Makefile.am to automatically patch machine.h after configure runs
  - Enabled STDC_HEADERS, HAVE_PROTOTYPES for modern compiler support
  - Fixed conflicting malloc declarations for macOS compatibility
  - Skip sparse matrix compilation (part3) since GWC doesn't use it
  - Both part1 and part2 now compile successfully

### ✅ SOLVED: Autotools Modernization
- **Issue**: Old configure.in and deprecated autoconf macros
- **Solution**: 
  - Renamed configure.in to configure.ac
  - Removed deprecated macros (AC_HEADER_STDC, AC_PROG_GCC_TRADITIONAL, etc.)
  - Updated to modern autotools syntax

### ✅ SOLVED: GTK Mac Integration Compatibility
- **Issue**: gtk-mac-integration-gtk2 package not available in Homebrew
- **Solution**: Made GTK Mac integration conditional compilation (disabled by default)
- **Impact**: Application builds without Mac-specific menu integration (can be added later)

### ✅ SOLVED: Missing Function Declarations
- **Issue**: Undefined warning() function in tap_reverb.c
- **Solution**: Added static function declaration
- **Result**: All source files compile without errors

### ⚠️ REMAINING: Compiler Warnings
- **Issue**: Deprecated GTK2 function warnings (GTypeDebugFlags, GTimeVal)
- **Impact**: Warnings only, does not prevent compilation
- **Solution**: These are expected for GTK2 applications and can be ignored

## Build Variants

### Debug Build
```bash
./configure --enable-pa --enable-debug
make
```

### Release Build
```bash
./configure --enable-pa
make
```

## Testing the Application

After successful compilation:

```bash
# Test basic functionality
./gtk-wave-cleaner --help

# Load an audio file (if available)
./gtk-wave-cleaner /path/to/audio/file.wav
```

## Troubleshooting

### PulseAudio Issues
If audio doesn't work:
```bash
# Check PulseAudio status
brew services list | grep pulseaudio

# Restart PulseAudio
brew services restart pulseaudio

# Test PulseAudio
pulseaudio --check -v
```

### GTK Issues
If the GUI doesn't display properly:
```bash
# Check GTK installation
pkg-config --modversion gtk+-2.0

# Test basic GTK
gtk-demo  # if available
```

### Build Errors
Common solutions:
- Run `autoconf` to regenerate configure script after modifying configure.ac
- Clean build: `make distclean && autoconf && ./configure --enable-pa`
- Check pkg-config paths: `pkg-config --list-all | grep -E "(gtk|fftw|pulse)"`

### Header Not Found Errors
If you get "file not found" errors for headers:
- Verify package installation: `brew list | grep -E "(gtk|fftw|pulseaudio)"`
- Check pkg-config detection: `pkg-config --cflags gtk+-2.0 fftw3 libpulse-simple`

## Development Notes

This build process addresses several modernization challenges:
- Updating 20+ year old autotools configuration
- Fixing deprecated API usage and missing headers
- Resolving audio backend compatibility
- Making GTK Mac integration optional
- Fixing legacy library compilation (Meschach from 1994)

Each major fix has been implemented with proper conditional compilation to maintain compatibility.

## Progress Commits

The following major changes have been implemented:

1. **Modernize autotools configuration** 
   - Update configure.in → configure.ac
   - Remove deprecated macros (AC_HEADER_STDC, AC_PROG_GCC_TRADITIONAL, AC_TYPE_SIGNAL)
   - Use modern autotools syntax

2. **Fix FFTW3 detection and headers**
   - Implement pkg-config detection for FFTW3
   - Include FFTW3_CFLAGS in compilation flags
   - Support both single and double precision

3. **Fix Meschach library compilation**
   - Automatic machine.h patching for ANSI C support
   - Fix malloc declaration conflicts on macOS
   - Enable modern compiler compatibility

4. **Fix PulseAudio headers**
   - Include PULSEAUDIO_CFLAGS in compilation flags
   - Ensure pulse/simple.h is found during compilation

5. **Make GTK Mac integration optional**
   - Conditional compilation for GTK Mac integration
   - Remove dependency on unavailable gtk-mac-integration-gtk2
   - Maintain basic functionality without Mac-specific features

6. **Add missing function declarations**
   - Fix undefined warning() function in tap_reverb.c
   - Resolve type compatibility issues

## Current Status

✅ **Autotools modernization** - Complete  
✅ **FFTW3 detection and headers** - Complete  
✅ **Meschach compilation** - Complete  
✅ **PulseAudio headers** - Complete  
✅ **GTK Mac integration compatibility** - Complete (optional)  
✅ **Missing function declarations** - Complete  
✅ **Main GWC compilation** - Complete with warnings only  
⏳ **Application testing** - Ready for testing  
⏳ **Audio backend verification** - Pending user testing

The application should now compile successfully on macOS with Homebrew dependencies.
