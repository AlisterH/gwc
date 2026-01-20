dnl Process this file with autoconf to produce a configure script.
AC_INIT([Gtk Wave Cleaner], [0.22-07], [http://gwc.sourceforge.net])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([1.12 foreign no-define tar-ustar])

PKG_CHECK_MODULES(SNDFILE, sndfile >= 1.0.0,
                   HAVE_LIBSNDFILE1="yes", HAVE_LIBSNDFILE1="no")

if test "x$HAVE_LIBSNDFILE1" = xno ; then
    AC_MSG_ERROR([
Libsndfile 1.0.2 or greater needed, find it at http://www.zip.com.au/~erikd/libsndfile/
OR perhaps configure can not file libsndfile, in which you can try 1 of 3 things:
1) set the environment variable PKG_CONFIG_PATH
   to find the package file for libsndfile for example:
   export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig
2) re-install libsndfile in /usr instead of /usr/local, where it installs by default.
3) There are some packages for libsndfile now, be sure to install libsndfile-devel
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

PKG_CHECK_MODULES(PULSEAUDIO, libpulse-simple >= 0.9.0,
                   HAVE_PULSE_AUDIO="yes", HAVE_PULSE_AUDIO="no")

dnl Checks for programs.
AC_PROG_CC
AC_LANG(C)

dnl Checks for libraries.

dnl Replace `main' with a function in -lm:
AC_CHECK_LIB(m, cos, [M_LIBS="-lm"], [M_LIBS=""])

dnl setup macros variables to be replaced in Makefile.in
AC_SUBST(OSXDEF)
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
AC_SUBST(OSXLIBS)

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
    PKG_CHECK_MODULES(ALSA, alsa >= 0.9.0, alsa="yes";ALSALIB=$ALSA_LIBS;ALSADEF="-DHAVE_ALSA";ALSACFLAGS=$ALSA_CFLAGS, alsa="disabled")
    AC_DEFINE([HAVE_ALSA], [1], [Define if ALSA support is enabled])
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

PALIB=""
PAHDR=""

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

dnl refer to comments in gwc.c where these versions are required:
PKG_CHECK_MODULES(GTK, [ gtk+-2.0 >= 2.16 glib-2.0 >= 2.30 ])

dnl Checks for library functions.
AC_FUNC_VPRINTF
AC_CHECK_FUNCS(gettimeofday)

dnl check for osx platform
AC_MSG_CHECKING([for Mac OS X platform])
case `uname -s` in
  Darwin)
    platform_osx=yes
    ;;
  *)
    platform_osx=no
    ;;
esac
AC_MSG_RESULT([$platform_osx])
if test x$platform_osx = xyes; then
  AC_DEFINE([MAC_OS_X], [1], [Define if building on macOS])
  OSXDEF="-DMAC_OS_X"
  
  dnl Check for gtk-mac-integration
  PKG_CHECK_MODULES(GTK_MAC, [gtk-mac-integration-gtk2 >= 2.0.0],
                   [HAVE_GTK_MAC="yes"], [HAVE_GTK_MAC="no"])
  
  if test "x$HAVE_GTK_MAC" = xyes ; then
    OSXDEF="$OSXDEF -DHAVE_GTK_MAC_INTEGRATION"
    GTK_CFLAGS="$GTK_CFLAGS $GTK_MAC_CFLAGS"
    # Only add GTK_MAC_LIBS that are not already in GTK_LIBS
    GTK_MAC_UNIQUE=""
    for mac_lib in $GTK_MAC_LIBS; do
      # Check if this library is not already in GTK_LIBS
      found=0
      for gtk_lib in $GTK_LIBS; do
        if test "$mac_lib" = "$gtk_lib"; then
          found=1
          break
        fi
      done
      if test $found -eq 0; then
        GTK_MAC_UNIQUE="$GTK_MAC_UNIQUE $mac_lib"
      fi
    done
    GTK_LIBS="$GTK_LIBS$GTK_MAC_UNIQUE"
    AC_MSG_NOTICE([GTK Mac Integration found - enabling native macOS integration])
  else
    AC_MSG_NOTICE([GTK Mac Integration not found - building without native macOS integration])
  fi
  
  if test "$enablepa" = "yes"; then
    # pulseaudio is enabled
    OSXLIBS="$LIBS"
  else
    # pulseaudio is not enabled so we need to build CoreAudio backend instead
    AC_MSG_NOTICE([
***********************!!!!!!! INFORMATION !!!!!!!***********************
You are building on Apple OSX with the CoreAudio backend.
Use --enable-pa if you wish to build with the pulseaudio backend instead.
*************************************************************************
])
    OSXLIBS="$LIBS -framework CoreAudio" 
  fi
fi

AM_CONDITIONAL([MAC_OS_X], [test "x$platform_osx" = "xyes"])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
