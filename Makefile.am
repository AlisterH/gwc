# Alister: there are issues with the way this is all set up.
# 1. It is too complicated - we aren't letting autotools do enough for us.
# 2. Make does not recognise if header files have changed and rebuild the necessary objects.
# 3. We haven't properly integrated meschach.  Make here doesn't recognise if any of meschach's header or source files have changed, and autoconf or autoreconf here doesn't touch it. (also see note in meschach/configure - neither autoconf nor building in meschach actually works in our version).
# 4. `make dist` needs to be told to include all of the header and source files, as we haven't specified them as gwc_SOURCES.
# 5. `make clean` errors trying to clean the meschach directory if `make` has not been run :)

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

if PLATFORM_OSX
#note on my test system I initially couldn't use -lgtkmacintegration as it wasn't building properly, so I had to link to the object files.
#OSX_OBJS = /Users/alister/gtk-mac-integration-2.0.8/src/.libs/*.o
#note depending on your build environment you might need -lgtkmacintegration.2 or something instead.
OSX_LIBS = -framework CoreAudio -lgtkmacintegration-gtk2
#looks like I don't need -lobjc, or this:
#OSX_CFLAGS = -xobjective-c

#might need something like this
#if test "`echo ${GCC_VERSION} | cut -d. -f1`" -eq 4; then
#   if test "`echo ${GCC_VERSION} | cut -d. -f2`" -ge 8; then
#  	   # This is gcc >= 4.8.x
#      OSX_CFLAGS = -xobjective-c -fobjc-exceptions
#   fi
#	else OSX_CFLAGS = -xobjective-c
#fi
endif

BINDIR = @bindir@
DATADIR = @datadir@
DATAROOTDIR = @datarootdir@
LIBDIR = @libdir@
SYSCONFDIR = @sysconfdir@
APPNAME = gnome-wave-cleaner

desktopdir = $(DATADIR)/applications
pixmapdir = $(DATADIR)/pixmaps
DOCDIR = $(DATADIR)/doc
# Alister: if you set HELPDIR different to DOCDIR, you will need to add it to the uninstall routine below
HELPDIR = $(DOCDIR)

# use these entries for SuSE and maybe other distros
#DOCDIR = /usr/share/doc/packages/gwc
#HELPDIR = $(DOCDIR)

DEFS = -DDATADIR=\"$(DATADIR)\" -DHELPDIR=\"$(HELPDIR)\" -DLIBDIR=\"$(LIBDIR)\" -Dpixmapdir=\"$(pixmapdir)\" -DAPPNAME=\"$(APPNAME)\" @OSXDEF@ @ALSADEF@ @PAHDR@ @FFTWHDR@ @FFTWPREC@ @OGGHDR@ @MP3HDR@
CFLAGS = -D_FILE_OFFSET_BITS=64 -Wall @CFLAGS@ @GTK_CFLAGS@ @SNDFILE_CFLAGS@

LIBS= meschach.a @GTK_LIBS@ @SNDFILE_LIBS@ @ALSALIB@ @PALIB@ @FFTWLIB@ @OGGLIB@ @MP3LIB@ -lm

SRC = tap_reverb_file_io.c tap_reverb.c reverb.c dialog.c gwc.c audio_device.c audio_edit.c audio_util.c gtkled.c gtkledbar.c preferences.c drawing.c amplify.c denoise.c undo.c declick.c sample_block.c decrackle.c stat.c dethunk.c i0.c i1.c chbevl.c markers.c encode.c soundfile.c pinknoise.c biquad.c
OBJS = $(SRC:.c=.o)
# Alister: We should be able to avoid most of this - see item 3 and 4 in the comment at the top
# Alister: leave out meschach/configure.in, as it makes a broken configure, and someone may try to use it!
EXTRA_DIST = $(SRC) icons Changelog doc ar.c audio_osx.c audio_pa.c audio_oss.c audio_alsa.c \
ar.h audio_device.h audio_edit.h biquad.h encoding.h fmtheaders.h gtkled.h gtkledbar.h gwc.h mconf.h mp3.h mp3-duration.h reverb_settings.h soundfile.h stat.h tap_reverb.h tap_reverb_common.h tap_reverb_file_io.h \
meschach/DOC meschach/MACHINES meschach/arnoldi.c meschach/bdfactor.c meschach/bkpfacto.c meschach/chfactor.c meschach/config.cache meschach/configure meschach/conjgrad.c meschach/copy.c meschach/copyright meschach/dmacheps.c meschach/err.c meschach/err.h \
meschach/extras.c meschach/fft.c meschach/FILELIST meschach/fmacheps.c meschach/givens.c meschach/hessen.c meschach/hsehldr.c meschach/init.c meschach/iotort.c meschach/iter.h meschach/iter0.c meschach/iternsym.c meschach/itersym.c meschach/itertort.c meschach/ivecop.c meschach/lanczos.c \
meschach/ls.dat meschach/lufactor.c meschach/machine.c meschach/machine.h.in meschach/makefile.in meschach/matlab.c meschach/matlab.h meschach/matop.c meschach/matrix.h meschach/matrix2.h meschach/matrixio.c meschach/maxint.c meschach/meminfo.c meschach/meminfo.h meschach/memory.c meschach/memstat.c \
meschach/memtort.c meschach/mfunc.c meschach/mfuntort.c meschach/norm.c meschach/oldnames.h meschach/otherio.c meschach/pxop.c meschach/qrfactor.c meschach/README meschach/rk4.dat meschach/schur.c meschach/solve.c meschach/sparse.c meschach/sparse.h meschach/sparse2.h meschach/sparseio.c \
meschach/spbkp.c meschach/spchfctr.c meschach/splufctr.c meschach/sprow.c meschach/spswap.c meschach/sptort.c meschach/submat.c meschach/svd.c meschach/symmeig.c meschach/tags meschach/torture.c meschach/tutadv.c meschach/tutorial.c meschach/update.c meschach/vecop.c meschach/version.c \
meschach/zcopy.c meschach/zfunc.c meschach/zgivens.c meschach/zhessen.c meschach/zhsehldr.c meschach/zlufctr.c meschach/zmachine.c meschach/zmatio.c meschach/zmatlab.c meschach/zmatop.c meschach/zmatrix.h meschach/zmatrix2.h meschach/zmemory.c meschach/znorm.c meschach/zqrfctr.c meschach/zschur.c \
meschach/zsolve.c meschach/ztorture.c meschach/zvecop.c

