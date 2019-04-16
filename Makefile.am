# Alister: there are issues with the way this is all set up.
# 1. It is too complicated - we aren't letting autotools do enough for us.
# 2. Make does not recognise if header files have changed and rebuild the necessary objects.
# 3. We haven't properly integrated meschach.  Make here doesn't recognise if any of meschach's header or source files have changed, and autoconf or autoreconf here doesn't touch it. (also see note in meschach/configure - neither autoconf nor building in meschach actually works in our version).
# 4. `make dist` needs to be told to include all of the header and source files, as we haven't specified them as gwc_SOURCES.
# 5. `make clean` errors trying to clean the meschach directory if `make` has not been run :)
# 6. `make install-strip` doesn't strip
# etc

# Note if we contemplate updating meschach or using an external meschach library we need to check what Jeff did to our version:
# - Jeff said he fixed a bad memory leak (I *think* this is also fixed in the debian version).
# - Jeff made a change to err.c, which I have commented
# - Anything else?

prefix = @prefix@
exec_prefix = @exec_prefix@

if DEBUG
AM_CFLAGS = -g3 -O0
AM_CXXFLAGS = -g3 -O0
else
AM_CFLAGS = -O2
AM_CXXFLAGS = -O2
endif

# Alister: OSX potential issues
# On an old test system I initially couldn't use -lgtkmacintegration as it wasn't building
# properly, so I had to link to the object files.
#OSX_OBJS = /Users/alister/gtk-mac-integration-2.0.8/src/.libs/*.o
# Note depending on your build environment you might need something like -lgtkmacintegration.2
# This is now taken care of by configure.in
#OSX_LIBS = -framework CoreAudio -lgtkmacintegration-gtk2
# Don't need coreaudio if using pulseaudio
#OSX_LIBS = -lgtkmacintegration-gtk2

BINDIR = @bindir@
DATADIR = @datadir@
# is this one used for anything?:
DATAROOTDIR = @datarootdir@
LIBDIR = @libdir@
SYSCONFDIR = @sysconfdir@
APPNAME = gtk-wave-cleaner

DOCDIR = $(DATADIR)/doc
# Alister: if you set HELPDIR different to DOCDIR, you will need to add it to the uninstall routine below
HELPDIR = $(DOCDIR)

# use this for SuSE and maybe other distros
#DOCDIR = /usr/share/doc/packages/gtk-wave-cleaner
#HELPDIR = $(DOCDIR)

DEFS = -DDATADIR=\"$(DATADIR)\" -DHELPDIR=\"$(HELPDIR)\" -DLIBDIR=\"$(LIBDIR)\" -DAPPNAME=\"$(APPNAME)\" @OSXDEF@ @ALSADEF@ @PAHDR@ @FFTWHDR@ @FFTWPREC@ @OGGHDR@ @MP3HDR@
CFLAGS = -D_FILE_OFFSET_BITS=64 -Wall @CFLAGS@ @GTK_CFLAGS@ @SNDFILE_CFLAGS@

LIBS= meschach.a @GTK_LIBS@ @SNDFILE_LIBS@ @ALSALIB@ @PALIB@ @FFTWLIB@ @OGGLIB@ @MP3LIB@ @OSXLIBS@ -lm

