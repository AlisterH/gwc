# Gtk Wave Cleaner (GWC) 0.22

Gtk Wave Cleaner is a GUI application to remove noise (hiss, pops, and clicks) from audio files in WAV and similar formats.

## Requirements
- A *nix based operating system (Linux, BSD, macOS, etc; on 64bit Windows 10/11 you can use Microsoft's "Windows Subsystem for Linux", with the pulseaudio backend)
- GTK2 ([www.gtk.org](https://www.gtk.org/))
- libsndfile ([www.mega-nerd.com/libsndfile/](http://www.mega-nerd.com/libsndfile/))
- OSS, ALSA, Pulse Audio, or Core Audio sound drivers (Core Audio recommended on macOS)
- FFTW libs ([www.fftw.org](http://www.fftw.org/))
- [Optional] Perl (for `gwcbatch` helper script)
- [Optional] perldoc (to view `gwcbatch` usage)
- [Optional] xdg-open (for help menu manual, not required on macOS)
- [Optional] vorbis-tools and lame (for ogg and mp3 export)

## License
The source code and all associated files are freely available under the GNU General Public License (GPL).

## Installation

### Linux/Unix
1. Extract the release source tarball: `tar -xvzf <...>`
2. Enter the directory created.
3. Run `autoreconf -i` (not needed for release tarballs)
4. Run `./configure`
5. Run `make`
6. Run `make install`

### macOS

#### Automated Build (Recommended)

**Prerequisites:**  
Homebrew is required to install dependencies. If you don't have it, install Homebrew from [https://brew.sh](https://brew.sh).

To build and package GWC automatically, run:
```sh
./contrib/macosx/scripts/all.sh
```
This script will install all dependencies, build the application, and create a distributable app bundle.

#### Step-by-step Build

1. **Install dependencies:**
    ```sh
    ./contrib/macosx/scripts/install.sh
    ```
2. **Configure and build:**
    ```sh
    ./contrib/macosx/scripts/build.sh
    ```
3. **Create the app bundle:**
    ```sh
    ./contrib/macosx/scripts/app.sh
    ```

The finished app bundle will be located at `osx_packaging/Gtk Wave Cleaner.app`, and a disk image will be created for easy installation.

For more information, see:
- `contrib/macosx/README.md`

#### Notes for macOS Compilation
- Native menu bar, dock, and window management are supported via gtk-mac-integration.
- Core Audio is enabled by default for optimal macOS audio performance.
- All dependencies are managed via Homebrew.
- The app bundle is fully self-contained and ready for distribution.

## Additional Options
- Run `./configure --help` for additional compile options.
- By default, the GWC binary is installed in `/usr/local/bin/` and documentation in `/usr/local/share/doc/gtk-wave-cleaner`.
- Override install location with `./configure --prefix=/usr` or similar.
- If you have playback problems, try disabling ALSA (`--disable-alsa`) to use OSS. In an ALSA environment, you can use it with `aoss` if necessary.
- Distributions may want to enable PulseAudio (`--enable-pa`).
- On macOS, Core Audio is enabled by default and works reliably.
- Note: mp3 and ogg reading support are currently still broken.

If you have problems installing, check whether they are documented in the `INSTALL` file.

## Instructions for Use
Check out the help documentation included in this distribution and available from the help menu in GWC.

## Known Issues
1. GWC fails to open wav files with metadata, such as those created by recent versions of ffmpeg. GWC will produce an error like:

```
Failed to open /root/whistle.wav, 'Error : Cannot open file in read/write mode due to string data in header.'
```

Libsndfile does not support RDWR mode for these files. If you are creating them using ffmpeg, try like this `ffmpeg -i input.wav -bitexact output.wav` (note that this is not the same as `ffmpeg -bitexact -i input.wav output.wav` or `ffmpeg -i input.wav -flags bitexact output.wav`), or write to a different libsndfile supported format such as `.au` or `.aiff`. A workaround for existing files is to open and save the file in mhwaveedit, or convert to a different format using `sndfile-convert` or `ffmpeg`, or use `SimplifyWave` from waveutils or `shntool strip` to make a copy of the file with a clean header.

2. Mono files look like stereo files, because the waveform is displayed twice.

3. On some systems (very old Mac with default coreaudio backend) GWC may fail to play files with unusual sample rates (typically 44,100Hz and 48,000Hz should be fine).

## Background
For links and a brief presentation describing the technical aspects of the audio restoration methods used in GWC, visit [http://gwc.sourceforge.net/](http://gwc.sourceforge.net/)

Contact: jeff@redhawk.org
