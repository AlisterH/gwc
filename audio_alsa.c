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

/* alsa interface impl.  ...frank 12.09.03 */

#include <sys/ioctl.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#ifdef ALSA_IN_SYS
#include <sys/asoundlib.h>
#else
#include <alsa/asoundlib.h>
#endif
#include "audio_device.h"
#include "gwc.h"

static snd_pcm_t *handle = NULL;
static snd_pcm_uframes_t written_frames = 0;
static long drain_delta = 0 ;
static long last_processed_bytes0 = -1 ;
static long last_processed_bytes = -1 ;
/* cached monotonic "processed bytes" value */
static long _audio_device_processed_bytes = 0 ;
/* true ALSA ring buffer size (frames). Must NOT be confused with "avail". */
static snd_pcm_uframes_t buffer_size_frames = 0 ;
static snd_pcm_uframes_t period_size_frames = 0 ;


static void snd_perr(char *text, int err)
{
    fprintf(stderr, "##########################################################\n");
    fprintf(stderr, "%s\n", text);
    fprintf(stderr, "%s\n", snd_strerror(err));
    warning(text) ;
}

int audio_device_open(char *output_device)
{
    int err = snd_pcm_open(&handle, output_device, /*"default",*/
                           SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
    if (err < 0) {
        snd_perr("ALSA audio_device_open: snd_pcm_open", err);
        return -1;
    }

    written_frames = 0;
    drain_delta=0 ;
    last_processed_bytes0 = -1 ;
    last_processed_bytes = -1 ;
    _audio_device_processed_bytes = 0 ;
    buffer_size_frames = 0 ;
    period_size_frames = 0 ;
    return 0;
}

int audio_device_set_params(AUDIO_FORMAT *format, int *channels, int *rate)
{
    unsigned int utmp ;
    int err;
    snd_pcm_format_t alsa_format;
    snd_pcm_hw_params_t *params;
    snd_pcm_sw_params_t *swparams;

    snd_pcm_hw_params_alloca(&params);
    snd_pcm_sw_params_alloca(&swparams);

    err = snd_pcm_hw_params_any(handle, params);
    if (err < 0) {
        snd_perr("ALSA audio_device_set_params: snd_pcm_hw_params_any", err);
        return -1;
    }

    err = snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0) {
        snd_perr("ALSA audio_device_set_params: snd_pcm_hw_params_set_access", err);
        return -1;
    }



    switch (*format)
    {
    case GWC_U8:     alsa_format = SND_PCM_FORMAT_U8; break;
    case GWC_S8:     alsa_format = SND_PCM_FORMAT_S8; break;
    case GWC_S16_BE: alsa_format = SND_PCM_FORMAT_S16_BE; break;
    default:
    case GWC_S16_LE: alsa_format = SND_PCM_FORMAT_S16_LE; break;
    }

    if (snd_pcm_hw_params_set_format(handle, params, alsa_format) < 0) {
        snd_perr("ALSA audio_device_set_params: snd_pcm_hw_params_set_format", err);
        return -1;
    }
    if (snd_pcm_hw_params_get_format(params, &alsa_format) < 0) {
        snd_perr("ALSA audio_device_set_params: snd_pcm_hw_params_get_format", err);
        return -1;
    }

    switch (alsa_format)
    {
    case SND_PCM_FORMAT_U8: *format = GWC_U8; break;
    case SND_PCM_FORMAT_S8 : *format = GWC_S8; break;
    case SND_PCM_FORMAT_S16_BE: *format = GWC_S16_BE; break;
    case SND_PCM_FORMAT_S16_LE: *format = GWC_S16_LE; break;
    default: *format = GWC_UNKNOWN; break;
    }



    err = snd_pcm_hw_params_set_channels(handle, params, *channels);
    if (err < 0) {
        snd_perr("ALSA audio_device_set_params: snd_pcm_hw_params_set_channels", err);
        return -1;
    }

    utmp = (unsigned int)*channels ;

    if (snd_pcm_hw_params_get_channels(params, &utmp) < 0) {
        snd_perr("ALSA audio_device_set_params: snd_pcm_hw_params_get_channels", err);
        return -1;
    }
    *channels = (int)utmp ;



    utmp = (unsigned int)*rate ;
    err = snd_pcm_hw_params_set_rate_near(handle, params, &utmp, 0);
    if (err < 0) {
        snd_perr("ALSA audio_device_set_params: snd_pcm_hw_params_set_rate_near", err);
        return -1;
    }
    *rate = (int)utmp ;


    err = snd_pcm_hw_params(handle, params);
    if (err < 0) {
        snd_perr("ALSA audio_device_set_params: snd_pcm_hw_params", err);
        return -1;
    }

    err = snd_pcm_prepare(handle);
    if (err < 0) {
        snd_perr("ALSA audio_device_set_params: snd_pcm_prepare", err);
        return -1;
    }

    /* New stream configuration => reset processed-bytes accumulator */
    _audio_device_processed_bytes = 0;

    /*
     * Cache the TRUE buffer size (frames). Do NOT use snd_pcm_status_get_avail()
     * for this; "avail" is a momentary value, not the capacity.
     */
    {
        snd_pcm_uframes_t ps = 0;
        err = snd_pcm_get_params(handle, &buffer_size_frames, &ps);
        if (err < 0) {
            snd_perr("ALSA audio_device_set_params: snd_pcm_get_params", err);
            buffer_size_frames = 0;
            period_size_frames = 0;
        } else {
            period_size_frames = ps;
        }
    }

    fprintf(stderr, "audio_device_handle %d\n",(int)handle);

    return 0;
}