SRC = tap_reverb_file_io.c tap_reverb.c reverb.c dialog.c gwc.c audio_device.c audio_edit.c audio_util.c gtkled.c gtkledbar.c preferences.c drawing.c amplify.c denoise.c undo.c declick.c sample_block.c decrackle.c stat.c dethunk.c i0.c i1.c chbevl.c markers.c encode.c soundfile.c pinknoise.c biquad.c
OBJS = $(SRC:.c=.o)
# Alister: We should be able to avoid most of this - see item 3 and 4 in the comment at the top
# Alister: leave out meschach/configure.in, as it makes a broken configure, and someone may try to use it!
EXTRA_DIST = $(SRC) icons data osx_packaging Changelog doc ar.c audio_osx.c audio_pa.c audio_oss.c audio_alsa.c \
ar.h audio_device.h audio_edit.h biquad.h encoding.h fmtheaders.h gtkled.h gtkledbar.h gwc.h mconf.h mp3.h mp3-duration.h reverb_settings.h soundfile.h stat.h tap_reverb.h tap_reverb_common.h tap_reverb_file_io.h \
meschach/DOC meschach/MACHINES meschach/arnoldi.c meschach/bdfactor.c meschach/bkpfacto.c meschach/chfactor.c meschach/configure meschach/conjgrad.c meschach/copy.c meschach/copyright meschach/dmacheps.c meschach/err.c meschach/err.h \
meschach/extras.c meschach/fft.c meschach/FILELIST meschach/fmacheps.c meschach/givens.c meschach/hessen.c meschach/hsehldr.c meschach/init.c meschach/iotort.c meschach/iter.h meschach/iter0.c meschach/iternsym.c meschach/itersym.c meschach/itertort.c meschach/ivecop.c meschach/lanczos.c \
meschach/ls.dat meschach/lufactor.c meschach/machine.c meschach/machine.h.in meschach/makefile.in meschach/matlab.c meschach/matlab.h meschach/matop.c meschach/matrix.h meschach/matrix2.h meschach/matrixio.c meschach/maxint.c meschach/meminfo.c meschach/meminfo.h meschach/memory.c meschach/memstat.c \
meschach/memtort.c meschach/mfunc.c meschach/mfuntort.c meschach/norm.c meschach/oldnames.h meschach/otherio.c meschach/pxop.c meschach/qrfactor.c meschach/README meschach/rk4.dat meschach/schur.c meschach/solve.c meschach/sparse.c meschach/sparse.h meschach/sparse2.h meschach/sparseio.c \
meschach/spbkp.c meschach/spchfctr.c meschach/splufctr.c meschach/sprow.c meschach/spswap.c meschach/sptort.c meschach/submat.c meschach/svd.c meschach/symmeig.c meschach/tags meschach/torture.c meschach/tutadv.c meschach/tutorial.c meschach/update.c meschach/vecop.c meschach/version.c \
meschach/zcopy.c meschach/zfunc.c meschach/zgivens.c meschach/zhessen.c meschach/zhsehldr.c meschach/zlufctr.c meschach/zmachine.c meschach/zmatio.c meschach/zmatlab.c meschach/zmatop.c meschach/zmatrix.h meschach/zmatrix2.h meschach/zmemory.c meschach/znorm.c meschach/zqrfctr.c meschach/zschur.c \
meschach/zsolve.c meschach/ztorture.c meschach/zvecop.c

BINFILES = $(APPNAME)
# Alister: normally this should automatically cause `make [un]install` to install/uninstall it, but in our case we are doing it manually below
dist_bin_SCRIPTS = gwcbatch
# Confusingly we don't have the DOCFILES in the doc directory, only the HELPFILES
DOCFILES = README INSTALL TODO COPYING Changelog
HELPFILES = $(APPNAME).html gtkrc-example.txt
HELPFILESSRCD = doc

DATADIRS = \
    icons/hicolor/16x16/apps \
    icons/hicolor/22x22/apps \
    icons/hicolor/24x24/apps \
    icons/hicolor/32x32/apps \
    icons/hicolor/64x64/apps \
    icons/hicolor/128x128/apps \
    icons/hicolor/256x256/apps \
    icons/hicolor/scalable/apps\
    applications

###
CC = @CC@
COMPILE = $(CC) $(DEFS) $(INCLUDES) $(CPPFLAGS) $(CFLAGS)
#Alister: I don't think we ever needed OSX_CFLAGS
#COMPILE = $(CC) $(DEFS) $(INCLUDES) $(CPPFLAGS) $(CFLAGS) $(OSX_CFLAGS)

### handy to have around for checking buffer overruns
#EFENCE = -lefence
EFENCE =

all : gwc

gwc : meschach.a $(OBJS)
	$(CC) $(OBJS) $(EFENCE) $(LDFLAGS) $(LIBS) -o $(APPNAME)
        # Alister: If you have trouble building on a Mac
        # set OSX_OBJS and OSX_LIBS above and use this:
        #$(CC) $(OSX_LIBS) $(OBJS) $(OSX_OBJS) $(EFENCE) $(LDFLAGS) $(LIBS) -o $(APPNAME)

