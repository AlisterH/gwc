# macOS Support Files

This directory contains all macOS-specific files for GTK Wave Cleaner.

## Directory Structure

- **`docs/`** - macOS build documentation
  - `BUILD_MACOS.md` - Detailed macOS build guide
  - `BREW_BUILD.md` - Homebrew automation documentation
  - `MACOS_AUDIO_BACKENDS.md` - Audio backend information

- **`scripts/`** - Build and setup scripts
  - `all.sh` - Complete automated build (install + build + app)
  - `install.sh` - Install Homebrew dependencies
  - `build.sh` - Build application using Homebrew
  - `app.sh` - Create macOS app bundle
  - `setup.sh` - Complete setup script (deprecated, use all.sh)
  - `bundle.sh` - Create complete app bundle
  - `icon.sh` - Create macOS application icon
  - `info.sh` - Icon information utility

- **`resources/`** - macOS-specific resources
  - `AppIcon-new.icns` - macOS application icon

- **`osx_packaging/`** - App bundle packaging
  - Contains the complete macOS app bundle structure

- **`audio_osx.c`** - macOS Core Audio implementation

## Quick Start

To build on macOS:

**Simple one-command build:**
```bash
./contrib/macosx/scripts/all.sh     # Does everything automatically
```

**Step by step:**
```bash
# From the project root directory
./contrib/macosx/scripts/install.sh  # Install dependencies
./contrib/macosx/scripts/build.sh    # Build application
./contrib/macosx/scripts/app.sh      # Create app bundle
```

For detailed instructions, see the documentation in the `docs/` directory.