int audio_device_read(unsigned char *buffer, int buffersize)
{
    /* not implemented */
    return -1;
}

/* recover underrun and suspend */
static int recover_snd_handle(int err)
{
    if (err == -EPIPE) { /* underrun */
	fprintf(stderr, "recover_snd_handle: err == -EPIPE\n");
        err = snd_pcm_prepare(handle);
        if (err < 0)
            snd_perr("ALSA recover_snd_handle: can't recover underrun, prepare failed", err);
        return 0;
    }
    else if (err == -ESTRPIPE) { /* suspend */
	fprintf(stderr, "recover_snd_handle: err == -ESTRPIPE\n");
        while ((err = snd_pcm_resume(handle)) == -EAGAIN)
            sleep(1);

        if (err < 0) {
            err = snd_pcm_prepare(handle);
            if (err < 0)
                snd_perr("ALSA recover_snd_handle: can't recover suspend, prepare failed", err);
        }
        return 0;
    }
    return err;
}

int audio_device_write(unsigned char *data, int count)
{
    snd_pcm_sframes_t err;
    snd_pcm_uframes_t result_frames = 0;
    snd_pcm_uframes_t count_frames = snd_pcm_bytes_to_frames(handle, count);

    while (count_frames > 0) {
        err = snd_pcm_writei(handle, data, count_frames);

        if (err > 0) {
            result_frames += err;
            count_frames -= err;
            data += snd_pcm_frames_to_bytes(handle, err);
        } else if (err == -EAGAIN) {
            snd_pcm_wait(handle, 1000);
        } else if (err < 0) {
	    if(err == -EINVAL) {
		fprintf(stderr, "snd_pcm_writei invalid argument: %d %d %d\n",(int)handle,(int)data,(int)count_frames);
		exit(1) ;
	    } else if (recover_snd_handle(err) < 0) {
		fprintf(stderr, "audio_device_write %d %d %d\n",(int)handle,(int)data,(int)count_frames);
                snd_perr("ALSA audio_device_write: snd_pcm_writei", err);
		exit(1) ;
                return -1;
            }
        }
    }

    written_frames += result_frames;

    return snd_pcm_frames_to_bytes(handle, result_frames);
}

/* Number of bytes processed since opening the device. */
long query_processed_bytes(void)
{
    if(handle != NULL) {

        int err;
        snd_pcm_status_t *status;
        snd_pcm_sframes_t avail_frames;
        snd_pcm_sframes_t queued_frames;
        snd_pcm_sframes_t played_frames;
        long played_bytes;

        snd_pcm_status_alloca(&status);
        err = snd_pcm_status(handle, status);
        if (err < 0) {
            if (recover_snd_handle(err) < 0) {
                snd_perr("ALSA query_processed_bytes: snd_pcm_status", err);
                return 0;
            }
            err = snd_pcm_status(handle, status);
            if (err < 0) {
                snd_perr("ALSA query_processed_bytes: snd_pcm_status (after recover)", err);
                return 0;
            }
        }

        /* Consistent snapshot of avail (frames available for writing). */
        avail_frames = (snd_pcm_sframes_t)snd_pcm_status_get_avail(status); /* see docs */

        /* Ensure we have a real buffer size cached; fallback if needed. */
        if (buffer_size_frames == 0) {
            snd_pcm_uframes_t period_size = 0;
            err = snd_pcm_get_params(handle, &buffer_size_frames, &period_size);
            if (err < 0) buffer_size_frames = 0;
        }
        if (buffer_size_frames == 0) {
            /* Can't compute queued reliably; best effort: never advance backwards */
            return _audio_device_processed_bytes;
        }

        /* queued = buffer_size - avail (clamp to sane range) */
        queued_frames = (snd_pcm_sframes_t)buffer_size_frames - avail_frames;
        if (queued_frames < 0) queued_frames = 0;
        if ((snd_pcm_uframes_t)queued_frames > buffer_size_frames)
            queued_frames = (snd_pcm_sframes_t)buffer_size_frames;

        played_frames = (snd_pcm_sframes_t)written_frames - queued_frames;
        if (played_frames < 0) played_frames = 0;
        if ((snd_pcm_uframes_t)played_frames > written_frames)
            played_frames = (snd_pcm_sframes_t)written_frames;

        played_bytes = snd_pcm_frames_to_bytes(handle, (snd_pcm_uframes_t)played_frames);

        /* Monotonic clamp: never go backwards due to jitter */
        if (played_bytes < _audio_device_processed_bytes)
            played_bytes = _audio_device_processed_bytes;

        return played_bytes;
    }

    return 0 ;
}