BINFILES = $(APPNAME)
dist_bin_SCRIPTS = gwcbatch.pl
pixmap_DATA = $(APPNAME).xpm
desktop_DATA = $(APPNAME).desktop
DOCFILES = README INSTALL TODO COPYING Changelog
HELPFILES = $(APPNAME).html
HELPFILESSRCD = doc

###
CC = @CC@
COMPILE = $(CC) $(DEFS) $(INCLUDES) $(CPPFLAGS) $(CFLAGS) $(OSX_CFLAGS)

### handy to have around for checking buffer overruns
#EFENCE = -lefence
EFENCE =

all : gwc

gwc : meschach.a $(OBJS) 
	$(CC) $(OSX_LIBS) $(OBJS) $(OSX_OBJS) $(EFENCE) $(LDFLAGS) $(LIBS) -o $(APPNAME)

#audio_device.o : audio_device.c audio_alsa.c audio_oss.c audio_osx.c audio_pa.c Makefile
audio_device.o : audio_device.c audio_osx.c Makefile
	$(COMPILE) -c audio_device.c
	
audio_util.o : audio_util.c Makefile
	$(COMPILE) -c audio_util.c

.c.o :
	$(COMPILE) -c $<

install : gwc
	install -d $(BINDIR)
	install -d $(DOCDIR)/$(APPNAME)
	install -d $(HELPDIR)/$(APPNAME)
	install -d $(pixmapdir)
	install -p -s $(BINFILES) $(BINDIR)
	install -p -m 0644 $(DOCFILES) $(DOCDIR)/$(APPNAME)
	for hf in $(HELPFILES) ; do install -p -m 0644 $(HELPFILESSRCD)/$$hf $(HELPDIR)/$(APPNAME) ; done
	install -p -m 0644 icons/$(pixmap_DATA) $(pixmapdir)
	install -p -m 0644 $(desktop_DATA) $(desktopdir)

uninstall :
	( cd $(BINDIR) && rm -f $(BINFILES) )
	( cd $(DOCDIR)/$(APPNAME) && rm -f $(DOCFILES) )
	( cd $(HELPDIR)/$(APPNAME) && rm -f $(HELPFILES) )
	( cd $(pixmapdir) && rm -f $(pixmap_DATA) )
	( cd $(desktopdir) && rm -f $(desktop_DATA) )
# Alister: note osx errors on --ignore-fail-on-non-empty
# But we wouldn't normally run `make install` on osx
	( rmdir --ignore-fail-on-non-empty $(DOCDIR)/$(APPNAME) $(DOCDIR) $(pixmapdir) $(desktopdir))
# Alister: why does this test not work?
#	( if [$(DOCDIR) != $(HELPDIR)]; then rmdir --ignore-fail-on-non-empty $(HELPDIR)/$(APPNAME) $(HELPDIR); fi)

meschach.a : meschach/meschach.a
	cp meschach/meschach.a .

meschach/meschach.a :
# Alister: a very old message on the mailing list seemed to imply that --with-sparse is necessary to build successfully,
#          but this doesn't actually seem to be the case now
	(cd meschach ; ./configure --with-sparse ; make part1 ; make part2 ; make part3 ; cp machine.h ..)

clean :
	rm -f gnome-wave-cleaner *.o core meschach.a meschach/meschach.a machine.h
	(cd meschach ; make realclean)
