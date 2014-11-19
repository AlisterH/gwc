/****************************************************************************
*   Gnome Wave Cleaner Version 0.01
*   Copyright (C) 2001 Jeffrey J. Welty
*   
*   This program is free software; you can redistribute it and/or
*   modify it under the terms of the GNU General Public License
*   as published by the Free Software Foundation; either version 2
*   of the License, or (at your option) any later version.
*   
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*   
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*******************************************************************************/
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sndfile.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <libgen.h> /* for basename() */

#include "gwc.h"
#include "encoding.h"

/* sound files for in and out streams */
SNDFILE *in_fd;
SNDFILE *out_fd;
char *options[255];
char pipe_name[256] ;

SF_INFO insfinfo, outsfinfo;	/* sound file format descriptions for input and output */
extern struct encoding_prefs encoding_prefs;

struct stat filestat;

int build_options(int fmt, char *newfilename, char *trackname)
{
    int i, optcnt = 0;

    sprintf(pipe_name, ".gwc_pipe%d\n", (int)getpid()) ;

    /* remove any old options */
    for (i = 0; i < 255; i++)
	options[i] = (char *)NULL;

    if (insfinfo.channels > 2) {
	warning("We only support mono and stereo encodings");
	return (1);
    }

    if (fmt == OGG_FMT) {

	/* Check if defined encoder location and it exists */
	if ((strlen(encoding_prefs.oggloc) == 0)
	    || (stat(encoding_prefs.oggloc, &filestat) == -1)) {
	    warning("Encoder for Ogg is not defined correctly\n");
	    return 1;
	}
	/* Required Settings */

	options[optcnt] = (char *) basename(encoding_prefs.oggloc);	/* argv[0] is the excutable */
	optcnt++;
	options[optcnt] = "-";	/* read from stdin */
//	Alister: need to comment this out if use_sox=1.
	options[optcnt] = pipe_name ;	/* named pipe */
	optcnt++;
	options[optcnt] = "-o";	/* Output to newfilename */
	optcnt++;
	options[optcnt] = newfilename;
	optcnt++;
	options[optcnt] = "-r";	/* Raw input */
	optcnt++;
	options[optcnt] = "-C";	/* Channels */
	optcnt++;

	if (insfinfo.channels == 2) {
	    options[optcnt] = "2";
	} else {
	    options[optcnt] = "1";
	}
	optcnt++;

	options[optcnt] = "-R";	/* Sample Rate */
	optcnt++;

// Alister: I think we could get rid of the warning below and possibly allow the user to choose the samplerate instead of getting it from the input file.
	switch (insfinfo.samplerate) {
	case 48000:
	    options[optcnt] = "48000";
	    break;
	case 44100:
	    options[optcnt] = "44100";
	    break;
	case 32000:
	    options[optcnt] = "32000";
	    break;
	case 24000:
	    options[optcnt] = "24000";
	    break;
	case 22050:
	    options[optcnt] = "22050";
	    break;
	case 16000:
	    options[optcnt] = "16000";
	    break;
	case 12000:
	    options[optcnt] = "12000";
	    break;
	case 11025:
	    options[optcnt] = "11025";
	    break;
	case 8000:
	    options[optcnt] = "8000";
	    break;
	default:
	    warning
		("Please resample audio file to standard 8/11.025/12/16/22.05/24/32/44.1/48 kHz\n");
	    return (1);
	    break;
	}
	optcnt++;

	options[optcnt] = "-B";	/* Bit Width */
	optcnt++;

	options[optcnt] = "16";	/* Output format is 16 bit PCM */
	optcnt++;

	/* check for downmix */
	if (encoding_prefs.ogg_downmix == 1) {
	    options[optcnt] = "--downmix";
	    optcnt++;
	}
	/* check for resample */
	if (encoding_prefs.ogg_useresample == 1) {
	    options[optcnt] = "--resample";
	    options[optcnt + 1] = encoding_prefs.ogg_resample;
	    optcnt = optcnt + 2;

	}
	/* check for adv low pass */

	if (encoding_prefs.ogg_useadvlowpass == 1) {
	    options[optcnt] = "--advanced_encode_option";
	    options[optcnt + 1] =
		strcat("lowpass_frequency=",
		       encoding_prefs.ogg_lowpass_frequency);
	    optcnt = optcnt + 2;
	}
	/* check for adv Bitrate Avg Window */
	if (encoding_prefs.ogg_useadvbravgwindow == 1) {
	    options[optcnt] = "--advanced_encode_option";
	    options[optcnt + 1] =
		strcat("bitrate_average_window=",
		       encoding_prefs.ogg_bitrate_average_window);
	    optcnt = optcnt + 2;
	}
	if ((strlen(trackname) > 0)) {
	    options[optcnt] = "-t";
	    options[optcnt + 1] = trackname ;
	    optcnt += 2;
	}
	/* check Encoding Mode options */

	switch (encoding_prefs.ogg_encopt) {
	case 0:
	    break;		/* We want to use the default options */
	case 1:
	    options[optcnt] = "--managed";
	    options[optcnt + 1] = "-M";
	    options[optcnt + 2] = encoding_prefs.ogg_maxbitrate;
	    options[optcnt + 3] = "-m";
	    options[optcnt + 4] = encoding_prefs.ogg_minbitrate;
	    optcnt = optcnt + 5;
	    break;
	case 2:
	    options[optcnt] = "-b";
	    options[optcnt + 1] = encoding_prefs.ogg_bitrate;
	    optcnt = optcnt + 2;
	    break;
	case 3:
	    options[optcnt] = "-q";
	    options[optcnt + 1] = encoding_prefs.ogg_quality_level;
	    break;
	default:
	    warning("Unrecognized encoding options selected\n");
	    return (1);
	    break;		/* should not be here unknown value */
	}
    } else if(fmt == MP3_FMT) {
	/* Check if defined encoder location and it exists */
	if ((strlen(encoding_prefs.mp3loc) == 0)
	    || (stat(encoding_prefs.mp3loc, &filestat) == -1)) {
	    warning("Encoder for MP3 is not defined correctly\n");
	    return 1;
	}
	/* Required Settings */

	options[optcnt] = (char *) basename(encoding_prefs.mp3loc);	/* argv[0] is the executable */
	optcnt++;
	options[optcnt] = "-r";	/* read raw */
	optcnt++;
	options[optcnt] = "--bitwidth";
	optcnt++;
	options[optcnt] = "16";
	optcnt++;
	options[optcnt] = "-s";	/* Sample Rate */
	optcnt++;

// Alister: I think we could get rid of the warning below and possibly allow the user to choose the samplerate instead of getting it from the input file.
	switch (insfinfo.samplerate) {
	case 48000:
	    options[optcnt] = "48";
	    break;
	case 44100:
	    options[optcnt] = "44.1";
	    break;
	case 32000:
	    options[optcnt] = "32";
	    break;
	case 24000:
	    options[optcnt] = "24";
	    break;
	case 22050:
	    options[optcnt] = "22.05";
	    break;
	case 16000:
	    options[optcnt] = "16";
	    break;
	case 12000:
	    options[optcnt] = "12";
	    break;
	case 11025:
	    options[optcnt] = "11.025";
	    break;
	case 8000:
	    options[optcnt] = "8";
	    break;
	default:
	    warning
		("Please resample audio file to standard 8/11.025/12/16/22.05/24/32/44.1/48 kHz\n");
	    return (1);
	    break;
	}
	optcnt++;

	options[optcnt] = "-m";	/*  Mode */
	optcnt++;

	if (insfinfo.channels == 2) {
	    options[optcnt] = "s";	/* stereo */
	} else {
	    options[optcnt] = "m";	/* mono */
	}
	optcnt++;

//	Alister: this creates a file that is just a loud hiss for at least me and John Cirillo (see mailing list)
//	options[optcnt] = "-x";	/* Swap bytes */

//	optcnt++;

	/* asm options only for MMX enabled version of lame */
	if (encoding_prefs.mp3_lame_mmx_enabled) {
	    if (!encoding_prefs.mp3_sse) {
		options[optcnt] = "--noasm";
		options[optcnt + 1] = "sse";
		optcnt = optcnt + 2;
	    }

	    if (!encoding_prefs.mp3_mmx) {
		options[optcnt] = "--noasm";
		options[optcnt + 1] = "mmx";
		optcnt = optcnt + 2;
	    }

	    if (!encoding_prefs.mp3_threednow) {
		options[optcnt] = "--noasm";
		options[optcnt + 1] = "3dnow";
		optcnt = optcnt + 2;
	    }
	}

	if (encoding_prefs.mp3_copyrighted) {

	    options[optcnt] = "-c";
	    optcnt++;
	}

	if (encoding_prefs.mp3_nofilters) {

	    options[optcnt] = "-k";
	    optcnt++;
	}

	if (encoding_prefs.mp3_add_crc) {

	    options[optcnt] = "-p";
	    optcnt++;
	}

	if (encoding_prefs.mp3_strict_iso) {

	    options[optcnt] = "--strictly-enforce-ISO";
	    optcnt++;
	}

	if (encoding_prefs.mp3_use_lowpass) {

	    options[optcnt] = "--lowpass";
	    options[optcnt + 1] = encoding_prefs.mp3_lowpass_freq;
	    optcnt = optcnt + 2;

	}

	if (encoding_prefs.mp3_use_highpass) {

	    options[optcnt] = "--highpass";
	    options[optcnt + 1] = encoding_prefs.mp3_highpass_freq;
	    optcnt = optcnt + 2;

	}


	{
	    int qval = atoi(encoding_prefs.mp3_quality_level);
	    if (qval < 0 || qval > 9) {
		warning
		    ("Quality level for MP3 encoder should be between 0 and 9\n");
		return (1);
	    } else {
		/* Set quality level */
		options[optcnt] = "-q";
		options[optcnt + 1] = encoding_prefs.mp3_quality_level;
		optcnt = optcnt + 2;
	    }
	}


	/* check if preset is enabled */
	if (encoding_prefs.mp3presets == 0) {
	    /*  check for alternate settings */

	    if (encoding_prefs.mp3_br_mode == 1) {
		/* ABR */
		options[optcnt] = "--abr";
		options[optcnt + 1] = encoding_prefs.mp3_bitrate;
		optcnt = optcnt + 2;
	    } else if (encoding_prefs.mp3_br_mode == 2) {
		int bval = atoi(encoding_prefs.mp3_bitrate);
		/* CBR */
		options[optcnt] = "--cbr";
		options[optcnt + 1] = "-b";


		switch (bval) {
		case 320:
		case 256:
		case 224:
		case 192:
		case 160:
		case 128:
		case 112:
		case 96:
		case 80:
		case 64:
		case 48:
		case 40:
		case 32:
		    break;	/* all these values are okay */
		default:
		    warning
			("Please choose bitrate of 320/256/224/192/160/128/112/96/80/64/48/40 or 32");
		    return (1);	/* Bad value */
		    break;
		}

		options[optcnt + 2] = encoding_prefs.mp3_bitrate;
		optcnt = optcnt + 3;

	    } else if (encoding_prefs.mp3_br_mode == 3) {
		/* VBR */
		options[optcnt] = "-V";
		options[optcnt + 1] = encoding_prefs.mp3_quality_level;
		optcnt = optcnt + 2;

	    }
	} else {
	    printf("MP3 preset is %d\n", encoding_prefs.mp3presets);

	    switch (encoding_prefs.mp3presets) {

	    case 1:
		options[optcnt] = "--r3mix";
		optcnt = optcnt + 1;
		break;
	    case 2:
		options[optcnt] = "--preset";
		options[optcnt + 1] = "standard";
		optcnt = optcnt + 2;
		break;
	    case 3:
		options[optcnt] = "--preset";
		options[optcnt + 1] = "medium";
		optcnt = optcnt + 2;
		break;
	    case 4:
		options[optcnt] = "--preset";
		options[optcnt + 1] = "extreme";
		optcnt = optcnt + 2;
		break;
	    case 5:
		options[optcnt] = "--preset";
		options[optcnt + 1] = "insane";
		optcnt = optcnt + 2;
		break;
	    case 6:
		options[optcnt] = "--preset";
		options[optcnt + 1] = "fast";
		options[optcnt + 2] = "standard";
		optcnt = optcnt + 3;
		break;
	    case 7:
		options[optcnt] = "--preset";
		options[optcnt + 1] = "fast";
		options[optcnt + 2] = "medium";
		optcnt = optcnt + 3;
		break;
	    case 8:
		options[optcnt] = "--preset";
		options[optcnt + 1] = "fast";
		options[optcnt + 2] = "extreme";
		optcnt = optcnt + 3;
		break;
	    default:
		warning
		    ("Failed to interpret which mp3 encoding preset you selected");
		return (1);
		break;
	    }

	}

	if ((strlen(trackname) > 0)) {
	    options[optcnt] = "--tt";
	    options[optcnt + 1] = trackname ;
	    optcnt += 2;
	}

	/* All other options come before the input and output filenames */
	options[optcnt] = "-";	/* Stdin */
//	Alister: need to comment this out if use_sox=1.
	options[optcnt] = pipe_name ;	/* named pipe */
	options[optcnt + 1] = newfilename;
    } else {
	/* fmt == MP3_SIMPLE_FMT */
	/* Check if defined encoder location and it exists */
	if ((strlen(encoding_prefs.mp3loc) == 0)
	    || (stat(encoding_prefs.mp3loc, &filestat) == -1)) {
	    warning("Encoder for MP3 is not defined correctly\n");
	    return 1;
	}
	/* Required Settings */

	options[optcnt] = (char *) basename(encoding_prefs.mp3loc);	/* argv[0] is the executable */
	optcnt++;
	options[optcnt] = "-h";	/* use recommended quality setting from lame */
	optcnt++;
	options[optcnt] = "-r";	/* read raw */
	optcnt++;
	options[optcnt] = "--bitwidth";
	optcnt++;
	options[optcnt] = "16";
	optcnt++;
	options[optcnt] = "-V";
	optcnt++;
	options[optcnt] = encoding_prefs.mp3_quality_level ;
	optcnt++;
	options[optcnt] = "-s";	/* Sample Rate */
	optcnt++;

// Alister: I think we could get rid of the warning below.
	switch (insfinfo.samplerate) {
	case 48000:
	    options[optcnt] = "48";
	    break;
	case 44100:
	    options[optcnt] = "44.1";
	    break;
	case 32000:
	    options[optcnt] = "32";
	    break;
	case 24000:
	    options[optcnt] = "24";
	    break;
	case 22050:
	    options[optcnt] = "22.05";
	    break;
	case 16000:
	    options[optcnt] = "16";
	    break;
	case 12000:
	    options[optcnt] = "12";
	    break;
	case 11025:
	    options[optcnt] = "11.025";
	    break;
	case 8000:
	    options[optcnt] = "8";
	    break;
	default:
	    warning
		("Please resample audio file to standard 8/11.025/12/16/22.05/24/32/44.1/48 kHz\n");
	    return (1);
	    break;
	}
	optcnt++;

	options[optcnt] = "-m";	/*  Mode */
	optcnt++;
	if (insfinfo.channels == 2) {
	    options[optcnt] = "s";	/* stereo */
	} else {
	    options[optcnt] = "m";	/* mono */
	}
	optcnt++;
	if ((strlen(encoding_prefs.artist) > 0)) {
	    options[optcnt] = "--ta";
	    options[optcnt + 1] = encoding_prefs.artist;
	    optcnt += 2;
	}

	if ((strlen(encoding_prefs.album) > 0)) {
	    options[optcnt] = "--tl";
	    options[optcnt + 1] = encoding_prefs.album;
	    optcnt += 2;
	}

	if ((strlen(trackname) > 0)) {
	    options[optcnt] = "--tt";
	    options[optcnt + 1] = trackname ;
	    optcnt += 2;
	}

	/* All other options come before the input and output filenames */
	options[optcnt] = "-";	/* Stdin */
//	Alister: need to comment this out if use_sox=1.
	options[optcnt] = pipe_name ;	/* named pipe */
	options[optcnt + 1] = newfilename;
    }

    return (0);
}


