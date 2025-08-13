# macOS Support Files

This directory contains all macOS-specific files for GTK Wave Cleaner.

## Directory Structure

- **`docs/`** - macOS build documentation
  - `BUILD_MACOS.md` - Detailed macOS build guide
  - `BREW_BUILD.md` - Homebrew automation documentation
  - `MACOS_AUDIO_BACKENDS.md` - Audio backend information

- **`scripts/`** - Build and setup scripts
  - `brew-install-deps.sh` - Install Homebrew dependencies
  - `brew-build.sh` - Build application using Homebrew
  - `brew-setup-complete.sh` - Complete setup script
  - `complete_app_bundle.sh` - Create complete app bundle
  - `create_macos_app.sh` - Create macOS app bundle
  - `create-mac-icon.sh` - Create macOS application icon
  - `icon-info.sh` - Icon information utility

- **`resources/`** - macOS-specific resources
  - `AppIcon-new.icns` - macOS application icon

- **`osx_packaging/`** - App bundle packaging
  - Contains the complete macOS app bundle structure

- **`audio_osx.c`** - macOS Core Audio implementation

## Quick Start

To build on macOS:

```bash
# From the project root directory
./contrib/macosx/scripts/brew-install-deps.sh  # Install dependencies
./contrib/macosx/scripts/brew-build.sh         # Build application
./contrib/macosx/scripts/create_macos_app.sh   # Create app bundle
```

For detailed instructions, see the documentation in the `docs/` directory.
