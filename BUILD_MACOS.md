# Building GTK Wave Cleaner on macOS

This document describes the steps needed to build GTK Wave Cleaner on macOS using Homebrew.

## Overview

GTK Wave Cleaner (GWC) is a GTK2-based audio noise removal application originally designed for Linux. To build it on macOS, we need to:

1. Install required Homebrew packages
2. Adjust build configuration for macOS
3. Fix audio backend selection (use PulseAudio instead of broken CoreAudio)
4. Add GTK2 macOS integration
5. Fix Meschach library compilation issues
6. Update autotools configuration

## Prerequisites

- macOS 10.14 or later
- Xcode Command Line Tools: `xcode-select --install`
- Homebrew: https://brew.sh

## Step 1: Install Required Homebrew Packages

```bash
# Core dependencies
brew install autoconf automake libtool pkg-config

# GTK2 and related libraries
brew install gtk+ gtk-mac-integration

# Audio libraries
brew install libsndfile pulseaudio

# Math and signal processing libraries
brew install fftw

# Optional dependencies for additional formats
brew install vorbis-tools lame
```

## Step 2: Audio Backend Configuration

The original CoreAudio backend is broken. We use PulseAudio instead:

```bash
# Start PulseAudio daemon (required for audio playback)
brew services start pulseaudio
```

## Step 3: Build Process

```bash
# Generate build files
autoreconf -fiv

# Configure with PulseAudio and macOS-specific settings
./configure --enable-pa --prefix=/usr/local

# Build
make

# Install
make install
```

## Known Issues and Solutions

### 1. Meschach Library Build Issues
- The embedded Meschach library has old autotools configuration
- Solution: Update Meschach autotools or use system-provided alternative

### 2. GTK2 macOS Integration
- Need proper GTK Mac Integration for native macOS behavior
- Menu bar integration and native dialogs

### 3. Audio Backend Selection
- CoreAudio backend is broken
- PulseAudio is the recommended solution

### 4. Compiler Warnings
- Old C code may generate warnings on modern compilers
- Solution: Update code to use modern C standards

## Build Variants

### Debug Build
```bash
./configure --enable-pa --enable-debug --prefix=/usr/local
make
```

### Release Build
```bash
./configure --enable-pa --prefix=/usr/local
make
```

## Troubleshooting

### PulseAudio Issues
If audio doesn't work:
```bash
# Check PulseAudio status
brew services list | grep pulseaudio

# Restart PulseAudio
brew services restart pulseaudio
```

### GTK Issues
If the GUI doesn't display properly:
```bash
# Check GTK installation
pkg-config --modversion gtk+-2.0

# Verify GTK Mac Integration
pkg-config --modversion gtk-mac-integration-gtk2
```

### Build Errors
Common solutions:
- Run `autoreconf -fiv` to regenerate autotools files
- Clean build: `make distclean && autoreconf -fiv && ./configure --enable-pa`
- Check pkg-config paths: `export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH`

## Development Notes

This build process addresses several modernization challenges:
- Updating 20+ year old autotools configuration
- Fixing deprecated API usage
- Resolving audio backend compatibility
- Adding proper macOS integration

Each major fix should be committed separately for maintainability.