int encode(int mode, char *origfilename, char *newfilename, long start,
	   long length, char *trackname)
{

    int ret;

    int fd_test = open(newfilename, O_RDONLY) ;

    if(fd_test != -1) {
	close(fd_test) ;
	if(yesno("File exists, overwrite?"))
	    return 0 ;
    }

    signal(SIGCHLD, SIG_IGN);	/* Make sure we dont create a zombie */
    signal(SIGPIPE, SIG_IGN);	/*  Make sure broken pipe doesnt make us exit */

    /* open file to get paramters needed for rate etc.. */
    /* Open a sound file */

    insfinfo.format = 0;	/* before open set format to 0 */

    in_fd = sf_open(origfilename, SFM_READ, &insfinfo);


    if (in_fd == NULL) {
	fprintf(stderr, "Failed to open  %s", origfilename);
	return (1);
    }

    if (start + length > insfinfo.frames) {
	warning("Selection is outside of soundfile\n");
	sf_close(in_fd);
	return (1);
    }

    outsfinfo = insfinfo;

    /* skip to position in sound file */
    if (!sf_seek(in_fd, start, SEEK_SET) == start) {
	warning("Failed to seek to position in sound file\n");
	sf_close(in_fd);
	return (1);
    }


    ret = build_options(mode, newfilename, trackname);	/* create options based on encoder chosen */

    if (ret != 0) {
	sf_close(in_fd);
	return (ret);
    }
    /* set format for output */
    outsfinfo.format = (SF_FORMAT_RAW | SF_FORMAT_PCM_16);

    ret = start_encode(mode, newfilename, start, length, origfilename);

    return ret;

}


