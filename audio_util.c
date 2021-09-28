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

int looped_count = 0 ; // increments when frames *played* exceeds total frames to play
int buffered_looped_count = 0 ; // increments when frames sent to output device exceeds total frames to play

void position_wavefile_pointer(long frame_number) ;

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

	    if(write(thefd, &header, sizeof(header) != sizeof(header)))
		warning("unable to write wavfile header") ;
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

    channels = stereo + 1 ;

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
// Jeff - no -- audio_device_set_params() may choose to alter (rate,format or channels) based on limitations of the audio device
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

long playback_frames_not_output = 0 ;
long playback_total_bytes ;
long playback_total_frames ;
int playback_bytes_per_block ;

#define MAXBUFSIZE 32768
int BUFSIZE ;
unsigned char audio_buffer[MAXBUFSIZE] ;
unsigned char audio_buffer2[MAXBUFSIZE] ;

long playback_start_frame ;
long playback_end_frame ;
long playback_read_frame_position ;
long prev_device_frames_played=-1 ;
extern int audio_is_looping ;

// a simple ring buffer to store the last 'n' number of frames played in the set_playback_cursor_position events
#define MAX_RINGBUFFER_SIZE 15
int rb_head=0 ;
int rb_tail=0 ;
int rb_n_elements=0 ;
long ring_buffer[MAX_RINGBUFFER_SIZE] ;

void rb_init()
{
    rb_head=0 ;
    rb_tail=0 ;
    rb_n_elements=0 ;
}

void rb_add(long x)
{
    rb_head = (rb_head+1)%MAX_RINGBUFFER_SIZE ;
    ring_buffer[rb_head] = x ;
    if(rb_head == rb_tail)
	rb_tail = (rb_tail+1)%MAX_RINGBUFFER_SIZE ;
    else
	rb_n_elements++ ;
}

long rb_element(int i)
{
    int j=(rb_tail+i)%MAX_RINGBUFFER_SIZE ;
    return ring_buffer[j] ;
}

long rb_avg_first()
{
    // compute average of all elements in the ringbuffer, excluding the last 5 added
    long sum=0 ;
    int i ;
    int n = 10 ;

    if(rb_n_elements > 10) {
	n=rb_n_elements-5 ;
    } else if(rb_n_elements > 5) {
	n=rb_n_elements-2 ;
    } else {
	n=1 ;
    }

    if(rb_n_elements == 0) return 0 ;

    for(i = 0 ; i < n ; i++) {
	sum += rb_element(i) ;
    }

    return sum/n ;
}

long set_playback_cursor_position(struct view *v, long millisec_per_visual_frame)
{
    if((audio_state&AUDIO_IS_PLAYING)) {
	// determine number of frames played thru the audio device (may have some error in it, but should be very close)
	long device_frames_played = audio_device_processed_frames() - looped_count*playback_total_frames ;
	long delta_dfp = device_frames_played-prev_device_frames_played ;

	if(!(audio_state&AUDIO_IS_BUFFERING)) {
	    long avg_dfp = rb_avg_first() ;

	    // is the device reporting too many or two few delta frames? (can happen with alsa as buffers drop below or above certain thresholds)
	    if((float)delta_dfp/(float)avg_dfp < 0.66 || (float)delta_dfp/(float)avg_dfp > 1.5) {
		//fprintf(stderr, "SPCP:override device_frames_played from %ld to %ld\n", delta_dfp, avg_dfp) ;
		device_frames_played = avg_dfp+prev_device_frames_played ;
		if(device_frames_played > playback_total_frames)
		    device_frames_played = playback_total_frames ;
	    }

	} else {
	    // add the current delta frames to the ring buffer
	    rb_add(delta_dfp) ;
	}

	long frames_played ; // holds best estimate of frames played, based on logic below.

/*  	fprintf(stderr, "timer has %12.0f FRAMES, device has %12ld FRAMES (frac=%f) delta=%d\n", v->expected_frames_played,device_frames_played,  */
/*  	    v->expected_frames_played/(float)device_frames_played, device_frames_played-prev_device_frames_played) ;  */

	prev_device_frames_played = device_frames_played ;

	// trust that the audio device is correct
	frames_played = device_frames_played ;
	if(frames_played > playback_total_frames) {
	    looped_count++ ;
	    frames_played -= playback_total_frames ;
	}

	if(frames_played >= 0) {
	    v->cursor_position = playback_start_frame+frames_played ;
	    return frames_played ;
	} else {
	    v->cursor_position = playback_end_frame ;
	    fprintf(stderr, "!!!!!!!!!!!!  processed bytes < 0!\n") ;
	    return playback_end_frame-playback_start_frame+1 ;
	}
    }

    return -1 ;
}

