# Alister: are we finally letting autotools do enough for us? Please report any remaining build issues
# Note that Meschach uses a legacy build system, so isn't properly integrated - autoconf or autoreconf don't touch it, and shouldn't (also see note in meschach/configure - does building in meschach still not work in our version?).

# Note if we contemplate updating meschach or using an external meschach library we need to check what Jeff did to our version:
# - Jeff said he fixed a bad memory leak (I *think* this is also fixed in the debian version).
# - Jeff made a change to err.c, which I have commented
# - Anything else?

APPNAME = gtk-wave-cleaner

AM_CFLAGS = -I$(srcdir)/ -D_FILE_OFFSET_BITS=64 -Wall @GTK_CFLAGS@ @SNDFILE_CFLAGS@

AM_CPPFLAGS = -I$(top_srcdir)/meschach -I$(top_builddir)/meschach -I$(top_srcdir) -I$(top_builddir) \
  -DHAVE_CONFIG_H @FFTW_CFLAGS@ \
  -DDATADIR=\"$(datadir)\" \
  -DHELPDIR=\"$(datadir)/doc/$(APPNAME)\" \
  -DLIBDIR=\"$(libdir)\" \
  -DAPPNAME=\"$(APPNAME)\" \
  @ALSADEF@ @PAHDR@ @OGGHDR@ @MP3HDR@

# Alister: OSX potential issues
# On an old test system I initially couldn't use -lgtkmacintegration as it wasn't building
# properly, so I had to link to the object files.
#OSX_OBJS = /Users/alister/gtk-mac-integration-2.0.8/src/.libs/*.o
# Note depending on your build environment you might need something like -lgtkmacintegration.2
# This is now taken care of by configure.in
#OSX_LIBS = -framework CoreAudio -lgtkmacintegration-gtk2
# Don't need coreaudio if using pulseaudio
#OSX_LIBS = -lgtkmacintegration-gtk2

gtk_wave_cleaner_SOURCES = \
	tap_reverb_file_io.c \
	tap_reverb.c \
	reverb.c \
	dialog.c \
	gwc.c \
	audio_edit.c \
	audio_util.c \
	gtkled.c \
	gtkledbar.c \
	preferences.c \
	drawing.c \
	amplify.c \
	denoise.c \
	undo.c \
	declick.c \
	sample_block.c \
	decrackle.c \
	stat.c \
	dethunk.c \
	i0.c \
	i1.c \
	chbevl.c \
	markers.c \
	encode.c \
	soundfile.c \
	pinknoise.c \
	biquad.c \
	audio_device.h \
	audio_edit.h \
	biquad.h \
	encoding.h \
	fmtheaders.h \
	gtkled.h \
	gtkledbar.h \
	gwc.h \
	mconf.h \
	mp3.h \
	mp3-duration.h \
	reverb_settings.h \
	soundfile.h \
	stat.h \
	tap_reverb.h \
	tap_reverb_common.h \
	tap_reverb_file_io.h \
	icons/declick_m_dark.xpm \
	icons/declick_w.xpm \
	icons/declick_w_dark.xpm \
	icons/decrackle.xpm \
	icons/decrackle_dark.xpm \
	icons/delete.xpm \
	icons/delete_dark.xpm \
	icons/estimate.xpm \
	icons/estimate_dark.xpm \
	icons/filter.xpm \
	icons/filter_dark.xpm \
	icons/gtk-wave-cleaner.xpm \
	icons/noise_sample.xpm \
	icons/noise_sample_dark.xpm \
	icons/pinknoise.xpm \
	icons/pinknoise_dark.xpm \
	icons/remove_noise.xpm \
	icons/remove_noise_dark.xpm \
	icons/select_all.xpm \
	icons/select_all_dark.xpm \
	icons/silence.xpm \
	icons/silence_dark.xpm \
	icons/spectral.xpm \
	icons/spectral_dark.xpm \
	icons/start.xpm \
	icons/start_dark.xpm \
	icons/stop.xpm \
	icons/stop_dark.xpm \
	icons/view_all.xpm \
	icons/view_all_dark.xpm \
	icons/zoom_in.xpm \
	icons/zoom_in_dark.xpm \
	icons/zoom_out.xpm \
	icons/zoom_out_dark.xpm \
	icons/zoom_sel.xpm \
	icons/zoom_sel_dark.xpm \
	icons/amplify.xpm \
	icons/amplify_dark.xpm \
	icons/declick.xpm \
	icons/declick_dark.xpm \
	icons/declick_m.xpm