void create_progress_window(void) ; 
gint encode_progress (gfloat pvalue) ;
void destroy_progress_window(void) ;
gint stop_encoding = FALSE ;

int start_encode_old(int mode, char *newfilename, long start, long length, char *origfilename)
{
    long samples_read;
    long ctr;
    long numframes = 0;
    int f_des[2], child_pid;
    int i=0 ;
    int use_sox = 1 ;
    char cmd[2048] ;
    char *exec_loc ;

    if (mode == OGG_FMT) {
	/* execute ogg encoder using prebuilt options */
	exec_loc = encoding_prefs.oggloc ;
    } else if (mode == MP3_FMT) {
	/* execute mp3 encoder using prebuilt options */
	exec_loc = encoding_prefs.mp3loc ;
    } else if (mode == MP3_SIMPLE_FMT) {
	/* execute mp3 encoder using prebuilt options */
	exec_loc = encoding_prefs.mp3loc ;
    }

    printf("Encoding using %s\n", exec_loc) ;
    printf("   ") ;

    for(i = 0 ; options[i] != (char *)NULL ; i++)
	printf("\'%s\'", options[i]) ;

    printf("\n") ;

    if(use_sox) {
    sprintf(cmd, "sox %s -t raw - trim %ld\s %ld\s |", origfilename, start, length) ;

    for(i = 0 ; options[i] != (char *)NULL ; i++) {
	int j ;
	strcat(cmd, " ") ;

	for(j = 0 ; j < strlen(options[i]) ; j++) {
	    char buf[10] ;
	    int need_esc=0 ;

	    if(options[i][j] == ' ') need_esc = 1 ;
	    if(options[i][j] == '\'') need_esc = 1 ;
	    if(options[i][j] == '\"') need_esc = 1 ;

	    if(need_esc)
		sprintf(buf, "\\%c", options[i][j]) ;
	    else
		sprintf(buf, "%c", options[i][j]) ;

	    strcat(cmd, buf) ;
	}
    }

    printf("CMD:\n'%s\'\n", cmd) ;
    system(cmd) ;
    return 0 ;
    }




    /* Create a  pipe to send data through - must do first */

    if (pipe(f_des) != 0) {
	warning("Unable to process file now \n");
	return (1);

    }
    /* fork to allow us to create a child process for processing data */

    if ((child_pid = fork()) == 0) {

	/* Child (or proc 2) */

	close(0);		/* redirecting stdin fd=0 */
	dup(f_des[0]);		/* duplicate pipe read with stdin i.e. 0=3 */

	close(f_des[1]);	/* child proc does not need stdout of pipe */

	if (execvp(exec_loc, options) == -1) {
	    warning("Failed to process audio file\n");
	    return (1);
	}

    } else {
	double *framebuf = NULL ;
	FILE *fp ;
	/* Parent */
	fprintf(stderr, "encoding child pid is %d\n", child_pid) ;
	close(1);		/* redirecting stdout fd=1 */
	dup(f_des[1]);		/* duplicate pipe write with stdout i.e. 1=4 */
	close(f_des[0]);	/* closing stdin of pipe for this process not used */

	/*  open output to standard out */
	out_fd = sf_open("/dev/stdout", SFM_WRITE, &outsfinfo);

	if (out_fd == NULL) {
	    warning("Failed to open output file\n");
	    sf_close(in_fd);
	    return (1);
	}
	/*  put out raw data */

	/* Make sure frame buffer size is multiple of # channels */
	numframes = 44100;
	framebuf = (double*)calloc(numframes * insfinfo.channels, sizeof(double));
	if (framebuf == NULL)
		{
		warning( "failed to allocate intermediate buffer\n");
		sf_close(in_fd);
		return (1);
		}
	create_progress_window() ;

	/* read in blocks of size  buffer and  send out converted to new sound format */
	for (ctr = 0; ctr < length; ctr += samples_read) {
	    if(stop_encoding == TRUE) break ;
	    samples_read = sf_readf_double(in_fd, framebuf, numframes);
	    sf_writef_double(out_fd, framebuf, samples_read);
	    encode_progress ((double)ctr/(double)length) ;
	}

	destroy_progress_window() ;

	close(1);		/* close stdout on parent  */

	/*  Main  Parent (proc 1) */
	sf_close(in_fd);	/* done reading/writing from program */
	sf_close(out_fd);

	close(f_des[0]);	/*  overall parent closes pipes so that  */
	close(f_des[1]);	/*  processes terminate on time */

	fp = freopen("/dev/tty", "w", stdout);	/* reopen tty for writing on stdout */

	if(fp == NULL) {
	    fprintf(stderr, "FAILED to reopen stdout!\n") ;
	} else {
	    stdout = fp ;
	    fprintf(stderr, "SUCCEEDED to reopen stdout!\n") ;
	    printf("Finished encoding using %s\n", encoding_prefs.mp3loc) ;
	}
    }

    return (0);

}