gfloat *led_levels_r = NULL ;
gfloat *led_levels_l = NULL ;
gint n_frames_in_led_levels ;  // number of frames of data loaded into the led_levels_r[] and led_levels_l[] block arrays
gint totblocks_in_led_levels ; // number of blocks (elements) alloc()'ed in the led_levels_r and led_levels_l arrays
gint LED_LEVEL_FRAME_SIZE = 4410 ;  // number of frames examined for max values in a single led_level element, will be reset based on prefs.rate

void get_led_levels(gfloat *pL, gfloat *pR, gfloat *pLold, gfloat *pRold, long frame_number)
{
    int block = frame_number/LED_LEVEL_FRAME_SIZE ;

    if(block > totblocks_in_led_levels-1)
	block = totblocks_in_led_levels-1 ;

    if(block < 0)
	block = 0 ;

    //fprintf(stderr, "GLL block=%d, samples_played=%ld\n", block, samples_played) ;

    *pL = led_levels_l[block] ;
    *pR = led_levels_r[block] ;
    *pLold = led_levels_l[block] ;
    *pRold = led_levels_r[block] ;

    // show largest value in past 10 blocks
    int firstblock = block-9 ;
    if(firstblock < 0)
	firstblock = 0 ;

    int i ;
    for(i=firstblock ; i <= block ; i++) {
	if(*pLold < led_levels_l[i]) *pLold = led_levels_l[i] ;
	if(*pRold < led_levels_r[i]) *pRold = led_levels_r[i] ;
    }
}