# Alister: leave out meschach/configure.in, as it makes a broken configure, and someone may try to use it!
# ar.c and ar.h are here as if ar.c is added to gtk_wave_cleaner_SOURCES the build fails, and it seems that we don't actually need to compile it anyway
EXTRA_DIST = contrib/macosx INSTALL ar.c ar.h \
meschach/DOC meschach/MACHINES meschach/arnoldi.c meschach/bdfactor.c meschach/bkpfacto.c meschach/chfactor.c meschach/configure meschach/conjgrad.c meschach/copy.c meschach/copyright meschach/dmacheps.c meschach/err.c meschach/err.h \
meschach/extras.c meschach/fft.c meschach/FILELIST meschach/fmacheps.c meschach/givens.c meschach/hessen.c meschach/hsehldr.c meschach/init.c meschach/iotort.c meschach/iter.h meschach/iter0.c meschach/iternsym.c meschach/itersym.c meschach/itertort.c meschach/ivecop.c meschach/lanczos.c \
meschach/ls.dat meschach/lufactor.c meschach/machine.c meschach/machine.h.in meschach/makefile.in meschach/matlab.c meschach/matlab.h meschach/matop.c meschach/matrix.h meschach/matrix2.h meschach/matrixio.c meschach/maxint.c meschach/meminfo.c meschach/meminfo.h meschach/memory.c meschach/memstat.c \
meschach/memtort.c meschach/mfunc.c meschach/mfuntort.c meschach/norm.c meschach/oldnames.h meschach/otherio.c meschach/pxop.c meschach/qrfactor.c meschach/README meschach/rk4.dat meschach/schur.c meschach/solve.c meschach/sparse.c meschach/sparse.h meschach/sparse2.h meschach/sparseio.c \
meschach/spbkp.c meschach/spchfctr.c meschach/splufctr.c meschach/sprow.c meschach/spswap.c meschach/sptort.c meschach/submat.c meschach/svd.c meschach/symmeig.c meschach/tags meschach/torture.c meschach/tutadv.c meschach/tutorial.c meschach/update.c meschach/vecop.c meschach/version.c \
meschach/zcopy.c meschach/zfunc.c meschach/zgivens.c meschach/zhessen.c meschach/zhsehldr.c meschach/zlufctr.c meschach/zmachine.c meschach/zmatio.c meschach/zmatlab.c meschach/zmatop.c meschach/zmatrix.h meschach/zmatrix2.h meschach/zmemory.c meschach/znorm.c meschach/zqrfctr.c meschach/zschur.c \
meschach/zsolve.c meschach/ztorture.c meschach/zvecop.c

# all this stuff needs to be installed/packaged
bin_PROGRAMS = $(APPNAME)
dist_bin_SCRIPTS = gwcbatch
dist_doc_DATA = doc/gtkrc-example.txt README.md COPYING ChangeLog # Historically Automake automatically included some of these, but not anymore
dist_html_DATA = doc/$(APPNAME).html
dist_desktop_DATA = data/applications/gtk-wave-cleaner.desktop
desktopdir = $(datadir)/applications
# these need to be in the source dist, but not installed/packaged
dist_noinst_DATA = INSTALL TODO

dist_icons_scalable_apps_DATA = data/icons/hicolor/scalable/apps/gtk-wave-cleaner.svg
icons_scalable_appsdir = $(datadir)/icons/hicolor/scalable/apps
dist_icons_16x16_apps_DATA = data/icons/hicolor/16x16/apps/gtk-wave-cleaner.png
icons_16x16_appsdir = $(datadir)/icons/hicolor/16x16/apps
dist_icons_22x22_apps_DATA = data/icons/hicolor/22x22/apps/gtk-wave-cleaner.png
icons_22x22_appsdir = $(datadir)/icons/hicolor/22x22/apps
dist_icons_24x24_apps_DATA = data/icons/hicolor/24x24/apps/gtk-wave-cleaner.png
icons_24x24_appsdir = $(datadir)/icons/hicolor/24x24/apps
dist_icons_32x32_apps_DATA = data/icons/hicolor/32x32/apps/gtk-wave-cleaner.png
icons_32x32_appsdir = $(datadir)/icons/hicolor/32x32/apps
dist_icons_48x48_apps_DATA = data/icons/hicolor/48x48/apps/gtk-wave-cleaner.png
icons_48x48_appsdir = $(datadir)/icons/hicolor/48x48/apps
dist_icons_64x64_apps_DATA = data/icons/hicolor/64x64/apps/gtk-wave-cleaner.png
icons_64x64_appsdir = $(datadir)/icons/hicolor/64x64/apps
dist_icons_128x128_apps_DATA = data/icons/hicolor/128x128/apps/gtk-wave-cleaner.png
icons_128x128_appsdir = $(datadir)/icons/hicolor/128x128/apps
dist_icons_256x256_apps_DATA = data/icons/hicolor/256x256/apps/gtk-wave-cleaner.png
icons_256x256_appsdir = $(datadir)/icons/hicolor/256x256/apps

