prefix = @prefix@
exec_prefix = @exec_prefix@

if DEBUG
AM_CFLAGS = -g3 -O0
AM_CXXFLAGS = -g3 -O0
else
AM_CFLAGS = -O2
AM_CXXFLAGS = -O2
endif

BINDIR = @bindir@
DATADIR = @datadir@
DATAROOTDIR = @datarootdir@
LIBDIR = @libdir@
SYSCONFDIR = @sysconfdir@
APPNAME = gnome_wave_cleaner

pixmapdir = $(DATADIR)/pixmaps
DOCDIR = $(DATADIR)/doc/gwc
HELPDIR = $(DOCDIR)

# use these entries for SuSE and maybe other distros
#DOCDIR = /usr/share/doc/packages/gwc
#HELPDIR = $(DOCDIR)

DEFS = -DDATADIR=\"$(DATADIR)\" -DHELPDIR=\"$(HELPDIR)\" -DLIBDIR=\"$(LIBDIR)\" -Dpixmapdir=\"$(pixmapdir)\" -DAPPNAME=\"$(APPNAME)\" @ALSADEF@ @PAHDR@ @FFTWHDR@ @FFTWPREC@ @OGGHDR@ @MP3HDR@
CFLAGS = -D_FILE_OFFSET_BITS=64 -Wall @CFLAGS@ @GTK_CFLAGS@ @SNDFILE_CFLAGS@

LIBS= meschach.a @GTK_LIBS@ @SNDFILE_LIBS@ @ALSALIB@ @PALIB@ @FFTWLIB@ @OGGLIB@ @MP3LIB@ -lm

SRC = tap_reverb_file_io.c tap_reverb.c reverb.c dialog.c gwc.c audio_device.c audio_edit.c audio_util.c gtkled.c gtkledbar.c preferences.c drawing.c amplify.c denoise.c undo.c declick.c sample_block.c decrackle.c stat.c dethunk.c i0.c i1.c chbevl.c markers.c encode.c soundfile.c pinknoise.c biquad.c
OBJS = $(SRC:.c=.o)

BINFILES = gwc
dist_bin_SCRIPTS = gwcbatch.pl
pixmap_DATA = gwc-logo.png
DOCFILES = README INSTALL TODO COPYING Changelog
HELPFILES = gwc.html
HELPFILESSRCD = doc

###
CC = @CC@
COMPILE = $(CC) $(DEFS) $(INCLUDES) $(CPPFLAGS) $(CFLAGS)

### handy to have around for checking buffer overruns
#EFENCE = -lefence
EFENCE =

all : gwc

gwc : meschach.a $(OBJS)
	$(CC) $(OBJS) $(EFENCE) $(LDFLAGS) $(LIBS) -o gwc

audio_device.o : audio_device.c audio_alsa.c audio_oss.c audio_osx.c audio_pa.c Makefile
	$(COMPILE) -c audio_device.c

.c.o :
	$(COMPILE) -c $<

install : gwc
	install -d $(BINDIR)
	install -d $(DOCDIR)
	install -d $(pixmapdir)
	install -p -s $(BINFILES) $(BINDIR)
	install -p -m 0644 $(DOCFILES) $(DOCDIR)
	for hf in $(HELPFILES) ; do install -p -m 0644 $(HELPFILESSRCD)/$$hf $(HELPDIR) ; done
	install -p -m 0644 $(pixmap_DATA) $(pixmapdir)

uninstall :
	( cd $(BINDIR) && rm -f $(BINFILES) )
	( cd $(DOCDIR) && rm -f $(DOCFILES) )
	( cd $(HELPDIRC) && rm -f $(HELPFILES) )
	( cd $(pixmapdir) && rm -f $(pixmap_DATA) )
	( rmdir --ignore-fail-on-non-empty $(DOCDIR) $(pixmapdir) )
# gives an error message because we have DOCDIR=HELPDIR
#	( rmdir --ignore-fail-on-non-empty $(DOCDIR) $(HELPDIR) $(pixmapdir) )

meschach.a : meschach/meschach.a
	cp meschach/meschach.a .

meschach/meschach.a :
	(cd meschach ; ./configure --with-sparse ; make part1 ; make part2 ; make part3 ; cp machine.h ..)

clean :
	rm -f gwc *.o core meschach.a meschach/meschach.a machine.h
	(cd meschach ; make realclean)
