/*****************************************************************************
*   Gnome Wave Cleaner Version 0.19
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

/* audio_util.c */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <memory.h>

#ifdef MAC_OS_X

/* this seems to give wrong results on intel macs :( */
#include <machine/endian.h>

/* doing this doesn't seem to fix it, and would presumably break it on */
/* powerpc macs (but are we otherwise supported there?) */
/*#define __BYTE_ORDER __LITTLE_ENDIAN */

#else
#include <endian.h>
#endif

#include <fcntl.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <sndfile.h>

#ifdef HAVE_OGG
#include "vorbis/codec.h"
#include "vorbis/vorbisfile.h"
#endif

#include "gwc.h"

#ifdef HAVE_MP3
#include "mpg123.h"
#endif

#include "fmtheaders.h"
#include "encoding.h"
#include "audio_device.h"
#include "soundfile.h"

int audio_state = AUDIO_IS_IDLE ;
int wavefile_fd = -1 ;
long audio_bytes_written ;
int rate = 44100 ;
int stereo = 1 ;
int audio_bits = 16 ;
int BYTESPERSAMPLE = 2 ;
int MAXSAMPLEVALUE = 1 ;
int PLAYBACK_FRAMESIZE = 4 ;
int FRAMESIZE = 4 ;
int current_ogg_bitstream = 0 ;
int nonzero_seek ;
/*  int dump_sample = 0 ;  */
long wavefile_data_start ;

SNDFILE      *sndfile = NULL ;
SF_INFO      sfinfo ;

#ifdef HAVE_OGG
OggVorbis_File oggfile ;
#endif

FILE *fp_ogg = NULL ;

#ifdef HAVE_MP3
mpg123_handle *fp_mp3 = NULL ;
#endif

int audiofileisopen = 0 ;
long current_ogg_or_mp3_pos ;

#define SNDFILE_TYPE 0x01
#define OGG_TYPE 0x02
#define MP3_TYPE 0x04

int audio_type ;


extern struct view audio_view ;
extern struct sound_prefs prefs ;
extern struct encoding_prefs encoding_prefs;

int current_sample ;

void position_wavefile_pointer(long sample_number) ;

void audio_normalize(int flag)
{
    if(audio_type == SNDFILE_TYPE) {
	if(flag == 0)
	    sf_command(sndfile, SFC_SET_NORM_DOUBLE, NULL, SF_FALSE) ;
	else
	    sf_command(sndfile, SFC_SET_NORM_DOUBLE, NULL, SF_TRUE) ;
    }
}

void write_wav_header(int thefd, int speed, long bcount, int bits, int stereo)
{
    /* Spit out header here... */
	    wavhead header;

	    char *riff = "RIFF";
	    char *wave = "WAVE";
	    char *fmt = "fmt ";
	    char *data = "data";

	    memcpy(&(header.main_chunk), riff, 4);
	    header.length = sizeof(wavhead) - 8 + bcount;
	    memcpy(&(header.chunk_type), wave, 4);

	    memcpy(&(header.sub_chunk), fmt, 4);
	    header.sc_len = 16;
	    header.format = 1;
	    header.modus = stereo + 1;
	    header.sample_fq = speed;
	    header.byte_p_sec = ((bits > 8)? 2:1)*(stereo+1)*speed;
/* Correction by J.A. Bezemer: */
	    header.byte_p_spl = ((bits > 8)? 2:1)*(stereo+1);
    /* was: header.byte_p_spl = (bits > 8)? 2:1; */

	    header.bit_p_spl = bits;

	    memcpy(&(header.data_chunk), data, 4);
	    header.data_length = bcount;
	    write(thefd, &header, sizeof(header));
}

void config_audio_device(int rate_set, int bits_set, int stereo_set)
{
    AUDIO_FORMAT format,format_set;
    int channels ;
/*      int fragset = 0x7FFF000F ;  */

    bits_set = 16 ;
    /* play everything as 16 bit, signed integers */
    /* using the appropriate endianness */

/* Alister: I have swapped this around as an intel mac seems to think */
/* the first test is true regardless of whether you test for BE or LE */
/* Presumably it might fail on a powerpc mac now?                     */
/* Also, does it break other platforms (since LE is normal these days */
/* it should really stay default                                      */
#if __BYTE_ORDER == __LITTLE_ENDIAN
    format_set = GWC_S16_LE ;
#elif __BYTE_ORDER == __BIG_ENDIAN
    format_set = GWC_S16_BE ;
#else
    format_set = GWC_S16_LE ;
#endif

    rate = rate_set ;
    audio_bits = bits_set ;
    stereo = stereo_set ;
    format = format_set ;

/*      if(ioctl(audio_fd, SNDCTL_DSP_SETFRAGMENT, &fragset) == -1) {  */
/*  	warning("error setting buffer size on audio device") ;  */
/*      }  */

    channels = stereo + 1 ;
// Alister: Eh?  Isn't this set above?
    rate = rate_set ;

    if (audio_device_set_params(&format_set, &channels, &rate) == -1) {
	warning("unknown error setting device parameter") ;
    }

    if(format != format_set) {
	char *buf_fmt_str ;
	char buf[85] ;
	switch(format_set) {
	    case GWC_U8 : buf_fmt_str = "8 bit (unsigned)" ; bits_set = 8 ; break ;
	    case GWC_S8 : buf_fmt_str = "8 bit (signed)" ; bits_set = 8 ; break ;
	    case GWC_S16_BE :
	    case GWC_S16_LE : buf_fmt_str = "16 bit" ; bits_set = 16 ; break ;
	    default : buf_fmt_str = "unknown!" ; bits_set = 8 ; break ;
	}
        snprintf(buf, sizeof(buf), "Set bits to %s - does your soundcard support what you requested?\n", buf_fmt_str) ;
	warning(buf) ;
    }

    if(channels != stereo + 1) {
	char buf[80] ;
	if(stereo == 0)
	    snprintf(buf, sizeof(buf), "Failed to set mono mode\nYour sound card may not support mono\n") ;
	else
	    snprintf(buf, sizeof(buf), "Failed to set stereo mode\nYour sound card may not support stereo\n") ;
	warning(buf) ;
    }
    stereo_set = channels - 1 ;

// Alister: eh? does this make sense if rate has been set from rate_set above?
    if(ABS(rate_set-rate) > 10) {
	char buf[80] ;
	snprintf(buf, sizeof(buf), "Rate set to %d instead of %d\nYour sound card may not support the desired rate\n",
	             rate_set, rate) ;
	warning(buf) ;
    }

// Alister: Eh?  Isn't this set above?
    rate = rate_set ;
    audio_bits = bits_set ;
    stereo = stereo_set ;
}

