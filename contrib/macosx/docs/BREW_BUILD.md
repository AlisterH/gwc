# Homebrew Build Scripts for GTK Wave Cleaner

This directory contains automated Homebrew-based build scripts for GTK Wave Cleaner on macOS.

## Quick Start

**One-command setup** (recommended for new users):
```bash
./brew-setup-complete.sh
```

This single script will:
1. Install all Homebrew dependencies
2. Configure and build GTK Wave Cleaner
3. Create a distributable macOS app bundle

## Individual Scripts

### 1. `brew-install-deps.sh`
Installs all required Homebrew dependencies:
- **Build tools**: autoconf, automake, libtool, pkg-config
- **GUI framework**: GTK2 (`gtk+`)
- **Audio libraries**: libsndfile
- **Signal processing**: FFTW3
- **Optional formats**: vorbis-tools, lame

**Usage:**
```bash
./brew-install-deps.sh
```

### 2. `brew-build.sh`
Configures and builds GTK Wave Cleaner with proper Homebrew paths:
- Sets up PKG_CONFIG_PATH for Homebrew
- Handles both Intel and Apple Silicon Macs
- Configures with CoreAudio backend (native macOS audio)
- Builds with parallel compilation

**Usage:**
```bash
./brew-build.sh
```

### 3. `brew-setup-complete.sh`
Complete automation that runs both scripts above plus creates app bundle:
- Runs dependency installation
- Builds the application
- Creates macOS app bundle for distribution

**Usage:**
```bash
./brew-setup-complete.sh
```

## Architecture Support

These scripts automatically detect and configure for:
- **Intel Macs**: Uses `/usr/local` Homebrew paths
- **Apple Silicon Macs**: Uses `/opt/homebrew` paths

## Audio Backend

The build is configured to use **CoreAudio** (native macOS audio) by default:
- ✅ **CoreAudio**: Native macOS audio - **recommended and working**
- ❌ **PulseAudio**: Disabled by default (requires additional setup)

## Build Output

After successful build, you'll have:
- `gtk-wave-cleaner` - Command-line executable
- `GTK Wave Cleaner.app` - macOS app bundle (if app bundle script available)

## Requirements

- **macOS**: 10.14 or later
- **Xcode Command Line Tools**: `xcode-select --install`
- **Homebrew**: https://brew.sh

## Troubleshooting

### Missing Dependencies
If you see dependency errors, run:
```bash
./brew-install-deps.sh
```

### Build Configuration Issues
The scripts automatically handle:
- Homebrew path detection
- PKG_CONFIG_PATH setup
- Architecture-specific configurations
- FFTW3 header location

### Audio Issues
- The build uses CoreAudio which works natively on macOS
- No additional audio system setup required
- Test audio output with any WAV file

## Advanced Usage

### Manual Build with Custom Options
```bash
# Install dependencies first
./brew-install-deps.sh

# Then configure manually
export PKG_CONFIG_PATH="$(brew --prefix)/lib/pkgconfig:$PKG_CONFIG_PATH"
./configure --enable-coreaudio --disable-pulseaudio
make
```

### Creating App Bundle Only
If you already have the executable built:
```bash
./create_macos_app.sh  # If available
```

## What's Different from Manual Build

These Homebrew scripts provide:
1. **Automated dependency management** - No manual package hunting
2. **Proper path configuration** - Handles Homebrew's complex path structure
3. **Architecture detection** - Works on both Intel and Apple Silicon
4. **CoreAudio optimization** - Native macOS audio configured correctly
5. **Parallel compilation** - Faster builds using all CPU cores
6. **Error handling** - Clear error messages and validation

## Distribution

The created app bundle includes all dependencies and can be distributed to other Macs without requiring recipients to install Homebrew or any dependencies.

---

**💡 Tip**: For development work, use the individual scripts. For one-time builds or distribution, use `brew-setup-complete.sh`.