/* Number of bytes processed since opening the device. */
long audio_device_processed_bytes(void)
{
    if(handle != NULL)
	_audio_device_processed_bytes = query_processed_bytes() ;

    return _audio_device_processed_bytes ;
}

void audio_device_close(int drain)
{
    if (handle != NULL) {
        int err;

	printf("Closing the ALSA audio device\n") ;

	_audio_device_processed_bytes = query_processed_bytes() ;

	if(drain)
	    err = snd_pcm_drain(handle);

        err = snd_pcm_drop(handle);
        if (err < 0) {
            snd_perr("ALSA audio_device_close: snd_pcm_drop", err);
        }

        err = snd_pcm_close(handle);
        if (err < 0) {
            snd_perr("ALSA audio_device_close: snd_pcm_close", err);
        }

        handle = NULL;
    }
    drain_delta=0 ;
}

int audio_device_best_buffer_size(int playback_bytes_per_block)
{
    int frame_size = 4096;
    snd_pcm_uframes_t bs = buffer_size_frames;
    snd_pcm_uframes_t ps = period_size_frames;

    /* Ensure cached params exist (best effort) */
    if (handle != NULL && (bs == 0 || ps == 0)) {
        snd_pcm_uframes_t tmp_ps = 0;
        int err = snd_pcm_get_params(handle, &bs, &tmp_ps);
        if (err >= 0) {
            buffer_size_frames = bs;
            period_size_frames = tmp_ps;
            ps = tmp_ps;
        }
    }

    /* Use period size as a stable write quantum; fallback to 4K if unknown */
    if (handle != NULL && ps > 0)
        frame_size = snd_pcm_frames_to_bytes(handle, ps);
    else
        frame_size = 4096;

/*      fprintf(stderr, "ALSA audio_device_best_buffer_size:%d (frames:%ld)\n", frame_size, buffer_total_frames) ;  */


    if(frame_size < 4096 && frame_size > 0) {
	int s = frame_size ;
	while(frame_size < 4096) frame_size += s ;
	printf("ALSA audio_device_adjusted_buffer_size:%d\n", frame_size) ;
    }

    if(frame_size == 0) {
	warning("Your ALSA audio device driver gives invalid information for its buffer size, defaulting to 4K bytes, this may produce strange playback results") ;
	frame_size = 4096 ;
    }

    return frame_size ;
}

int audio_device_nonblocking_write_buffer_size(int maxbufsize,
                                               int playback_bytes_remaining)
{
    int len = 0;
    snd_pcm_sframes_t frames = snd_pcm_avail_update(handle);

    if (frames < 0) {
        snd_perr("audio_device_nonblocking_write_buffer_size: snd_pcm_avail_update",
             frames);

		if (recover_snd_handle(frames) < 0) {
			fprintf(stderr, "audio_device_nonblocking_write_buffer_size: could not recover handle\n");
			return -1 ;
		}

        /* Re-query after recovery */
        frames = snd_pcm_avail_update(handle);
        if (frames < 0) {
            snd_perr("audio_device_nonblocking_write_buffer_size: snd_pcm_avail_update (after recover)",
                 frames);
            return -1;
        }
    }

    len = snd_pcm_frames_to_bytes(handle, frames);

    if (len > maxbufsize)
        len = maxbufsize;

    if (len > playback_bytes_remaining)
        len = playback_bytes_remaining;

    /*     printf("audio_device_nonblocking_write_buffer_size:%d\n", len); */

    return len;
}