long playback_samples_remaining = 0 ;
long playback_total_bytes ;
int playback_bytes_per_block ;
int looped_count ;

#define MAXBUFSIZE 32768
int BUFSIZE ;
unsigned char audio_buffer[MAXBUFSIZE] ;
unsigned char audio_buffer2[MAXBUFSIZE] ;

long playback_start_position ;
long playback_end_position ;
long playback_position ;
long first_playback_sample ;

long set_playback_cursor_position(struct view *v, long millisec_per_visual_frame)
{
    long first, last ;

    if(audio_state == AUDIO_IS_PLAYBACK) {
	long bytes = audio_device_processed_bytes()-looped_count*playback_total_bytes ;
	get_region_of_interest(&first, &last, v) ;

	v->cursor_position = first_playback_sample+bytes/(PLAYBACK_FRAMESIZE) ;

	return playback_total_bytes - bytes ;
    }

    {
	long inc = rate*millisec_per_visual_frame/1000 ;
/*  	g_print("inc:%ld\n", inc) ;  */
	v->cursor_position += inc ;
	return 1 ;
    }
    
}

long start_playback(char *output_device, struct view *v, struct sound_prefs *p, double seconds_per_block, double seconds_to_preload)
{
    long first, last ;
    long playback_samples ;
    gfloat lv, rv ;

    if(audio_type == SNDFILE_TYPE && sndfile == NULL) return 1 ;
#ifdef HAVE_OGG
    if(audio_type == OGG_TYPE && fp_ogg == NULL) return 1 ;
#endif

    audio_device_close(1) ;

    if (audio_device_open(output_device) == -1) {
	char buf[255] ;
	snprintf(buf, sizeof(buf), "Failed to open OSS audio output device %s, check settings->miscellaneous for device information", output_device) ;
#ifdef HAVE_ALSA
	snprintf(buf, sizeof(buf), "Failed to open alsa output device %s, check settings->miscellaneous for device information", output_device) ;
#endif
#ifdef HAVE_PULSE_AUDIO
	snprintf(buf, sizeof(buf), "Failed to open Pulse audio output device, recommend internet search about pulse audio configuration for your OS") ;
#endif
	warning(buf) ;
	return 0 ;
    }

    get_region_of_interest(&first, &last, v) ;

/*      g_print("first is %ld\n", first) ;  */
/*      g_print("last is %ld\n", last) ;  */
/*      g_print("rate is %ld\n", (long)p->rate) ;  */

    first_playback_sample = first ;

    playback_start_position =  first ;
    playback_end_position = last+1;
    playback_position = playback_start_position ;
    playback_samples = p->rate*seconds_per_block ;
    playback_bytes_per_block = playback_samples*PLAYBACK_FRAMESIZE ;

    //  This was moved down 8 lines to make it work in OS X.  Rob
    config_audio_device(p->rate, p->playback_bits, p->stereo);	//Set up the audio device.
    //stereo is 1 if it is stereo
    //playback_bits is the number of bits per sample
    //rate is the number of samples per second

    BUFSIZE = audio_device_best_buffer_size(playback_bytes_per_block);

    playback_bytes_per_block = BUFSIZE ;

    if(playback_bytes_per_block > MAXBUFSIZE) {
	playback_bytes_per_block = MAXBUFSIZE ;
    }

    playback_samples = playback_bytes_per_block/PLAYBACK_FRAMESIZE ;

    BUFSIZE = playback_bytes_per_block ;

    playback_samples_remaining = (last-first+1) ;
    playback_total_bytes = playback_samples_remaining*PLAYBACK_FRAMESIZE ;

    audio_state = AUDIO_IS_PLAYBACK ;

    position_wavefile_pointer(playback_start_position) ;
/*      g_print("playback_start_position is %ld\n", playback_start_position) ;  */

    /* put some data in the buffer queues, to avoid underflows */
    if(0) {
	int n = (int)(seconds_to_preload / seconds_per_block+0.5) ;
	int old_playback_bytes = playback_bytes_per_block ;

	playback_bytes_per_block *= n ;
	if(playback_bytes_per_block > MAXBUFSIZE) playback_bytes_per_block = MAXBUFSIZE ;
	process_audio(&lv, &rv) ;
	v->cursor_position = first+playback_bytes_per_block/(PLAYBACK_FRAMESIZE) ;
	playback_bytes_per_block = old_playback_bytes ;
    }

/*      g_print("playback_samples is %ld\n", playback_samples) ;  */
/*      g_print("BUFSIZE %ld (%lg fragments)\n", (long)BUFSIZE, (double)BUFSIZE/(double)oss_info.fragsize) ;  */

    v->prev_cursor_position = -1 ;
    looped_count = 0 ;

    return playback_samples ;
}

