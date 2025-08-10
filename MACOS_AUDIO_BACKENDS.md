# macOS Audio Backend Comparison

## Overview
GTK Wave Cleaner now supports two audio backends on macOS:

1. **PulseAudio** (via Homebrew)
2. **CoreAudio** (native macOS)

## Configuration Options

### PulseAudio Backend (Recommended)
```bash
./configure --enable-pa
```
- **Pros:**
  - Cross-platform consistency
  - Well-tested on Linux
  - Active audio processing without service issues
  - More reliable for audio editing tasks
  
- **Cons:**
  - Requires PulseAudio service to be running
  - Additional dependency via Homebrew
  - May conflict with other macOS audio applications

### CoreAudio Backend (Native)
```bash
./configure
# (CoreAudio is default on macOS when PulseAudio not explicitly enabled)
```
- **Pros:**
  - Native macOS audio system
  - No additional dependencies
  - Direct system audio integration
  
- **Cons:**
  - Original developers warned: "This currently does not seem to work"
  - Less tested than PulseAudio version
  - May have limitations with certain audio operations

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
Use **PulseAudio backend** with:
```bash
brew install pulseaudio
./configure --enable-pa
make
```

### For System Integration
Use **CoreAudio backend** if you prefer no additional dependencies:
```bash
./configure
make
```

### Audio Service Setup (PulseAudio)
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
