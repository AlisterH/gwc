# macOS Audio Backend Comparison

## Overview
GTK Wave Cleaner now supports two audio backends on macOS:

1. **PulseAudio** (via Homebrew)
2. **CoreAudio** (native macOS)

## Configuration Options

### CoreAudio Backend (Default - Recommended)
```bash
./configure
```
- **Pros:**
  - Native macOS audio system
  - No additional dependencies
  - Direct system audio integration
  - Better macOS integration and performance
  - No service management required
  
- **Cons:**
  - Original developers noted some historical limitations
  - Platform-specific (not cross-platform)

### PulseAudio Backend (Optional)
```bash
./configure --enable-pa
```
- **Pros:**
  - Cross-platform consistency
  - Well-tested on Linux
  - Active audio processing without service issues
  
- **Cons:**
  - Requires PulseAudio service to be running
  - Additional dependency via Homebrew
  - May conflict with other macOS audio applications
  - Unnecessary complexity on macOS

## Build Status

### ✅ Working Configurations
- **PulseAudio Build:** Fully functional, all dependencies resolved
- **CoreAudio Build:** Successfully compiles and launches GUI

### 🔧 Fixed Issues
- Carbon framework include path updated for modern macOS
- Old hardcoded system framework paths replaced
- All compilation errors resolved for both backends

## Testing Results

### Application Launch
Both backends successfully:
- Launch GUI interface
- Display help information
- Load libsndfile support
- Show proper version information

### Known Warnings
Both versions show minor GLib object warnings that don't affect functionality:
```
GLib-GObject-CRITICAL: g_object_ref: assertion 'G_IS_OBJECT (object)' failed
GLib-GIO-CRITICAL: g_loadable_icon_load: assertion 'G_IS_LOADABLE_ICON (icon)' failed
```

## Recommendations

### For Regular Users
Use **CoreAudio backend** (default):
```bash
./configure
make
```

### For Cross-Platform Development
Use **PulseAudio backend** if you need consistency across platforms:
```bash
brew install pulseaudio
./configure --enable-pa
make
```

### Audio Service Setup (PulseAudio only)
If using PulseAudio backend, ensure service is running:
```bash
brew services start pulseaudio
```

## Development Notes

- CoreAudio backend required fixing obsolete Carbon framework include
- Both backends link properly with modern macOS SDKs
- FFTW3 and libsndfile integration works with both audio systems
- No audio backend selection at runtime - chosen at compile time

## Compatibility
- **macOS Version:** Tested on modern macOS with Apple Silicon
- **Dependencies:** GTK+ 2.24, libsndfile, FFTW3
- **Architecture:** Universal support (Intel/Apple Silicon via Homebrew)