void *wavefile_data ;
#ifdef TRUNCATE_OLD
void truncate_wavfile(struct view *v)
{
#define REALLY_TRUNCATE
#ifndef REALLY_TRUNCATE
    warning("Truncation temporarily disabled, while incorporating libsndfile...") ;
#else
    /* we must do 3 things:
	1. Shift all samples forward by v->truncate_head
	2. Rescan the sample blocks (along the way)
	3. Physically truncate the size of the file on 
	   the filesystem by (v->truncate_head + (n_samples-1)-v->truncate_tail) samples
    */

    long prev ;
    long new ;
    int n_in_buf ;
    long first, last ;

#define TMPBUFSIZE     (SBW*1000)
    long left[TMPBUFSIZE], right[TMPBUFSIZE] ;

    push_status_text("Truncating audio data") ;
    update_progress_bar(0.0, PROGRESS_UPDATE_INTERVAL, TRUE) ;

    /* something like this, gotta buffer this or the disk head will
       burn a hole in the platter */

    if(v->truncate_head > 0) {

	for(prev = v->truncate_head ; prev <= v->truncate_tail ; prev += TMPBUFSIZE) {
	    update_progress_bar((gfloat)(prev-v->truncate_head)/(gfloat)(v->truncate_tail-v->truncate_head), PROGRESS_UPDATE_INTERVAL, FALSE) ;
	    last = MIN((prev+TMPBUFSIZE-1), v->truncate_tail) ;
	    n_in_buf = read_wavefile_data(left, right, prev, last) ;
	    new = prev - v->truncate_head ;
	    first = new ;
	    last = new + n_in_buf - 1 ;
	    write_wavefile_data(left, right, first, last) ;
	    resample_audio_data(&prefs, first, last) ;
	}

    }

    prefs.n_samples = v->truncate_tail - v->truncate_head + 1 ;

    if(1) save_sample_block_data(&prefs) ;

    if(1) {
	sf_count_t total_samples =  prefs.n_samples ;
	if(sf_command(sndfile, SFC_FILE_TRUNCATE, &total_samples, sizeof(total_samples)))
	    warning("Libsndfile reports truncation of audio file failed") ;
    }

    pop_status_text() ;

#endif
}
#endif /* !TRUNCATE_OLD */

void sndfile_truncate(long n_samples)
{
    sf_count_t total_samples =  n_samples ;
    if(sf_command(sndfile, SFC_FILE_TRUNCATE, &total_samples, sizeof(total_samples)))
	warning("Libsndfile reports truncation of audio file failed") ;
}

int close_wavefile(struct view *v)
{
    if(audio_type == SNDFILE_TYPE) {
#ifdef TRUNCATE_OLD
	int r ;

	if(v->truncate_head > 0 || v->truncate_tail < v->n_samples -1) {
	    r = yesnocancel("Part of the waveform is selected for truncation, do you really want to truncate?") ;
	    if(r == 2) return 0 ;
	    if(r == 0) truncate_wavfile(v) ;
	}
#endif /* TRUNCATE_OLD */
	if(sndfile != NULL) {
	    sf_close(sndfile) ;
	}
	audio_device_close(0) ;
	sndfile = NULL ;
#ifdef HAVE_OGG
    } else if(audio_type == OGG_TYPE) {
	if(fp_ogg != NULL) {
	    ov_clear(&oggfile) ;
	}
	fp_ogg = NULL ;
#endif
#ifdef HAVE_MP3
    } else if(audio_type == MP3_TYPE) {
	if(fp_mp3 != NULL) {
	    mpg123_close(fp_mp3) ;
	}
	fp_mp3 = NULL ;
#endif
    }

    return 1 ;
}

void save_as_wavfile(char *filename_new, long first_sample, long last_sample)
{
    SNDFILE *sndfile_new ;
    SF_INFO sfinfo_new ;

    long total_samples ;
    long total_bytes ;

    total_samples =  last_sample-first_sample+1 ;

    if(total_samples < 0) {
	warning("Invalid selection") ;
	return ;
    }

    total_bytes = total_samples*FRAMESIZE ;

    sfinfo_new = sfinfo ;
    sfinfo_new.frames = total_samples ;

    if (! (sndfile_new = sf_open (filename_new, SFM_WRITE, &sfinfo_new))) {
	/* Open failed so print an error message. */

	char buf[PATH_MAX] ;
	snprintf(buf, sizeof(buf), "Cannot write to %s: %s", filename_new, strerror(errno)) ;
	warning(buf) ;
	return ;
    } ;

    push_status_text("Saving selection") ;


    /* something like this, gotta buffer this or the disk head will
       burn a hole in the platter */

    position_wavefile_pointer(first_sample) ;

    {
	long n_copied  ;

#define TMPBUFSIZE     (SBW*1000)
	unsigned char buf[TMPBUFSIZE] ;
	long framebufsize = (TMPBUFSIZE/FRAMESIZE) * FRAMESIZE ;

	update_progress_bar(0.0,PROGRESS_UPDATE_INTERVAL,TRUE) ;

	for(n_copied = 0 ; n_copied < total_bytes ; n_copied += framebufsize) {
	    long n_to_copy = framebufsize ;

#ifdef MAC_OS_X /* MacOSX */
	    usleep(2) ; // prevents segfault on OSX, who knows, something to do with status bar update...
#endif

	    update_progress_bar((gfloat)(n_copied)/(gfloat)(total_bytes),PROGRESS_UPDATE_INTERVAL,FALSE) ;

	    if(n_copied + n_to_copy > total_bytes) n_to_copy = total_bytes - n_copied ;

	    n_to_copy = sf_read_raw(sndfile, buf, n_to_copy) ;
	    sf_write_raw(sndfile_new, buf, n_to_copy) ;
	}

    }

    update_progress_bar((gfloat)0.0,PROGRESS_UPDATE_INTERVAL,TRUE) ;

    sf_close(sndfile_new) ;

    pop_status_text() ;

}

void save_selection_as_wavfile(char *filename_new, struct view *v)
{
    SNDFILE *sndfile_new ;
    SF_INFO sfinfo_new ;

    long total_samples ;
    long total_bytes ;

    total_samples =  v->selected_last_sample-v->selected_first_sample+1 ;

    if(total_samples < 0 || total_samples > v->n_samples) {
	warning("Invalid selection") ;
	return ;
    }

    save_as_wavfile(filename_new, v->selected_first_sample, v->selected_last_sample) ;
}