###
#Alister: I don't think we ever needed OSX_CFLAGS
#COMPILE = $(CC) $(DEFS) $(INCLUDES) $(CPPFLAGS) $(CFLAGS) $(OSX_CFLAGS)

### handy to have around for checking buffer overruns
#EFENCE = -lefence

gtk_wave_cleaner_LDADD = meschach.a @FFTW_LIBS@ \
	@GTK_LIBS@ @SNDFILE_LIBS@ @ALSALIB@ @PALIB@ \
	@OGGLIB@ @MP3LIB@ @OSXLIBS@ @M_LIBS@

BUILT_SOURCES = meschach/machine.h
CLEANFILES = meschach/machine.h meschach.a
MOSTLYCLEANFILES = core

# PulseAudio (highest priority)
if HAVE_PULSE_AUDIO
gtk_wave_cleaner_SOURCES += audio_pa.c
endif

# ALSA (only if PA is not enabled)
if HAVE_ALSA
if !HAVE_PULSE_AUDIO
gtk_wave_cleaner_SOURCES += audio_alsa.c
endif
endif

# macOS CoreAudio (only if PA and ALSA are not enabled)
if MAC_OS_X
if !HAVE_PULSE_AUDIO
gtk_wave_cleaner_SOURCES += audio_osx.c
endif
endif

# OSS fallback
if !HAVE_PULSE_AUDIO
if !HAVE_ALSA
if !MAC_OS_X
gtk_wave_cleaner_SOURCES += audio_oss.c
endif
endif
endif

# Files that depend on meschach headers need meschach.a built first
declick.o dethunk.o stat.o : meschach.a

# Alister: note that we can mostly install to the OSX appdir using something like this:
# make DESTDIR=/tmp/gwc/osx_packaging/Gtk\ Wave\ Cleaner.app/Contents/Resources install
# but the binary won't be in the ...Contents/MacOS directory
# so do this:
# mv /osx_packaging/Gtk\ Wave\ Cleaner.app/Contents/Resources/bin/gtk-wave-cleaner /osx_packaging/Gtk\ Wave\ Cleaner.app/Contents/MacOS/gtk-wave-cleaner.bin
# The .desktop and icon files aren't used, anyway, but at least it installs the docs!
# How do we get it to load the logo in the about dialog from either the icon files or the .icns?

# This supposedly means `make clean` etc still works if someone makes a file called clean
.PHONY: clean distclean maintainer-clean clean-local distclean-local maintainer-clean-local

meschach/machine.h:
	@echo "Configuring bundled meschach"
	@mkdir -p meschach
	cd meschach && \
	  $(abs_srcdir)/meschach/configure --srcdir=$(abs_srcdir)/meschach

meschach.a : meschach/meschach.a
	cp meschach/meschach.a $@

meschach/meschach.a : meschach/machine.h
# Build Meschach without sparse matrix support (part3) since GWC doesn't use it
# and sparse.c has compilation issues with modern compilers
	@echo "Building bundled meschach library"
	$(MAKE) -C meschach  CFLAGS="-I. -I$(abs_srcdir)/meschach -std=c89 -DANSI_C -DHAVE_PROTOTYPES -DPROTOTYPES_IN_STRUCT" part1
	$(MAKE) -C meschach  CFLAGS="-I. -I$(abs_srcdir)/meschach -std=c89 -DANSI_C -DHAVE_PROTOTYPES -DPROTOTYPES_IN_STRUCT" part2

#	@echo "Building bundled meschach (legacy mode)"
#	$(MAKE) -C meschach clean
#	$(MAKE) -C meschach \
#	    CC="$(CC)" \
#	    CFLAGS="-std=c89 -O2 -Wno-implicit-function-declaration" \
#	    part1
#	$(MAKE) -C meschach \
#	    CC="$(CC)" \
#	    CFLAGS="-std=c89 -O2 -Wno-implicit-function-declaration" \
#	    part2

# Use clean-local rather than clean, otherwise `make distclean` doesn't work
clean-local:
	-$(MAKE) -C meschach clean

# don't need this much manual code to clean meschach dir
#meschach-clean:
# Note that meschach/configure.in tells makefile, but supposedly Makefile is standard
#	@if test -f meschach/makefile; then \
#	  echo "Cleaning meschach"; \
#	  (cd meschach && $(MAKE) realclean); \
#	else \
#	  echo "meschach not configured — skipping clean"; \
#	fi

# Use distclean-local rather than distclean
distclean-local:
	-$(MAKE) -C meschach realclean
	rm -rf .deps

maintainer-clean-local:
	rm -f configure aclocal.m4 compile install-sh missing depcomp config.h.in Makefile.in
# WARNING - DO NOT DELETE meschach/configure AS IT IS NOT GENERATED BY AUTOCONF
