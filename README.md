# GTK Wave Cleaner - macOS Port

**Professional audio restoration software now fully working on macOS!** 🎵

![Version](https://img.shields.io/badge/version-0.22-blue)
![Platform](https://img.shields.io/badge/platform-macOS-green)
![Status](https://img.shields.io/badge/status-production%20ready-brightgreen)

## 🎯 Quick Start

**One command installation:**
```bash
./brew-setup-complete.sh
```

This will automatically:
- Install all Homebrew dependencies
- Build GTK Wave Cleaner
- Create a distributable macOS app bundle

## ✨ What's New in macOS Port

- ✅ **Audio output fixed** - CoreAudio backend now works perfectly
- ✅ **Professional app bundle** - Complete .app with all dependencies
- ✅ **Automated build** - One-command Homebrew installation
- ✅ **Native icons** - High-quality .icns generated from project assets
- ✅ **Modern compatibility** - Works on Intel and Apple Silicon Macs

## 📋 Requirements

- macOS 10.14 or later
- Xcode Command Line Tools: `xcode-select --install`
- Homebrew: https://brew.sh

## 🚀 Build Options

### Automated (Recommended)
```bash
./brew-setup-complete.sh     # Everything automated
```

### Step by Step
```bash
./contrib/macosx/scripts/brew-install-deps.sh       # Install dependencies
./contrib/macosx/scripts/brew-build.sh              # Build application
./contrib/macosx/scripts/create_macos_app.sh        # Create app bundle
```

### Manual
```bash
autoreconf -fiv
./configure --enable-coreaudio
make
```

## 📖 Documentation

- **[BUILD_MACOS.md](contrib/macosx/docs/BUILD_MACOS.md)** - Detailed macOS build guide
- **[BREW_BUILD.md](contrib/macosx/docs/BREW_BUILD.md)** - Homebrew automation documentation
- **[PROJECT_SUMMARY.md](PROJECT_SUMMARY.md)** - Complete technical details

## 🎵 About GTK Wave Cleaner

GTK Wave Cleaner is a professional audio restoration application that removes:
- Background hiss and noise
- Clicks and pops
- Digital artifacts
- Audio imperfections

Originally designed for Linux, this macOS port brings full functionality to Mac users with native audio support.

## 📦 Distribution

The build process creates:
- **App Bundle**: `osx_packaging/Gtk Wave Cleaner.app`
- **Disk Image**: `GWC-YYYYMMDD.dmg`
- **Standalone**: All dependencies bundled

Perfect for distribution to end users who don't need developer tools.

---

**Ready to clean up your audio? Get started with `./brew-setup-complete.sh`!** 🎧