#ifdef HAVE_MP3
int gwc_mpg123_open(char *filename)
{
    int r ;

    mpg123_init() ;

    fp_mp3 = mpg123_new(NULL,NULL) ;

    r = mpg123_open(fp_mp3,filename) ;

    if(r != MPG123_OK) {
	mpg123_delete(fp_mp3) ;
	fp_mp3 = NULL ;
    }

    return r ;
}

int gwc_mpg123_close(void)
{
    mpg123_close(fp_mp3) ;
    mpg123_delete(fp_mp3) ;
    mpg123_exit() ;
    return 0 ;
}
#endif


int is_valid_audio_file(char *filename)
{
    SNDFILE *sndfile ;
    SF_INFO sfinfo ;

    sfinfo.format = 0 ;

#ifdef HAVE_OGG
    if((fp_ogg = fopen(filename, "r")) != NULL) {
        if(ov_open(fp_ogg, &oggfile, NULL, 0) < 0) {
            fclose(fp_ogg) ;
            fp_ogg = NULL ;
        } else {
	    ov_clear(&oggfile) ;
            fclose(fp_ogg) ;
	    fp_ogg = NULL ;
	    audio_type = OGG_TYPE ;
	    return 1 ;
	}
    }
#endif


#ifdef HAVE_MP3
    if(gwc_mpg123_open(filename) == MPG123_OK) {
	gwc_mpg123_close() ;
        fp_mp3 = NULL ;

        audio_type = MP3_TYPE ;
        return 1 ;
    }
#endif

    if((sndfile = sf_open(filename, SFM_RDWR, &sfinfo)) != NULL) {
	sf_close(sndfile) ;
	audio_type = SNDFILE_TYPE ;
	return 1 ;
    } else {
	char buf[180+PATH_MAX] ;
	snprintf(buf, sizeof(buf), "Failed to open  %s, \'%s\'", filename, sf_strerror(NULL)) ;
	warning(buf) ;
    }

    return 0 ;
}


struct sound_prefs open_wavefile(char *filename, struct view *v)
{
    struct sound_prefs wfh ;

    /* initialize all wfh structure members to defaults.  Will be overwritten on successful file open */
    wfh.rate = 44100 ;
    wfh.n_channels = 2 ;
    wfh.stereo = 1 ;
    wfh.n_samples = 2 ;
    wfh.playback_bits = wfh.bits = 16 ;
    wfh.max_allowed = MAXSAMPLEVALUE-1 ;
    wfh.wavefile_fd = wavefile_fd ;
    wfh.sample_buffer_exists = FALSE ;

    if(close_wavefile(v)) {
	wfh.successful_open = TRUE ;
    } else {
	wfh.successful_open = FALSE ;
	return wfh ;
    }


    if(audio_type == SNDFILE_TYPE) {
	if (! (sndfile = sf_open (filename, SFM_RDWR, &sfinfo))) {
	    /* Open failed so print an error message. */

	    char buf[80+PATH_MAX] ;
	    snprintf(buf, sizeof(buf), "Failed to open  %s, no permissions or unknown audio format", filename) ;
	    warning(buf) ;
	    wfh.successful_open = FALSE ;
	    return wfh ;

	    /* Print the error message from libsndfile. */
    /*          sf_perror (NULL) ;  */
    /*          return  1 ;  */
	} ;
    }

#ifdef HAVE_OGG
    if(audio_type == OGG_TYPE) {
        if((fp_ogg = fopen(filename, "r")) != NULL) {
            if(ov_open(fp_ogg, &oggfile, NULL, 0) < 0) {
                /* Open failed so print an error message. */

                char buf[80+PATH_MAX] ;
                snprintf(buf, sizeof(buf), "Failed to open  %s", filename) ;
                warning(buf) ;
		wfh.successful_open = FALSE ;
		fclose(fp_ogg) ;
		fp_ogg = NULL ;
                return wfh ;
            }
        }

    }
#endif

#ifdef HAVE_MP3
    if(audio_type == MP3_TYPE) {
	if(gwc_mpg123_open(filename) != MPG123_OK) {
	    /* Open failed so print an error message. */

	    char buf[80+PATH_MAX] ;
	    snprintf(buf, sizeof(buf), "Failed to open  %s", filename) ;
	    warning(buf) ;
	    wfh.successful_open = FALSE ;
	    fp_mp3 = NULL ;
	    return wfh ;
        }

    }
#endif


    wfh.wavefile_fd = 1 ;