// Alister: if using this, need to uncomment the line noted above so the pipe name is inserted for the "simple encode..." function
int start_encode(int mode, char *newfilename, long start, long length, char *origfilename)
{
    long samples_read;
    long ctr;
    long numframes = 0;
    int f_des[2], child_pid;
    int i=0 ;
    int use_sox = 0 ;
    char cmd[2048] ;
    char *exec_loc ;

    if (mode == OGG_FMT) {
	/* execute ogg encoder using prebuilt options */
	exec_loc = encoding_prefs.oggloc ;
    } else if (mode == MP3_FMT) {
	/* execute mp3 encoder using prebuilt options */
	exec_loc = encoding_prefs.mp3loc ;
    } else if (mode == MP3_SIMPLE_FMT) {
	/* execute mp3 encoder using prebuilt options */
	exec_loc = encoding_prefs.mp3loc ;
    }

    printf("Encoding using %s\n", exec_loc) ;
    printf("   ") ;

    for(i = 0 ; options[i] != (char *)NULL ; i++)
	printf("\'%s\'", options[i]) ;

    printf("\n") ;

    if(use_sox) {
	sprintf(cmd, "sox %s -t raw - trim %ld\s %ld\s |", origfilename, start, length) ;

	for(i = 0 ; options[i] != (char *)NULL ; i++) {
	    int j ;
	    strcat(cmd, " ") ;

	    for(j = 0 ; j < strlen(options[i]) ; j++) {
		char buf[10] ;
		int need_esc=0 ;

		if(options[i][j] == ' ') need_esc = 1 ;
		if(options[i][j] == '\'') need_esc = 1 ;
		if(options[i][j] == '\"') need_esc = 1 ;

		if(need_esc)
		    sprintf(buf, "\\%c", options[i][j]) ;
		else
		    sprintf(buf, "%c", options[i][j]) ;

		strcat(cmd, buf) ;
	    }
	}

	printf("CMD:\n'%s\'\n", cmd) ;
	system(cmd) ;
	return 0 ;
    }




    /* Create a  pipe to send data through - must do first */
    /* now using a named pipe */
    if(mkfifo(pipe_name, S_IRWXU)) {
	static char buf[254] ;
	snprintf(buf,255,"Failed to open named pipe to transfer data to %s, cannot proceed", exec_loc) ;
	warning(buf) ;
	return 1 ;
    }

    /* fork to allow us to create a child process for processing data */

    if ((child_pid = fork()) == 0) {

	/* Child (or proc 2) */
	if (execvp(exec_loc, options) == -1) {
	    warning("Failed to process audio file\n");
	    return (1);
	}

    } else {
	double *framebuf = NULL ;
	FILE *fp ;
	/* Parent */
	fprintf(stderr, "encoding child pid is %d\n", child_pid) ;

	//close(1);		/* redirecting stdout fd=1 */
	//dup(f_des[1]);		/* duplicate pipe write with stdout i.e. 1=4 */
	//close(f_des[0]);	/* closing stdin of pipe for this process not used */

	/*  open output to named_pipe */
	out_fd = sf_open(pipe_name, SFM_WRITE, &outsfinfo);

	if (out_fd == NULL) {
	    warning("Failed to open output file\n");
	    sf_close(in_fd);
	    unlink(pipe_name) ;
	    return (1);
	}
	/*  put out raw data */

	/* Make sure frame buffer size is multiple of # channels */
	numframes = 44100;
	framebuf = (double*)calloc(numframes * insfinfo.channels, sizeof(double));
	if (framebuf == NULL) {
	    warning( "failed to allocate intermediate buffer\n");
	    sf_close(in_fd);
	    unlink(pipe_name) ;
	    return (1);
	}

	create_progress_window() ;

	/* read in blocks of size  buffer and  send out converted to new sound format */
	for (ctr = 0; ctr < length; ctr += samples_read) {
	    if(stop_encoding == TRUE) break ;
	    samples_read = sf_readf_double(in_fd, framebuf, numframes);
	    sf_writef_double(out_fd, framebuf, samples_read);
	    encode_progress ((double)ctr/(double)length) ;
	}

	destroy_progress_window() ;

	/*  Main  Parent (proc 1) */
	sf_close(in_fd);	/* done reading/writing from program */
	sf_close(out_fd);
	unlink(pipe_name) ;
    }

    return (0);

}