long start_playback(char *output_device, struct view *v, struct sound_prefs *p, double seconds_per_block, int *pBest_framesize)
{
    long first, last ;

    if(audio_type == SNDFILE_TYPE && sndfile == NULL) return 1 ;
#ifdef HAVE_OGG
    if(audio_type == OGG_TYPE && fp_ogg == NULL) return 1 ;
#endif

    audio_device_close(1) ;

    if (audio_device_open(output_device) == -1) {
	char buf[255] ;
#ifdef HAVE_ALSA
	snprintf(buf, sizeof(buf), "Failed to open alsa output device %s, check settings->miscellaneous for device information", output_device) ;
#else
	snprintf(buf, sizeof(buf), "Failed to open OSS audio output device %s, check settings->miscellaneous for device information", output_device) ;
#endif
#ifdef HAVE_PULSE_AUDIO
	snprintf(buf, sizeof(buf), "Failed to open Pulse audio output device, recommend internet search about pulse audio configuration for your OS") ;
#endif
	warning(buf) ;
	return -1 ;
    }

    prev_device_frames_played = -1 ;
    rb_init() ;

    // this is only used for pulse audio
    *pBest_framesize = audio_device_best_buffer_size(playback_bytes_per_block);

    get_region_of_interest(&first, &last, v) ;

/*      g_print("first is %ld\n", first) ;  */
/*      g_print("last is %ld\n", last) ;  */
/*      g_print("rate is %ld\n", (long)p->rate) ;  */

    playback_start_frame =  first ;
    playback_end_frame = last ;

    playback_read_frame_position = playback_start_frame ;

/*      playback_samples = p->rate*seconds_per_block ;  */
/*      playback_bytes_per_block = playback_samples*PLAYBACK_FRAMESIZE ;  */

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

    BUFSIZE = playback_bytes_per_block ;

    playback_frames_not_output = (last-first+1) ;
    playback_total_frames = (last-first+1) ;
    playback_total_bytes = playback_frames_not_output*PLAYBACK_FRAMESIZE ;

    audio_state = AUDIO_IS_PLAYING|AUDIO_IS_BUFFERING ;

    position_wavefile_pointer(playback_start_frame) ;

    v->prev_cursor_position = -1 ;

    looped_count = 0 ;
    buffered_looped_count = 0 ;

    // setup to get date to track max levels given a playback position
    LED_LEVEL_FRAME_SIZE = p->rate/10 ;

    if(led_levels_r != NULL) {
	fprintf(stderr, "Free-ing led_levels data\n") ;
	free(led_levels_r) ;
	free(led_levels_l) ;
	led_levels_r = NULL ;
	led_levels_l = NULL ;
    }


    n_frames_in_led_levels=0 ;

    totblocks_in_led_levels = (last-first+1)/LED_LEVEL_FRAME_SIZE+1 ;
    fprintf(stderr, "Initializing led_levels data, totblocks=%d\n", totblocks_in_led_levels) ;

    led_levels_r = (gfloat *)calloc(totblocks_in_led_levels, sizeof(gfloat)) ;
    led_levels_l = (gfloat *)calloc(totblocks_in_led_levels, sizeof(gfloat)) ;

    int i ;
    for(i=0 ; i < totblocks_in_led_levels ; i++) {
	led_levels_r[i] = 0. ;
	led_levels_l[i] = 0. ;
    }

    return first ;
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

void save_as_wavfile(char *filename_new, long first_frame, long last_frame)
{
    SNDFILE *sndfile_new ;
    SF_INFO sfinfo_new ;

    long total_frames ;
    long total_bytes ;

    total_frames =  last_frame-first_frame+1 ;

    if(total_frames < 0) {
	warning("Invalid selection") ;
	return ;
    }

    total_bytes = total_frames*FRAMESIZE ;

    sfinfo_new = sfinfo ;
    sfinfo_new.frames = total_frames ;

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

    position_wavefile_pointer(first_frame) ;

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
    long total_frames ;

    total_frames =  v->selected_last_sample-v->selected_first_sample+1 ;

    if(total_frames < 0 || total_frames > v->n_samples) {
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
    PLAYBACK_FRAMESIZE = BYTESPERSAMPLE*wfh.n_channels ;

    wfh.playback_bits = audio_bits = wfh.bits = BYTESPERSAMPLE*8 ;

    wfh.max_allowed = MAXSAMPLEVALUE-1 ;

    gwc_window_set_title(filename) ;

    return wfh ;
}

void position_wavefile_pointer(long frame_number)
{
    if(audio_type == SNDFILE_TYPE) {
	sf_seek(sndfile, frame_number, SEEK_SET) ;
    } else if(audio_type == MP3_TYPE) {
#ifdef HAVE_MP3
        if(current_ogg_or_mp3_pos != frame_number) {
	    off_t new_pos ;
            current_ogg_or_mp3_pos = frame_number ;
	    if(frame_number != 0) {
		unsigned char buf[1152*4] ;
		new_pos = mpg123_seek(fp_mp3,frame_number,SEEK_SET) ;
		off_t curr_frame = mpg123_tellframe(fp_mp3) ;
/*  		if(curr_frame > 0) curr_frame-- ;  */
		mpg123_seek_frame(fp_mp3,curr_frame,SEEK_SET) ;
		int preframe_number = (int)mpg123_tell(fp_mp3) ;
/*  		if(preframe_number > 0) preframe_number-- ;  */
		int frames_to_read = frame_number - preframe_number ;
		new_pos = mpg123_seek(fp_mp3,preframe_number,SEEK_SET) ;

/*  		    fprintf(stderr, "position_wf_ptr, samples_to_read:%d > 1152!!\n", samples_to_read) ;  */
/*  		    fprintf(stderr, "curr_frame:%d presample_number:%d\n", curr_frame,presample_number) ;  */
/*  		    fprintf(stderr, "position_wf_ptr, want:%d got%d\n", (int)sample_number, (int)new_pos) ;  */
		if(frames_to_read > 1152) {
		    exit(1) ;
		}

		unsigned int done ;
		int err ;

		err = mpg123_read(fp_mp3, buf, frames_to_read*FRAMESIZE, &done) ;
	    } else {
		new_pos = mpg123_seek(fp_mp3,frame_number,SEEK_SET) ;
		nonzero_seek = 0 ;
	    }
/*  	    fprintf(stderr, "position_wf_ptr, want:%d got%d\n", (int)sample_number, (int)new_pos) ;  */
        }
#endif

    } else {
#ifdef HAVE_OGG
        if(current_ogg_or_mp3_pos != frame_number) {
            fprintf(stderr, "pos_wv_ptr, was %ld, want %ld\n", current_ogg_or_mp3_pos, frame_number) ;
            ov_pcm_seek(&oggfile, frame_number) ;
            current_ogg_or_mp3_pos = frame_number ;
        }
#endif
    }
}

int read_raw_wavefile_data(char buf[], long first_frame, long last_frame)
{
    long n = last_frame - first_frame + 1 ;
    int n_read = 0 ;
    int n_bytes_read = 0 ;

    position_wavefile_pointer(first_frame) ;

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

int write_raw_wavefile_data(char buf[], long first_frame, long last_frame)
{
    long n = last_frame - first_frame + 1 ;
    int n_read ;

    position_wavefile_pointer(first_frame) ;

    n_read = sf_write_raw(sndfile, buf, n*FRAMESIZE) ;

    return n_read/FRAMESIZE ;
}

int read_wavefile_data(long left[], long right[], long first_frame, long last_frame)
{
    long n_frames_wanted = last_frame - first_frame + 1 ;
    long s_i = 0 ;
    long bufsize_long ;
    long j ;
    int *p_int ;

    position_wavefile_pointer(first_frame) ;
    p_int = (int *)audio_buffer ;
    bufsize_long = sizeof(audio_buffer) / sizeof(long) ;

    while(s_i < n_frames_wanted) {
	long n_frames_read = 0 ;

#define TRY_NEW_ABSTRACTION_NOT
#ifdef TRY_NEW_ABSTRACTION
	n_frames_read = read_raw_wavefile_data((char *)p_int, first_frame, last_frame) ;
#else
	long n_this = MIN((n_frames_wanted-s_i)*(stereo+1), bufsize_long) ;
	if(audio_type == SNDFILE_TYPE) {
	    n_frames_read = sf_read_int(sndfile, p_int, n_this) ;
	}

#ifdef HAVE_OGG
	if(audio_type == OGG_TYPE) {
            long n_bytes_read = ov_read(&oggfile, (char *)p_int, n_this*FRAMESIZE,0,2,1,&current_ogg_bitstream) ;
	    n_frames_read = n_bytes_read/FRAMESIZE ;
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


	for(j = 0  ; j < n_frames_read ; ) {

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

	if(n_frames_read == 0) {
	    char tmp[100] ;
	    snprintf(tmp, sizeof(tmp), "Attempted to read past end of audio, first=%ld, last=%ld", first_frame, last_frame) ;
	    warning(tmp) ;
	    exit(1) ;
	}
    }

    return s_i ;
}

int read_fft_real_wavefile_data(fftw_real left[], fftw_real right[], long first_frame, long last_frame)
{
    long n = last_frame - first_frame + 1 ;
    long s_i = 0 ;
    long j ;

    double *buffer = (double *)audio_buffer ;

    int bufsize_double = sizeof(audio_buffer) / sizeof(double) ;

    position_wavefile_pointer(first_frame) ;

    if(audio_type != SNDFILE_TYPE) {
	long pos = first_frame ;
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
		s_i++ ;
	    }

	    if(n_read == 0) {
		char tmp[100] ;
		snprintf(tmp, sizeof(tmp), "read_fft_real Attempted to read past end of audio, first=%ld, last=%ld", first_frame, last_frame) ;
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
		snprintf(tmp, sizeof(tmp), "Attempted to read past end of audio, first=%ld, last=%ld", first_frame, last_frame) ;
		warning(tmp) ;
		exit(1) ;
	    }

	}
    }

    return s_i ;
}

int read_float_wavefile_data(float left[], float right[], long first_frame, long last_frame)
{
    long n = last_frame - first_frame + 1 ;
    long s_i = 0 ;
    long j ;

    float *buffer = (float *)audio_buffer ;

    int bufsize_float = sizeof(audio_buffer) / sizeof(float) ;

    position_wavefile_pointer(first_frame) ;

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
	    snprintf(tmp, sizeof(tmp), "Attempted to read past end of audio, first=%ld, last=%ld", first_frame, last_frame) ;
	    warning(tmp) ;
	    exit(1) ;
	}

    }

    return s_i ;
}

int sf_write_values(void *ptr, int n_frames)
{
    int n = 0 ;

    if(n_frames > 0) {
	n = sf_write_int(sndfile, ptr, n_frames) ;
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

    

int write_fft_real_wavefile_data(fftw_real left[], fftw_real right[], long first_frame, long last_frame)
{
    long n = last_frame - first_frame + 1 ;
    long j ;
    long n_written = 0 ;

/* make SURE MAXBUF is a multiple of 2 */
#define MAXBUF 500
    double buf[MAXBUF] ;

    n_in_buf = 0 ;

    position_wavefile_pointer(first_frame) ;

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

int write_float_wavefile_data(float left[], float right[], long first_frame, long last_frame)
{
    long n = last_frame - first_frame + 1 ;
    long j ;
    long n_written = 0 ;

/* make SURE MAXBUF is a multiple of 2 */
#define MAXBUF 500
    float buf[MAXBUF] ;

    n_in_buf = 0 ;

    position_wavefile_pointer(first_frame) ;

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

int write_wavefile_data(long left[], long right[], long first_frame, long last_frame)
{
    long n = last_frame - first_frame + 1 ;
    long j ;
    long n_written = 0 ;
/* make SURE MAXBUF is a multiple of 2 */
#define MAXBUF 500
    int buf[MAXBUF] ;

    n_in_buf = 0 ;

    position_wavefile_pointer(first_frame) ;

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
    long n_frames_to_read, n_frames_read=0 ;
    gfloat maxpossible ;
    double feather_out_N ;
    int feather_out = 0 ;

    *pL = 0.0 ;
    *pR = 0.0 ;

    /* only continue if audio is buffering or recording */
    if((audio_state&(AUDIO_IS_BUFFERING|AUDIO_IS_RECORDING)) == 0)
	return 1 ;
	

    if(audio_state&AUDIO_IS_IDLE) {
	d_print("process_audio says NOTHING is going on.\n") ;
	return 1 ;
    }

    if(audio_state&AUDIO_IS_RECORDING) {
	if((len = audio_device_read(audio_buffer, BUFSIZE)) == -1) {
	    warning("Error on audio read...") ;
	}
    } else if(audio_state&AUDIO_IS_BUFFERING) {
        len = audio_device_nonblocking_write_buffer_size(
            MAXBUFSIZE, playback_frames_not_output*PLAYBACK_FRAMESIZE);

	if (len <= 0) {
	    return 0 ;
	}
    }

    n_frames_to_read = len/PLAYBACK_FRAMESIZE ;

    //fprintf(stderr, "process_audio: read %d bytes, %f msec of data\n", len, (float)n_frames_to_read/(float)prefs.rate*1000.) ;

    if(n_frames_to_read*PLAYBACK_FRAMESIZE != len)
	g_print("ACK!!\n") ;

    p_char = (unsigned char *)audio_buffer ;
    p_short = (short *)audio_buffer ;
    p_int = (int *)audio_buffer ;


    /* for now force playback to 16 bit... */
#define BYTESPERSAMPLE 2

    if(audio_type == SNDFILE_TYPE) {
	if(BYTESPERSAMPLE < 3) {
	    maxpossible = 1 << 15 ;
	    n_frames_read = sf_readf_short(sndfile, p_short, n_frames_to_read) ;
	} else {
	    maxpossible = 1 << 23 ;
	    n_frames_read = sf_readf_int(sndfile, p_int, n_frames_to_read) ;
	}
    } else {
#if defined(HAVE_MP3) || defined(HAVE_OGG)
	maxpossible = 1 << 15 ;
	n_frames_read = read_raw_wavefile_data((char *)p_char, current_ogg_or_mp3_pos, current_ogg_or_mp3_pos+n_frames_to_read-1) ;
#endif
    }

#define FEATHER_WIDTH 30000
    if(playback_frames_not_output - n_frames_read < 0) {
	feather_out = 1 ;
	feather_out_N = MIN(n_frames_read, FEATHER_WIDTH) ;
	fprintf(stderr, "Feather out n_read=%ld, playback_frames_not_output=%ld, N=%lf\n", n_frames_read, playback_frames_not_output, feather_out_N) ;
    }

    for(frame = 0  ; frame < n_frames_read ; frame++) {
	int vl=0, vr=0 ;

	// I don't understand how all this code works, but if I remove the multiplication by two then the level meter actually works for mono files in ALSA, and for some reason there seem to be no side effects.
	// However, note that the level meters work quite differently in different configurations, and arguably don't show anything helpful, anyway.

	// Perhaps we should multiply by (stereo + 1) though? -- JJW, Jan 18, 2021 - YES!!!!
	//  a frame is a complete set of samples, which may be only 1 sample for mono, or two samples for stereo.
	// p_short or p_int has the channels interleaved l,r,l,r or for mono just l,l,l,l
	//  it is critical to get "i" right, because the audio is being feathered, or tapered down in volume for each
	//  frame.  i is the index of the index of the first sample in a frame, and must increment by 2 for stereo, and increment by 1 for mono
	i = frame*(stereo+1) ;



	if(BYTESPERSAMPLE < 3) {
	    if(feather_out == 1 && n_frames_read-(frame+1) < FEATHER_WIDTH) {
		int j = ((n_frames_read-(frame))-1) ;
		double p = (double)(j)/feather_out_N ;

		if(i > n_frames_read - 100) {
		    //printf("j:%d %lf %hd %hd ", j, p, p_short[i], p_short[i+1]) ;
		}

		p_short[i] *= p ;
		if(stereo) p_short[i+1] *= p ;

		//if(i > n_read - 100) {
		//    printf("%hd %hd\n", p_short[i], p_short[i+1]) ;
		//}

		if(frame == n_frames_read-1) fprintf(stderr, "Feather out final %lf, n_frames_read=%ld\n", p, n_frames_read) ;
	    }

	    vl = p_short[i] ;
	    if(stereo) vr = p_short[i+1] ;

	} else {
	    if(feather_out == 1 && n_frames_read-(i+1) < 10000) {
		double p = 1.0 - (double)(n_frames_read-(i+1))/9999.0 ;
		printf(".") ;
		p_int[i] *= p ;
		if(stereo) p_int[i+1] *= p ;
	    }

	    vl = p_int[i] ;
	    if(stereo) vr = p_int[i+1] ;
	}

	if(audio_is_looping == FALSE || buffered_looped_count == 0) {
	    // if looping, only need to do this for first time through the loop
	    int current_level_block=(n_frames_in_led_levels-1)/LED_LEVEL_FRAME_SIZE ;
	    if(current_level_block > totblocks_in_led_levels-1) {
		fprintf(stderr, "Uh-oh, current_level_block:%d, max:%d\n", current_level_block, totblocks_in_led_levels-1) ;
		current_level_block = totblocks_in_led_levels-1 ;
	    }

	    if(vl < 0) vl = -vl ;

	    if(led_levels_l[current_level_block] < (gfloat)vl/maxpossible)
		led_levels_l[current_level_block] = (gfloat)vl/maxpossible ;

	    if(stereo) {
		if(vr < 0) vr = -vr ;
		if(led_levels_r[current_level_block] < (gfloat)vr/maxpossible)
		    led_levels_r[current_level_block] = (gfloat)vr/maxpossible ;
	    } else {
		led_levels_r[current_level_block] = led_levels_l[current_level_block] ;
	    }

	    n_frames_in_led_levels++ ;
	}
    }

#undef BYTESPERSAMPLE
    if(feather_out == 1) printf("\n") ;

    if(audio_state&AUDIO_IS_RECORDING) {
	len = write(wavefile_fd, audio_buffer, len) ;
	audio_bytes_written += len ;
    } else if(audio_state&AUDIO_IS_BUFFERING) {
	len = audio_device_write(p_char, len) ;
	playback_read_frame_position += n_frames_read ;
	playback_frames_not_output -= n_frames_read ;

	if(playback_frames_not_output < 1) {

	    if(audio_is_looping == FALSE) {
		audio_state = AUDIO_IS_PLAYING ;
		//audio_playback = FALSE ;

		if(0) {
		    unsigned char zeros[1024] ;
		    long zeros_needed ;
		    memset(zeros,0,sizeof(zeros)) ;

		    zeros_needed = playback_bytes_per_block - (playback_total_bytes % playback_bytes_per_block) ;
		    if(zeros_needed < PLAYBACK_FRAMESIZE) zeros_needed = PLAYBACK_FRAMESIZE ; 
		    do {
			len = audio_device_write(zeros, MIN(zeros_needed, sizeof(zeros))) ;
			zeros_needed -= len ;
		    } while (len >= 0 && zeros_needed > 0) ;
		}

		g_print("Stop playback audio buffering with playback_frames_not_output:%ld\n", playback_frames_not_output) ;
		return 1 ;
	    } else {
		audio_state = AUDIO_IS_PLAYING|AUDIO_IS_BUFFERING ;
		playback_read_frame_position = playback_start_frame ;
		playback_frames_not_output = (playback_end_frame-playback_start_frame+1) ;
		sf_seek(sndfile, playback_read_frame_position, SEEK_SET) ;
		g_print("Loop with playback_frames_not_output:%ld\n", playback_frames_not_output) ;
		buffered_looped_count++ ;
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
	long new_playback = audio_device_processed_bytes();
	long  old_playback;

	while(new_playback > 0 && new_playback < playback_total_bytes) {
	    /* Robert altered */
	    usleep(100) ;
	    old_playback = new_playback;
	    new_playback=audio_device_processed_bytes();
	    fprintf(stderr,"SP:total_bytes:%ld\n", playback_total_bytes);
	    fprintf(stderr,"SP:old_playback:%ld\n", old_playback);
	    fprintf(stderr,"SP:new_playback:%ld\n", new_playback);

	    /* check if more samples have been processed, if not,quit */
	    if (new_playback > 0 && old_playback==new_playback){
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