    if(audio_type == SNDFILE_TYPE) {
	/* determine soundfile properties */
	wfh.rate = sfinfo.samplerate ;
	wfh.n_channels = sfinfo.channels ;
	wfh.stereo = stereo = sfinfo.channels-1 ;

	wfh.n_samples = sfinfo.frames ;
	
	switch(sfinfo.format & 0x00000F) {
	    case SF_FORMAT_PCM_U8 : BYTESPERSAMPLE=1 ; MAXSAMPLEVALUE = 1 << 8 ; break ;
	    case SF_FORMAT_PCM_S8 : BYTESPERSAMPLE=1 ; MAXSAMPLEVALUE = 1 << 7 ; break ;
	    case SF_FORMAT_PCM_16 : BYTESPERSAMPLE=2 ; MAXSAMPLEVALUE = 1 << 15 ; break ;
	    case SF_FORMAT_PCM_24 : BYTESPERSAMPLE=3 ; MAXSAMPLEVALUE = 1 << 23 ; break ;
	    case SF_FORMAT_PCM_32 : BYTESPERSAMPLE=4 ; MAXSAMPLEVALUE = 1 << 31 ; break ;
	    default : warning("Soundfile format not allowed") ; break ;
	}

	/* do some simple error checking on the wavfile header , so we don't seek data where it isn't */
	if(wfh.n_samples < 2) {
	    char tmp[140] ;
	    snprintf(tmp, sizeof(tmp), "Audio file is possibly corrupt, only %ld samples reported by audio header", wfh.n_samples) ;
	    info(tmp) ;
	    if(sndfile != NULL) {
		sf_close(sndfile) ;
	    }
	    wfh.successful_open = FALSE ;
	    return wfh ;
	}
    }

#ifdef HAVE_OGG
    if(audio_type == OGG_TYPE) {
        vorbis_info *vi = ov_info(&oggfile,-1) ;
        wfh.rate = vi->rate ;
        wfh.n_channels = vi->channels ;
        wfh.stereo = stereo = vi->channels-1 ;

        wfh.n_samples = ov_pcm_total(&oggfile,-1) ;

        BYTESPERSAMPLE=2 ;
        MAXSAMPLEVALUE = 1 << 15 ;

        current_ogg_or_mp3_pos = 0 ;
	fprintf(stderr, "Oggfile: FRAMESIZE=%d\n", BYTESPERSAMPLE*wfh.n_channels) ;
	wfh.successful_open = TRUE ;
    }
#endif

#ifdef HAVE_MP3
    if(audio_type == MP3_TYPE) {
	long rate ;
	int channels ;
	int encoding ;
	mpg123_getformat(fp_mp3,&rate,&channels,&encoding) ;
	mpg123_scan(fp_mp3) ;
        wfh.n_samples = mpg123_length(fp_mp3) ;
        wfh.rate = rate ;
        wfh.n_channels = channels ;
        wfh.stereo = stereo = channels-1 ;


        BYTESPERSAMPLE=2 ;
        MAXSAMPLEVALUE = 1 << 15 ;

	off_t pos = mpg123_tell(fp_mp3) ;
	off_t curr_frame = mpg123_tellframe(fp_mp3) ;

        current_ogg_or_mp3_pos = 0 ;
	fprintf(stderr, "Mp3file: FRAMESIZE=%d, pos=%d, frame=%d\n", BYTESPERSAMPLE*wfh.n_channels, (int)pos, (int)curr_frame) ;
	wfh.successful_open = TRUE ;
    }
#endif

    FRAMESIZE = BYTESPERSAMPLE*wfh.n_channels ;
    PLAYBACK_FRAMESIZE = 2*wfh.n_channels ;

    wfh.playback_bits = audio_bits = wfh.bits = BYTESPERSAMPLE*8 ;

    wfh.max_allowed = MAXSAMPLEVALUE-1 ;

    gwc_window_set_title(filename) ;

    return wfh ;
}

void position_wavefile_pointer(long sample_number)
{
    if(audio_type == SNDFILE_TYPE) {
	sf_seek(sndfile, sample_number, SEEK_SET) ;
    } else if(audio_type == MP3_TYPE) {
#ifdef HAVE_MP3
        if(current_ogg_or_mp3_pos != sample_number) {
	    off_t new_pos ;
            current_ogg_or_mp3_pos = sample_number ;
	    if(sample_number != 0) {
		unsigned char buf[1152*4] ;
		new_pos = mpg123_seek(fp_mp3,sample_number,SEEK_SET) ;
		off_t curr_frame = mpg123_tellframe(fp_mp3) ;
/*  		if(curr_frame > 0) curr_frame-- ;  */
		mpg123_seek_frame(fp_mp3,curr_frame,SEEK_SET) ;
		int presample_number = (int)mpg123_tell(fp_mp3) ;
/*  		if(presample_number > 0) presample_number-- ;  */
		int samples_to_read = sample_number - presample_number ;
		new_pos = mpg123_seek(fp_mp3,presample_number,SEEK_SET) ;

/*  		    fprintf(stderr, "position_wf_ptr, samples_to_read:%d > 1152!!\n", samples_to_read) ;  */
/*  		    fprintf(stderr, "curr_frame:%d presample_number:%d\n", curr_frame,presample_number) ;  */
/*  		    fprintf(stderr, "position_wf_ptr, want:%d got%d\n", (int)sample_number, (int)new_pos) ;  */
		if(samples_to_read > 1152) {
		    exit(1) ;
		}

		unsigned int done ;
		int err ;

		err = mpg123_read(fp_mp3, buf, samples_to_read*FRAMESIZE, &done) ;
	    } else {
		new_pos = mpg123_seek(fp_mp3,sample_number,SEEK_SET) ;
		nonzero_seek = 0 ;
	    }
/*  	    fprintf(stderr, "position_wf_ptr, want:%d got%d\n", (int)sample_number, (int)new_pos) ;  */
        }
#endif

    } else {
#ifdef HAVE_OGG
        if(current_ogg_or_mp3_pos != sample_number) {
            fprintf(stderr, "pos_wv_ptr, was %ld, want %ld\n", current_ogg_or_mp3_pos, sample_number) ;
            ov_pcm_seek(&oggfile, sample_number) ;
            current_ogg_or_mp3_pos = sample_number ;
        }
#endif
    }
}

int read_raw_wavefile_data(char buf[], long first, long last)
{
    long n = last - first + 1 ;
    int n_read = 0 ;
    int n_bytes_read = 0 ;
    int bufsize = n * FRAMESIZE ;

    position_wavefile_pointer(first) ;

    if(audio_type == SNDFILE_TYPE) {
	n_bytes_read = sf_read_raw(sndfile, buf, n*FRAMESIZE) ;
	return n_bytes_read/FRAMESIZE ;
    }

#ifdef HAVE_OGG
    if(audio_type == OGG_TYPE) {
	int ret ;
	while(n_read < n) {
            ret = ov_read(&oggfile, (char *)&buf[n_bytes_read], bufsize-n_bytes_read,0,2,1,&current_ogg_bitstream) ;
	    if(ret > 0) {
		n_read += ret/FRAMESIZE ;
		n_bytes_read += ret ;
	    } else {
		break ;
	    }
	}
	current_ogg_or_mp3_pos += n_read ;
	return n_read ;
    }
#endif

#ifdef HAVE_MP3
    if(audio_type == MP3_TYPE) {
	size_t done ;
	int err ;
	struct mpg123_frameinfo mi ;
	while(n_read < n) {
	    err = mpg123_read(fp_mp3, (unsigned char *)&buf[n_bytes_read], bufsize-n_bytes_read, &done) ;
	    if(err != MPG123_OK) fprintf(stderr, "read had a problem, %d\n", err) ;
	    err = mpg123_info(fp_mp3, &mi) ;
/*  	    fprintf(stderr, "fs %d\n", (int)mi.framesize) ;  */
	    n_bytes_read += done ;
	    n_read += done/FRAMESIZE ;
	}
	current_ogg_or_mp3_pos += n_read ;
	return n_read ;
    }
#endif

    return n_read ;
}