static GtkWidget *progress_dialog;
static GtkWidget *button;
static GtkWidget *label;
static GtkWidget *table;
static GtkWidget *pbar;

/* This function increments and updates the progress bar, it also resets
 the progress bar if pstat is FALSE */
gint encode_progress (gfloat pvalue)
{
    gtk_progress_bar_update (GTK_PROGRESS_BAR (pbar), pvalue);
    while(gtk_events_pending())
	gtk_main_iteration() ;
    
    return TRUE;
}

void cancel_encoding(GtkWidget *w)
{
    stop_encoding = TRUE ;
}

void destroy_progress_window(void)
{
    gtk_widget_destroy(pbar) ;
    gtk_widget_destroy(button) ;
    gtk_widget_destroy(label) ;
    gtk_widget_destroy(table) ;
    gtk_widget_destroy(progress_dialog) ;
}

void create_progress_window(void)
{
    //progress_dialog = gtk_dialog_new("Encoding progress window", NULL);
    progress_dialog = gtk_dialog_new();
    //Alister: this doesn't work for some reason
    //gtk_window_set_transient_for(GTK_WINDOW(progress_dialog), GTK_WINDOW(main_window));

    gtk_container_border_width (GTK_CONTAINER (progress_dialog), 10);
    
    table = gtk_table_new(3,1,TRUE);
    
    label = gtk_label_new ("Encoding progress");
    gtk_table_attach_defaults(GTK_TABLE(table), label, 0,1,0,1);
    gtk_widget_show(label);
    
    /* Create a new progress bar, pack it into the table, and show it */
    pbar = gtk_progress_bar_new ();
    gtk_table_attach_defaults(GTK_TABLE(table), pbar, 0,1,1,2);
    gtk_widget_show (pbar);
    
    button = gtk_button_new_with_label ("Cancel");

    /* gtk_signal_connect_object(GTK_OBJECT(button), "clicked", */
			      /* GTK_SIGNAL_FUNC(gtk_widget_destroy), (gpointer) progress_dialog); */
    gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
			      GTK_SIGNAL_FUNC(cancel_encoding), NULL);
    
    gtk_table_attach_defaults(GTK_TABLE(table), button, 0,1,2,3);
    gtk_widget_show (button);
    
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(progress_dialog)->vbox), table,
		       TRUE, TRUE, 0);
    gtk_widget_show(table);
    gtk_widget_show(progress_dialog);
    while(gtk_events_pending())
	gtk_main_iteration() ;
}