audio_device.o : audio_device.c audio_alsa.c audio_oss.c audio_osx.c audio_pa.c Makefile
	$(COMPILE) -c audio_device.c
	
audio_util.o : audio_util.c Makefile
	$(COMPILE) -c audio_util.c

.c.o :
	$(COMPILE) -c $<

# Alister: note that we can mostly install to the OSX appdir using something like this:
# make DESTDIR=/tmp/gwc/osx_packaging/Gtk\ Wave\ Cleaner.app/Contents/Resources install
# but the binary won't be in the ...Contents/MacOS directory
# so do this:
# mv /osx_packaging/Gtk\ Wave\ Cleaner.app/Contents/Resources/bin/gtk-wave-cleaner /osx_packaging/Gtk\ Wave\ Cleaner.app/Contents/MacOS/gtk-wave-cleaner.bin
# The .desktop and icon files aren't used, anyway, but at least it installs the docs!
# How do we get it to load the logo in the about dialog from either the icon files or the .icns?

install : gwc
	install -d "$(DESTDIR)"$(BINDIR)
	install -d "$(DESTDIR)"$(DOCDIR)/$(APPNAME)
	install -d "$(DESTDIR)"$(HELPDIR)/$(APPNAME)
	install -p $(BINFILES) $(dist_bin_SCRIPTS) "$(DESTDIR)"$(BINDIR)
	install -p -m 0644 $(DOCFILES) "$(DESTDIR)"$(DOCDIR)/$(APPNAME)
	for hf in $(HELPFILES) ; do install -p -m 0644 $(HELPFILESSRCD)/$$hf "$(DESTDIR)"$(HELPDIR)/$(APPNAME) ; done
	for hf in $(DATADIRS) ; do install -d "$(DESTDIR)"$(DATADIR)/$$hf && install -p -m 0644 data/$$hf/$(APPNAME).* "$(DESTDIR)"$(DATADIR)/$$hf ; done
#	if we are using DESTDIR this is pointless, so should we skip it?:
	gtk-update-icon-cache -f -t "$(DESTDIR)"$(DATADIR)/icons/hicolor

uninstall :
	( cd "$(DESTDIR)"$(BINDIR) && rm -f $(BINFILES) $(dist_bin_SCRIPTS) )
	( cd "$(DESTDIR)"$(DOCDIR)/$(APPNAME) && rm -f $(DOCFILES) )
	( cd "$(DESTDIR)"$(HELPDIR)/$(APPNAME) && rm -f $(HELPFILES) )
# Alister: note osx errors on --ignore-fail-on-non-empty
# But we wouldn't normally run `make uninstall` on osx
# If we need to we can install gnu coreutils and use its rmdir
	( rmdir --ignore-fail-on-non-empty "$(DESTDIR)"$(DOCDIR)/$(APPNAME) $(DOCDIR) )
	( for hf in $(DATADIRS) ; do rm "$(DESTDIR)"$(DATADIR)/$$hf/$(APPNAME).* && rmdir -p --ignore-fail-on-non-empty "$(DESTDIR)"$(DATADIR)/$$hf; done )
# Alister: why does this test not work?
#	( if [$(DOCDIR) != $(HELPDIR)]; then rmdir --ignore-fail-on-non-empty "$(DESTDIR)"$(HELPDIR)/$(APPNAME) "$(DESTDIR)"$(HELPDIR); fi)
# should we be running this on uninstall?:
#	gtk-update-icon-cache -f -t "$(DESTDIR)"$(DATADIR)/icons/hicolor

meschach.a : meschach/meschach.a
	cp meschach/meschach.a .

meschach/meschach.a :
# Alister: a very old message on the mailing list seemed to imply that --with-sparse is necessary to build successfully,
#          but this doesn't actually seem to be the case now
	(cd meschach ; ./configure --with-sparse ; make part1 ; make part2 ; make part3 ; cp machine.h ..)

clean :
	rm -f $(APPNAME) *.o core meschach.a meschach/meschach.a machine.h
	(cd meschach ; make realclean)