int write_raw_wavefile_data(char buf[], long first, long last)
{
    long n = last - first + 1 ;
    int n_read ;

    position_wavefile_pointer(first) ;

    n_read = sf_write_raw(sndfile, buf, n*FRAMESIZE) ;

    return n_read/FRAMESIZE ;
}

int read_wavefile_data(long left[], long right[], long first, long last)
{
    long n = last - first + 1 ;
    long s_i = 0 ;
    long bufsize_long ;
    long j ;
    int *p_int ;

    position_wavefile_pointer(first) ;
    p_int = (int *)audio_buffer ;
    bufsize_long = sizeof(audio_buffer) / sizeof(long) ;

    while(s_i < n) {
	long n_read ;

#define TRY_NEW_ABSTRACTION_NOT
#ifdef TRY_NEW_ABSTRACTION
	n_read = read_raw_wavefile_data((char *)p_int, first, last) ;
#else
	long n_this = MIN((n-s_i)*(stereo+1), bufsize_long) ;
	if(audio_type == SNDFILE_TYPE) {
	    n_read = sf_read_int(sndfile, p_int, n_this) ;
	}

#ifdef HAVE_OGG
	if(audio_type == OGG_TYPE) {
            n_read = ov_read(&oggfile, (char *)p_int, n_this*FRAMESIZE,0,2,1,&current_ogg_bitstream) ;
	    n_read /= FRAMESIZE ;
	}
#endif

#ifdef HAVE_MP3
	if(audio_type == MP3_TYPE) {
/*  	    size_t done ;  */
/*  	    int err = mpg123_read(fp_mp3, (unsigned char *)&buf[n_bytes_read], bufsize-n_bytes_read, &done) ;  */
/*  	    if(err != MPG123_OK) fprintf(stderr, "read had a problem, %d\n", err) ;  */
/*  	    n_read = done/FRAMESIZE ;  */
	}
#endif

#endif


	for(j = 0  ; j < n_read ; ) {

	    left[s_i] = p_int[j] ;
	    j++ ;

	    if(stereo) {
		right[s_i] = p_int[j] ;
		j++ ;
	    } else {
		right[s_i] = left[s_i] ;
	    }

	    s_i++ ;
	}

	if(n_read == 0) {
	    char tmp[100] ;
	    snprintf(tmp, sizeof(tmp), "Attempted to read past end of audio, first=%ld, last=%ld", first, last) ;
	    warning(tmp) ;
	    exit(1) ;
	}
    }

    return s_i ;
}

int read_fft_real_wavefile_data(fftw_real left[], fftw_real right[], long first, long last)
{
    long n = last - first + 1 ;
    long s_i = 0 ;
    long j ;

    double *buffer = (double *)audio_buffer ;

    int bufsize_double = sizeof(audio_buffer) / sizeof(double) ;

    position_wavefile_pointer(first) ;

    if(audio_type != SNDFILE_TYPE) {
	long pos = first ;
	short *buffer2 ;
	int bufsize_short ;

	buffer2 = (short *)audio_buffer2 ;
	bufsize_short = sizeof(audio_buffer2) / sizeof(short) ;

	while(s_i < n) {
	    long n_this = MIN((n-s_i), bufsize_short) ;
	    int n_read = read_raw_wavefile_data((char *)audio_buffer2, pos, pos+n_this-1) ;

	    pos += n_read ;

	    for(j = 0  ; j < n_read ; j++) {
		left[s_i] = buffer2[j] ;
		if(stereo) {
		    j++ ;
		    right[s_i] = buffer2[j] ;
		} else {
		    right[s_i] = left[s_i] ;
		}
		left[s_i] /= MAXSAMPLEVALUE ;
		right[s_i] /= MAXSAMPLEVALUE ;
		if(first == 0) {
/*  		    fprintf(stderr, "%4d %d %d\n", (int)s_i, (int)left[s_i], (int)right[s_i]) ;  */
		}
		s_i++ ;
	    }

	    if(n_read == 0) {
		char tmp[100] ;
		snprintf(tmp, sizeof(tmp), "read_fft_real Attempted to read past end of audio, first=%ld, last=%ld", first, last) ;
		warning(tmp) ;
		//exit(1) ;
	    }

	}
    } else {
	while(s_i < n) {
	    long n_this = MIN((n-s_i)*(stereo+1), bufsize_double) ;
	    long n_read = sf_read_double(sndfile, buffer, n_this) ;

	    for(j = 0  ; j < n_read ; j++) {
		left[s_i] = buffer[j] ;
		if(stereo) {
		    j++ ;
		    right[s_i] = buffer[j] ;
		} else {
		    right[s_i] = left[s_i] ;
		}
		s_i++ ;
	    }

	    if(n_read == 0) {
		char tmp[100] ;
		snprintf(tmp, sizeof(tmp), "Attempted to read past end of audio, first=%ld, last=%ld", first, last) ;
		warning(tmp) ;
		exit(1) ;
	    }

	}
    }

    return s_i ;
}

int read_float_wavefile_data(float left[], float right[], long first, long last)
{
    long n = last - first + 1 ;
    long s_i = 0 ;
    long j ;

    float *buffer = (float *)audio_buffer ;

    int bufsize_float = sizeof(audio_buffer) / sizeof(float) ;

    position_wavefile_pointer(first) ;

    while(s_i < n) {
	long n_this = MIN((n-s_i)*(stereo+1), bufsize_float) ;
	long n_read = sf_read_float(sndfile, buffer, n_this) ;

	for(j = 0  ; j < n_read ; j++) {
	    left[s_i] = buffer[j] ;
	    if(stereo) {
		j++ ;
		right[s_i] = buffer[j] ;
	    } else {
		right[s_i] = left[s_i] ;
	    }
	    s_i++ ;
	}

	if(n_read == 0) {
	    char tmp[100] ;
	    snprintf(tmp, sizeof(tmp), "Attempted to read past end of audio, first=%ld, last=%ld", first, last) ;
	    warning(tmp) ;
	    exit(1) ;
	}

    }

    return s_i ;
}

