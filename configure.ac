dnl Process this file with autoconf to produce a configure script.
AC_INIT([Gtk Wave Cleaner], [0.22-07], [http://gwc.sourceforge.net])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([1.12 foreign no-define tar-ustar])

dnl Checks for programs.
AC_PROG_CC
AC_CANONICAL_HOST

dnl check for osx platform
AC_MSG_CHECKING([for Mac OS X platform])
case "$host_os" in
  darwin*)
    platform_osx=yes
    ;;
  *)
    platform_osx=no
    ;;
esac
AC_MSG_RESULT([$platform_osx])

PKG_PROG_PKG_CONFIG

PKG_CHECK_MODULES(SNDFILE, sndfile >= 1.0.2,
                   HAVE_LIBSNDFILE1="yes", HAVE_LIBSNDFILE1="no")

if test "x$HAVE_LIBSNDFILE1" = xno ; then
    AC_MSG_ERROR([
Libsndfile 1.0.2 or greater not found
])
fi

AC_ARG_ENABLE(debug,
AS_HELP_STRING([--enable-debug],
               [enable debugging, default: no]),
[case "${enable_debug}" in
             yes) debug=true ;;
             no)  debug=false ;;
             *)   AC_MSG_ERROR([bad value ${enable_debug} for --enable-debug]) ;;
esac],
[debug=false])

AM_CONDITIONAL(DEBUG, test x"$debug" = x"true")

dnl ------------------------------------------------------------
dnl Strong debug policy:
dnl  - If --enable-debug: strip any -O* from flags and force -g3 -O0
dnl  - Else: add -O2 only if user didn't specify -O*
dnl ------------------------------------------------------------

AS_IF([test "x$debug" = "xtrue"], [
  AC_MSG_NOTICE([Debugging enabled: forcing -g3 -O0 (overriding any -O*)])
  AC_DEFINE([DEBUG], [1], [Define for debug builds])

  dnl Strip any existing optimisation flags from CFLAGS/CXXFLAGS
  CFLAGS=`echo " $CFLAGS " | sed -E 's/[[:space:]]-O([0-9]|s|g|fast|z)?[[:space:]]/ /g; s/^ //; s/ $//'`
  CXXFLAGS=`echo " $CXXFLAGS " | sed -E 's/[[:space:]]-O([0-9]|s|g|fast|z)?[[:space:]]/ /g; s/^ //; s/ $//'`

  dnl Optional: strip existing -g* so you don't end up with -g -g3 (harmless but messy)
  CFLAGS=`echo " $CFLAGS " | sed -E 's/[[:space:]]-g([0-9])?[[:space:]]/ /g; s/^ //; s/ $//'`
  CXXFLAGS=`echo " $CXXFLAGS " | sed -E 's/[[:space:]]-g([0-9])?[[:space:]]/ /g; s/^ //; s/ $//'`

  dnl Force debug flags
  CFLAGS="$CFLAGS -g3 -O0"
  CXXFLAGS="$CXXFLAGS -g3 -O0"

  dnl Optional (often helpful for stack traces/backtraces)
  dnl CFLAGS="$CFLAGS -fno-omit-frame-pointer"
  dnl CXXFLAGS="$CXXFLAGS -fno-omit-frame-pointer"

], [
  dnl Non-debug: only add -O2 if user didn't specify any -O*
  AS_CASE([" $CFLAGS "],
    [*" -O"*], [:],
    [CFLAGS="$CFLAGS -O2"])
])

dnl Checks for libraries.

