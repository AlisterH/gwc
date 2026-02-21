with (import <nixpkgs> { } );
mkShell {
  # these are things we use to compile from the host platform
  nativeBuildInputs = [
    # AI says don't need autoreconfHook - it is used in a derivation but not a mkShell
    autoconf
    automake
    libtool
    pkg-config
    # We don't need these for building but may want them for testing
    alsa-oss
    apulse
    # Might be useful for padsp instead of alsa-oss on a pulseaudio or pipewire system
    # pulseaudio
  ];

  # these are things we link against, so if we cross-compile they are from the target platform
  buildInputs = [
    alsa-lib
    libpulseaudio
    gtk2
    libsndfile
    fftw
    hicolor-icon-theme
  ];
}