int sf_write_values(void *ptr, int n_samples)
{
    int n = 0 ;

    if(n_samples > 0) {
	n = sf_write_int(sndfile, ptr, n_samples) ;
    }

    return n * BYTESPERSAMPLE ;
}

long n_in_buf = 0 ;

int WRITE_VALUE_TO_AUDIO_BUF(char *ivalue)
{
    int i ;
    int n_written = 0 ;


    if(BYTESPERSAMPLE+n_in_buf > MAXBUFSIZE) {
	n_written = sf_write_values(audio_buffer, n_in_buf/BYTESPERSAMPLE) ;
	n_in_buf = 0 ;
    }

    for(i = 0 ; i < BYTESPERSAMPLE ; i++, n_in_buf++)
	audio_buffer[n_in_buf] = ivalue[i] ;

    return n_written ;
}

#define FLUSH_AUDIO_BUF(n_written) {\
	n_written += sf_write_values(audio_buffer, n_in_buf/BYTESPERSAMPLE) ;\
	n_in_buf = 0 ;\
    }

    

int write_fft_real_wavefile_data(fftw_real left[], fftw_real right[], long first, long last)
{
    long n = last - first + 1 ;
    long j ;
    long n_written = 0 ;

/* make SURE MAXBUF is a multiple of 2 */
#define MAXBUF 500
    double buf[MAXBUF] ;

    n_in_buf = 0 ;

    position_wavefile_pointer(first) ;

    for(j = 0  ; j < n ; j++) {
	buf[n_in_buf] = left[j] ;
	n_in_buf++ ;
	if(stereo) {
	    buf[n_in_buf] = right[j] ;
	    n_in_buf++ ;
	}

	if(n_in_buf == MAXBUF) {
	    n_written += sf_write_double(sndfile, buf, n_in_buf) ;
	    n_in_buf = 0 ;
	}
    }

    if(n_in_buf > 0) {
	n_written += sf_write_double(sndfile, buf, n_in_buf) ;
	n_in_buf = 0 ;
    }

    return n_written/2 ;
#undef MAXBUF
}

int write_float_wavefile_data(float left[], float right[], long first, long last)
{
    long n = last - first + 1 ;
    long j ;
    long n_written = 0 ;

/* make SURE MAXBUF is a multiple of 2 */
#define MAXBUF 500
    float buf[MAXBUF] ;

    n_in_buf = 0 ;

    position_wavefile_pointer(first) ;

    for(j = 0  ; j < n ; j++) {
	buf[n_in_buf] = left[j] ;
	n_in_buf++ ;
	if(stereo) {
	    buf[n_in_buf] = right[j] ;
	    n_in_buf++ ;
	}

	if(n_in_buf == MAXBUF) {
	    n_written += sf_write_float(sndfile, buf, n_in_buf) ;
	    n_in_buf = 0 ;
	}
    }

    if(n_in_buf > 0) {
	n_written += sf_write_float(sndfile, buf, n_in_buf) ;
	n_in_buf = 0 ;
    }

    return n_written/2 ;
#undef MAXBUF
}

int write_wavefile_data(long left[], long right[], long first, long last)
{
    long n = last - first + 1 ;
    long j ;
    long n_written = 0 ;
/* make SURE MAXBUF is a multiple of 2 */
#define MAXBUF 500
    int buf[MAXBUF] ;

    n_in_buf = 0 ;

    position_wavefile_pointer(first) ;

    for(j = 0  ; j < n ; j++) {
	buf[n_in_buf] = left[j] ;
	n_in_buf++ ;
	if(stereo) {
	    buf[n_in_buf] = right[j] ;
	    n_in_buf++ ;
	}

	if(n_in_buf == MAXBUF) {
	    n_written += sf_write_int(sndfile, buf, n_in_buf) ;
	    n_in_buf = 0 ;
	}
    }

    if(n_in_buf > 0) {
	n_written += sf_write_int(sndfile, buf, n_in_buf) ;
	n_in_buf = 0 ;
    }

    return n_written/2 ;
#undef MAXBUF
}

void flush_wavefile_data(void)
{
    fsync(wavefile_fd) ;
}