dnl Replace `main' with a function in -lm:
AC_CHECK_LIB(m, cos, [M_LIBS="-lm"], [M_LIBS=""])

dnl setup macros variables to be replaced in Makefile.in
AC_SUBST(GTK_MAC_CFLAGS)
AC_SUBST(GTK_MAC_LIBS)
AC_SUBST(M_LIBS)
AC_SUBST(PALIB)
AC_SUBST(PAHDR)
AC_SUBST(ALSALIB)
AC_SUBST(ALSADEF)
AC_SUBST(ALSACFLAGS)
AC_SUBST(OGGLIB)
AC_SUBST(OGGHDR)
AC_SUBST(MP3LIB)
AC_SUBST(MP3HDR)

dnl enable ogg/vorbis libs on user request and check installation
OGGLIB=""
OGGHDR=""

AC_ARG_ENABLE(ogg, [  --enable-ogg           compile experimental Ogg file reading support (default disabled)])

AC_MSG_CHECKING([for ogg])
if test "$enable_ogg" = "yes"
then
    ogg="yes";OGGLIB="-lvorbis -logg -lvorbisfile";OGGHDR="-DHAVE_OGG"
else
    ogg="disabled"
fi
AC_MSG_RESULT($ogg)

dnl enable mp3 libs on user request and check installation
MP3LIB=""
MP3HDR=""

AC_ARG_ENABLE(mp3, [  --enable-mp3           compile experimental mp3 file reading support (default disabled)])

AC_MSG_CHECKING([for mp3])
if test "$enable_mp3" = "yes"
then
    mp3="yes";MP3LIB="-lmpg123";MP3HDR="-DHAVE_MP3"
else
    mp3="disabled"
fi
AC_MSG_RESULT($mp3)

dnl enable alsa on user request and check installation
ALSALIB=""
ALSADEF=""
alsa="enabled"

AC_ARG_ENABLE(alsa, [  --disable-alsa           disable ALSA support (default enabled)])

if test "$enable_alsa" != "no"
then
    PKG_CHECK_MODULES(ALSA, alsa >= 0.9.0, [
		alsa="yes"
		ALSALIB=$ALSA_LIBS
		ALSADEF="-DHAVE_ALSA"
		ALSACFLAGS=$ALSA_CFLAGS
		AC_DEFINE([HAVE_ALSA], [1], [Define if ALSA support is enabled])
	], [
		alsa="disabled"
	])
else
    alsa="disabled"
    AC_MSG_CHECKING([for ALSA])
    AC_MSG_RESULT($alsa)
fi

AM_CONDITIONAL([HAVE_ALSA], [test "x$alsa" = "xyes"])

dnl enable pulseaudio on user request and check installation
PALIB=""
PAHDR=""

AC_ARG_ENABLE(pa,
  AS_HELP_STRING([--enable-pa],
                 [compile PulseAudio support (default disabled)]),
  [enablepa="$enableval"],
  [enablepa="no"])

if test "x$enablepa" = "xyes"; then
  PKG_CHECK_MODULES([PULSEAUDIO], [libpulse-simple >= 0.9.0], [
    PALIB="$PULSEAUDIO_LIBS"
    PAHDR="-DHAVE_PULSE_AUDIO $PULSEAUDIO_CFLAGS"
    pa="yes"
    AC_DEFINE([HAVE_PULSE_AUDIO], [1], [Define if PulseAudio support is enabled])

    alsa="disabled"
    ALSALIB=""
    ALSADEF=""
  ], [
    AC_MSG_ERROR([PulseAudio support requested but libpulse-simple was not found])
  ])
else
  pa="disabled"
fi

AM_CONDITIONAL([HAVE_PULSE_AUDIO], [test "x$pa" = "xyes"])

dnl ------------------------------------------------------------
dnl FFTW3 (single or double precision)
dnl ------------------------------------------------------------

AC_ARG_WITH([fftw-precision],
  AS_HELP_STRING([--with-fftw-precision=single|double],
                 [choose FFTW3 precision (default: double)]),
  [fftw_precision="$withval"],
  [fftw_precision="double"])

FFTW_LIBS=""
FFTW_CFLAGS=""

AS_IF([test "x$fftw_precision" = "xsingle"], [
  PKG_CHECK_MODULES([FFTW], [fftw3f >= 3.0.0], [
    AC_DEFINE([HAVE_FFTW3], [1], [Have FFTW3])
    AC_DEFINE([FFTWPREC], [1], [FFTW precision: single])
  ], [
    AC_MSG_ERROR([Single-precision FFTW3 (fftw3f) not found])
  ])
], [
  PKG_CHECK_MODULES([FFTW], [fftw3 >= 3.0.0], [
    AC_DEFINE([HAVE_FFTW3], [1], [Have FFTW3])
    AC_DEFINE([FFTWPREC], [0], [FFTW precision: double])
  ], [
    AC_MSG_ERROR([Double-precision FFTW3 (fftw3) not found])
  ])
])

AC_SUBST([FFTW_LIBS])
AC_SUBST([FFTW_CFLAGS])


dnl Checks for header files.
AC_CHECK_HEADERS([fcntl.h limits.h malloc.h sys/ioctl.h sys/time.h unistd.h])

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T

dnl ------------------------------------------------------------
dnl Endianness detection (canonical, with PowerPC macOS fallback)
dnl ------------------------------------------------------------
AC_C_BIGENDIAN(
  [AC_DEFINE([WORDS_BIGENDIAN], [1], [Define if the system is big-endian])],
  [],
  [case "$host_cpu" in
     powerpc*) AC_DEFINE([WORDS_BIGENDIAN], [1], [Define if the system is big-endian]) ;;
   esac]
)

dnl Checks for library functions.
AC_CHECK_FUNCS(gettimeofday)

dnl refer to comments in gwc.c where these versions are required:
PKG_CHECK_MODULES(GTK, [ gtk+-2.0 >= 2.16 glib-2.0 >= 2.30 ])

if test x$platform_osx = xyes; then
  AC_DEFINE([MAC_OS_X], [1], [Define if building on macOS])
  
dnl CoreAudio / AudioUnit detection
AC_ARG_ENABLE([audiounit],
  AS_HELP_STRING([--enable-audiounit], [enable AudioUnit backend (default yes)]),
  [case "${enableval}" in
       yes) enable_audiounit=yes ;;
       no) enable_audiounit=no ;;
       *) AC_MSG_ERROR([bad value ${enableval} for --enable-audiounit]) ;;
   esac],
  [enable_audiounit=yes])

AC_MSG_CHECKING([for AudioUnit/AudioUnit.h])
AC_CHECK_HEADER([AudioUnit/AudioUnit.h], [have_audiounit=yes], [have_audiounit=no])
AC_MSG_RESULT([$have_audiounit])

  dnl Check for gtk-mac-integration
  PKG_CHECK_MODULES(GTK_MAC, [gtk-mac-integration-gtk2 >= 2.0.0],
                   [HAVE_GTK_MAC="yes"], [HAVE_GTK_MAC="no"])
  
if test "x$HAVE_GTK_MAC" = xyes ; then
  AC_DEFINE([HAVE_GTK_MAC_INTEGRATION], [1], [Define if gtk-mac-integration is available])
  GTK_CFLAGS="$GTK_CFLAGS $GTK_MAC_CFLAGS"
  for mac_lib in $GTK_MAC_LIBS; do
    case " $GTK_LIBS " in
      *" $mac_lib "*) : ;;  # already in GTK_LIBS
      *) GTK_LIBS="$GTK_LIBS $mac_lib" ;;
    esac
  done
  AC_MSG_NOTICE([GTK Mac Integration found - enabling native macOS integration])
  else
    AC_MSG_NOTICE([GTK Mac Integration not found - building without native macOS integration])
  fi
  
  if test "$enablepa" = "yes"; then
    # pulseaudio is enabled
    OSXLIBS="$LIBS"

  else

    if test "x$have_audiounit" = "xyes" && test "x$enable_audiounit" = "xyes"; then
      AC_DEFINE([USE_LEGACY_HAL], [0], [Use legacy HAL if 1, AudioUnit if 0])
      AC_MSG_NOTICE([
*****************************!!!!!!! INFORMATION !!!!!!!*****************************
You are building on Apple OSX without PulseAudio; Using AudioUnit backend.
*************************************************************************************
])
      OSXLIBS="$LIBS -framework AudioUnit -framework AudioToolbox -framework CoreAudio"
    else
      AC_DEFINE([USE_LEGACY_HAL], [1], [Use legacy HAL if 1, AudioUnit if 0])
      AC_MSG_NOTICE([
*****************************!!!!!!! INFORMATION !!!!!!!*****************************
You are building on Apple OSX without PulseAudio; Using legacy CoreAudio HAL backend.
*************************************************************************************
    ])
      OSXLIBS="$LIBS -framework CoreAudio"
    fi
  fi
  AC_SUBST(OSXLIBS)
fi

AM_CONDITIONAL([MAC_OS_X], [test "x$platform_osx" = "xyes"])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
