/*****************************************************************************
*   Gnome Wave Cleaner Version 0.19
*   Copyright (C) 2003 Jeffrey J. Welty
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

/* pulse audio interface jw Nov 14, 2010 */

#include <sys/ioctl.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

#include <pulse/simple.h>
#include <pulse/error.h>
#include <pulse/gccmacro.h>

#include "audio_device.h"
#include "gwc.h"

/* The Sample format to use */
static pa_sample_spec ss = {
    .format = PA_SAMPLE_S16LE,
    .rate = 44100,
    .channels = 2
};

static pa_simple *pa_device = NULL;

static long written_frames = 0;
static int framesize ;
double frames_per_usec ;

static int last_written_size ;
static int latency_flag ;

static void pa_perr(char *text, int err)
{
    fprintf(stderr, "##########################################################\n");
    fprintf(stderr, "%s\n", text);
    fprintf(stderr, "err=%d, %s\n", err, pa_strerror(err));
    warning(text) ;
}

int audio_device_open(char *output_device)
{
    int err ;

    ss.format = PA_SAMPLE_S16LE ;
    ss.rate = 44100 ;
    ss.channels = 2 ;

    //printf("Open the Pulse audio device\n") ;
    pa_device = pa_simple_new(NULL,"GWC",PA_STREAM_PLAYBACK,NULL,"GWC Playback", &ss, NULL, NULL, &err) ;

    if (pa_device == NULL) {
        pa_perr("audio_device_open: pa_simple_new", err);
        return -1;
    }

    written_frames = 0;
    last_written_size = -1 ;
    latency_flag = 1 ;

    return 0;
}

int audio_device_set_params(AUDIO_FORMAT *format, int *channels, int *rate)
{
    ss.rate = *rate ;
    ss.channels = *channels ;

    switch (*format)
    {
	case GWC_U8:     ss.format = PA_SAMPLE_U8; framesize=1 ; break;
	case GWC_S8:     ss.format = PA_SAMPLE_ALAW; framesize=1 ; break;
	case GWC_S16_BE: ss.format = PA_SAMPLE_S16BE; framesize=2 ; break;
	default:
	case GWC_S16_LE: ss.format = PA_SAMPLE_S16LE; framesize=2 ; break;
    }

    framesize *= *channels ;

    frames_per_usec = (double)(*rate) / 1000000.0 ;

    return 0;
}

int audio_device_read(unsigned char *buffer, int buffersize)
{
    /* not implemented */
    return -1;
}

int audio_device_write(unsigned char *data, int count)
{
    int err ;

    pa_simple_write(pa_device, data, (size_t) count, &err) ;

    written_frames += count/framesize ;

    return written_frames*framesize ;
}

/* Number of bytes processed since opening the device. */
long query_processed_bytes(void)
{

    if(pa_device != NULL) {
	int err ;
	pa_usec_t latency = pa_simple_get_latency(pa_device, &err) ;
	int bytes_unprocessed = (latency*ss.rate)/1000000 * framesize ;
	if((written_frames) *framesize == last_written_size) {
	    latency_flag++ ;
	    if(latency_flag > 4) {
		bytes_unprocessed = 0 ;
	    } else {
		bytes_unprocessed /= latency_flag ;
	    }
	}

	last_written_size = (written_frames) *framesize ;
	return (written_frames) *framesize - bytes_unprocessed ;
    }

    return 0 ;
}

long _audio_device_processed_bytes = 0 ;

/* Number of bytes processed since opening the device. */
long audio_device_processed_bytes(void)
{
    if(pa_device != NULL)
	_audio_device_processed_bytes = query_processed_bytes() ;

    return _audio_device_processed_bytes ;
}

void audio_device_close(int drain)
{
    if (pa_device != NULL) {
        int err;

	//printf("Closing the Pulse audio device\n") ;

	_audio_device_processed_bytes = query_processed_bytes() ;

	if(drain)
	    err = pa_simple_drain(pa_device, &err);

	pa_simple_free(pa_device) ;

        pa_device = NULL;
    }
}

#define BEST_BUFSIZE 4096

int audio_device_best_buffer_size(int playback_bytes_per_block)
{
    return BEST_BUFSIZE ;
}

int audio_device_nonblocking_write_buffer_size(int maxbufsize,
                                               int playback_bytes_remaining)
{
    int len = BEST_BUFSIZE ;

    if (len > maxbufsize)
        len = maxbufsize;

    if (len > playback_bytes_remaining)
        len = playback_bytes_remaining;

    /*     printf("audio_device_nonblocking_write_buffer_size:%d\n", len); */

    return len;
}