/* process_audio for mac_os_x is found in the audio_osx.c */
#if !defined MAC_OS_X || defined HAVE_PULSE_AUDIO
int process_audio(gfloat *pL, gfloat *pR)
{
    int len = 0 ;
    int i, frame ;
    short *p_short ;
    int *p_int ;
    unsigned char  *p_char ;
    short maxl = 0, maxr = 0 ;
    extern int audio_playback ;
    long n_samples_to_read, n_read ;
    double maxpossible ;
    double feather_out_N ;
    int feather_out = 0 ;

    *pL = 0.0 ;
    *pR = 0.0 ;
	

    if(audio_state == AUDIO_IS_IDLE) {
	d_print("process_audio says NOTHING is going on.\n") ;
	return 1 ;
    }

    if(audio_state == AUDIO_IS_RECORDING) {
	if((len = audio_device_read(audio_buffer, BUFSIZE)) == -1) {
	    warning("Error on audio read...") ;
	}
    } else if(audio_state == AUDIO_IS_PLAYBACK) {
        len = audio_device_nonblocking_write_buffer_size(
            MAXBUFSIZE, playback_samples_remaining*PLAYBACK_FRAMESIZE);

	if (len <= 0) {
	    return 0 ;
	}
    }

    n_samples_to_read = len/PLAYBACK_FRAMESIZE ;

    if(n_samples_to_read*PLAYBACK_FRAMESIZE != len)
	g_print("ACK!!\n") ;

    p_char = (unsigned char *)audio_buffer ;
    p_short = (short *)audio_buffer ;
    p_int = (int *)audio_buffer ;

    /* for now force playback to 16 bit... */
#define BYTESPERSAMPLE 2

    if(audio_type == SNDFILE_TYPE) {
	if(BYTESPERSAMPLE < 3) {
	    maxpossible = 1 << 15 ;
	    n_read = sf_readf_short(sndfile, p_short, n_samples_to_read) ;
	} else {
	    maxpossible = 1 << 23 ;
	    n_read = sf_readf_int(sndfile, p_int, n_samples_to_read) ;
	}
    } else {
#if defined(HAVE_MP3) || defined(HAVE_OGG)
	maxpossible = 1 << 15 ;
	n_read = read_raw_wavefile_data((char *)p_char, current_ogg_or_mp3_pos, current_ogg_or_mp3_pos+n_samples_to_read-1) ;
#endif
    }

#define FEATHER_WIDTH 30000
    if(playback_samples_remaining - n_read < 0) {
	feather_out = 1 ;
	feather_out_N = MIN(n_read, FEATHER_WIDTH) ;
	fprintf(stderr, "Feather out n_read=%ld, playback_samples_remaining=%ld, N=%lf\n", n_read, playback_samples_remaining, feather_out_N) ;
    }

    for(frame = 0  ; frame < n_read ; frame++) {
	int vl, vr ;
	// I don't understand how all this code works, but if I remove the multiplication by two then the level meter actually works for mono files in ALSA, and for some reason there seem to be no side effects.
	// However, note that the level meters work quite differently in different configurations, and arguably don't show anything helpful, anyway.
	// Perhaps we should multiply by (stereo + 1) though?
	// i = frame*2 ;
	i = frame;

	if(BYTESPERSAMPLE < 3) {
	    if(feather_out == 1 && n_read-(frame+1) < FEATHER_WIDTH) {
		int j = ((n_read-(frame))-1) ;
		double p = (double)(j)/feather_out_N ;

		if(i > n_read - 100) {
		    //printf("j:%d %lf %hd %hd ", j, p, p_short[i], p_short[i+1]) ;
		}

		p_short[i] *= p ;
		p_short[i+1] *= p ;

		//if(i > n_read - 100) {
		//    printf("%hd %hd\n", p_short[i], p_short[i+1]) ;
		//}

		if(frame == n_read-1) fprintf(stderr, "Feather out final %lf, n_read=%ld\n", p, n_read) ;
	    }

	    vl = p_short[i] ;
	    vr = p_short[i+1] ;

	} else {
	    if(feather_out == 1 && n_read-(i+1) < 10000) {
		double p = 1.0 - (double)(n_read-(i+1))/9999.0 ;
		printf(".") ;
		p_int[i] *= p ;
		p_int[i+1] *= p ;
	    }

	    vl = p_int[i] ;
	    vr = p_int[i+1] ;
	}

	if(vl > maxl) maxl = vl ;
	if(-vl > maxl) maxl = -vl ;

	if(stereo) {
	    if(vr > maxr) maxr = vr ;
	    if(-vr > maxr) maxr = -vr ;
	} else {
	    maxr = maxl ;
	}
    }
#undef BYTESPERSAMPLE
    if(feather_out == 1) printf("\n") ;

    *pL = (gfloat) maxl / maxpossible ;
    *pR = (gfloat) maxr / maxpossible ;

    if(audio_state == AUDIO_IS_RECORDING) {
	len = write(wavefile_fd, audio_buffer, len) ;
	audio_bytes_written += len ;
    } else if(audio_state == AUDIO_IS_PLAYBACK) {
	len = audio_device_write(p_char, len) ;
	playback_position += n_read ;
	playback_samples_remaining -= n_read ;

	if(playback_samples_remaining < 1) {
	    extern int audio_is_looping ;

	    if(audio_is_looping == FALSE) {
		unsigned char zeros[1024] ;
		long zeros_needed ;
		memset(zeros,0,sizeof(zeros)) ;
		audio_state = AUDIO_IS_PLAYBACK ;
		audio_playback = FALSE ;

		zeros_needed = playback_bytes_per_block - (playback_total_bytes % playback_bytes_per_block) ;
		if(zeros_needed < PLAYBACK_FRAMESIZE) zeros_needed = PLAYBACK_FRAMESIZE ; 
		do {
		    len = audio_device_write(zeros, MIN(zeros_needed, sizeof(zeros))) ;
		    zeros_needed -= len ;
		} while (len >= 0 && zeros_needed > 0) ;

		g_print("Stop playback with playback_samples_remaining:%ld\n", playback_samples_remaining) ;
		return 1 ;
	    } else {
		playback_position = playback_start_position ;
		playback_samples_remaining = (playback_end_position-playback_start_position) ;
		sf_seek(sndfile, playback_position, SEEK_SET) ;
		g_print("Loop with playback_samples_remaining:%ld\n", playback_samples_remaining) ;
		looped_count++ ;
	    }
	}
    }
    return 0 ;
}
#endif

void stop_playback(int force)
{
    if(!force) {
	/* Robert altered */
	int new_playback = audio_device_processed_bytes();
	int  old_playback;

	while(new_playback < playback_total_bytes) {
	    /* Robert altered */
	    usleep(100) ;
	    old_playback = new_playback;
	    new_playback=audio_device_processed_bytes();

	    /* check if more samples have been processed, if not,quit */
	    if (old_playback==new_playback){
		fprintf(stderr,"Playback appears frozen\n Breaking\n");
		break;
	    }
	}

	usleep(100) ;
    }

/*      fprintf(stderr, "Usleeping 300000\n") ;  */
/*      usleep(300000) ;  */
/*      fprintf(stderr, "Done usleeping 300000\n") ;  */

    audio_state = AUDIO_IS_IDLE ;
    audio_device_close(1-force) ;
}
