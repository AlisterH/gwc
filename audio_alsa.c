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

/* Frames we estimate were queued but got dropped when ALSA stream was reset
* (e.g. XRUN recovery via snd_pcm_prepare()). This keeps "processed bytes"
* from jumping forward incorrectly after recovery. */
static snd_pcm_uframes_t dropped_frames = 0;

/* cached monotonic "processed bytes" value */
static long _audio_device_processed_bytes = 0;

/* true ALSA ring buffer size (frames). Must NOT be confused with "avail". */
static snd_pcm_uframes_t buffer_size_frames = 0;
static snd_pcm_uframes_t period_size_frames = 0;

static snd_pcm_uframes_t estimate_queued_frames_best_effort(void);

static void snd_perr(char *text, int err)
{
	fprintf(stderr, "##########################################################\n");
	fprintf(stderr, "%s\n", text);
	fprintf(stderr, "%s\n", snd_strerror(err));
	warning(text);
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
	dropped_frames = 0;
	_audio_device_processed_bytes = 0;
	buffer_size_frames = 0;
	period_size_frames = 0;

	return 0;
}

int audio_device_set_params(AUDIO_FORMAT *format, int *channels, int *rate)
{
if (handle == NULL) {
    		warning("ALSA: audio_device_set_params called with NULL handle");
    		return -1;
	}

	unsigned int utmp;
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
	case GWC_U8: 	alsa_format = SND_PCM_FORMAT_U8; break;
	case GWC_S8: 	alsa_format = SND_PCM_FORMAT_S8; break;
	case GWC_S16_BE: alsa_format = SND_PCM_FORMAT_S16_BE; break;
	default:
	case GWC_S16_LE: alsa_format = SND_PCM_FORMAT_S16_LE; break;
	}

	err = snd_pcm_hw_params_set_format(handle, params, alsa_format);
	if (err < 0) {
    	snd_perr("ALSA audio_device_set_params: snd_pcm_hw_params_set_format", err);
    	return -1;
	}

	err = snd_pcm_hw_params_get_format(params, &alsa_format);
	if (err < 0) {
    	snd_perr("ALSA audio_device_set_params: snd_pcm_hw_params_get_format", err);
    	return -1;
	}

	switch (alsa_format)
	{
	case SND_PCM_FORMAT_U8:  	*format = GWC_U8; 	break;
	case SND_PCM_FORMAT_S8:  	*format = GWC_S8; 	break;
	case SND_PCM_FORMAT_S16_BE:  *format = GWC_S16_BE; break;
	case SND_PCM_FORMAT_S16_LE:  *format = GWC_S16_LE; break;
	default:                 	*format = GWC_UNKNOWN; break;
	}

	err = snd_pcm_hw_params_set_channels(handle, params, (unsigned int)*channels);
	if (err < 0) {
    	snd_perr("ALSA audio_device_set_params: snd_pcm_hw_params_set_channels", err);
    	return -1;
	}

	utmp = (unsigned int)*channels;
	err = snd_pcm_hw_params_get_channels(params, &utmp);
	if (err < 0) {
    	snd_perr("ALSA audio_device_set_params: snd_pcm_hw_params_get_channels", err);
    	return -1;
	}
	*channels = (int)utmp;

	utmp = (unsigned int)*rate;
	err = snd_pcm_hw_params_set_rate_near(handle, params, &utmp, 0);
	if (err < 0) {
    	snd_perr("ALSA audio_device_set_params: snd_pcm_hw_params_set_rate_near", err);
    	return -1;
	}
	*rate = (int)utmp;

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

	/* New stream configuration => reset all counters */
	written_frames = 0;
	dropped_frames = 0;
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

	/* Configure SW params for more predictable start/feeding behaviour */
	err = snd_pcm_sw_params_current(handle, swparams);
	if (err < 0) {
    	snd_perr("ALSA audio_device_set_params: snd_pcm_sw_params_current", err);
    	/* not fatal */
	} else {
    	snd_pcm_uframes_t start_th;
    	snd_pcm_uframes_t avail_min;

    	/* Start once we have at least one period (or a small fallback). */
    	start_th  = (period_size_frames > 0) ? period_size_frames : 1024;
    	avail_min = start_th;

    	(void)snd_pcm_sw_params_set_start_threshold(handle, swparams, start_th);
    	(void)snd_pcm_sw_params_set_avail_min(handle, swparams, avail_min);

    	err = snd_pcm_sw_params(handle, swparams);
    	if (err < 0) {
        	snd_perr("ALSA audio_device_set_params: snd_pcm_sw_params", err);
        	/* not fatal */
    	}
	}

	fprintf(stderr, "audio_device_handle %p\n", (void*)handle);

	return 0;
}

int audio_device_read(unsigned char *buffer, int buffersize)
{
	/* not implemented */
	(void)buffer;
	(void)buffersize;
	return -1;
}

/* recover underrun and suspend */
static int recover_snd_handle(int err)
{
	if (err == -EPIPE) { /* underrun */
    	fprintf(stderr, "recover_snd_handle: err == -EPIPE\n");

    	/* Best-effort estimate of queued frames that will be dropped by prepare */
    	snd_pcm_uframes_t q = estimate_queued_frames_best_effort();

    	err = snd_pcm_prepare(handle);
    	if (err < 0) {
        	snd_perr("ALSA recover_snd_handle: can't recover underrun, prepare failed", err);
        	return err;
    	}

    	/* Only account dropped frames if prepare succeeded */
    	dropped_frames += q;
    	return 0;
	}
	else if (err == -ESTRPIPE) { /* suspend */
    	fprintf(stderr, "recover_snd_handle: err == -ESTRPIPE\n");
    	while ((err = snd_pcm_resume(handle)) == -EAGAIN)
        	sleep(1);

    	if (err < 0) {
        	/* Best-effort estimate of queued frames that may be dropped */
        	snd_pcm_uframes_t q = estimate_queued_frames_best_effort();

        	err = snd_pcm_prepare(handle);
        	if (err < 0) {
            	snd_perr("ALSA recover_snd_handle: can't recover suspend, prepare failed", err);
            	return err;
        	}

        	/* Only account dropped frames if prepare succeeded */
        	dropped_frames += q;
    	}
    	return 0;
	}

	return err;
}

int audio_device_write(unsigned char *data, int count)
{
	snd_pcm_sframes_t r;
	snd_pcm_uframes_t total_frames = 0;
	snd_pcm_uframes_t frames_left;

	if (handle == NULL || data == NULL || count <= 0)
    	return 0;

	frames_left = snd_pcm_bytes_to_frames(handle, (snd_pcm_uframes_t)count);

	while (frames_left > 0) {
    	r = snd_pcm_writei(handle, data, frames_left);

    	if (r > 0) {
        	/* wrote r frames */
        	total_frames += (snd_pcm_uframes_t)r;
        	frames_left -= (snd_pcm_uframes_t)r;
        	data += snd_pcm_frames_to_bytes(handle, (snd_pcm_uframes_t)r);
        	continue;
    	}

    	if (r == -EINTR) {
        	/* interrupted by signal, retry */
        	continue;
    	}

if (r == -EAGAIN) {
	/* nonblocking: wait until device is ready */
	int w = snd_pcm_wait(handle, 1000);

	if (w == 0) {
		/* Timed out waiting; no progress right now. */
		return (total_frames > 0)
			? (int)snd_pcm_frames_to_bytes(handle, total_frames)
			: 0;
	}

	if (w < 0) {
    	/* wait failed; try recovery then retry */
    	if (recover_snd_handle(w) < 0) {
        	snd_perr("ALSA audio_device_write: snd_pcm_wait", w);
        	return (total_frames > 0)
            	? (int)snd_pcm_frames_to_bytes(handle, total_frames)
            	: -1;
    	}
	}
	continue;
}

    	if (r == -EINVAL) {
        	/* programming error / wrong params; do NOT crash whole app */
        	fprintf(stderr,
                	"ALSA snd_pcm_writei invalid argument: handle=%p data=%p frames=%lu\n",
                	(void*)handle, (void*)data, (unsigned long)frames_left);
        	snd_perr("ALSA audio_device_write: snd_pcm_writei (-EINVAL)", (int)r);
        	return (total_frames > 0)
            	? (int)snd_pcm_frames_to_bytes(handle, total_frames)
            	: -1;
    	}

    	/* Other errors: try XRUN/suspend recovery */
    	if (recover_snd_handle((int)r) < 0) {
        	fprintf(stderr,
                	"ALSA audio_device_write failed: handle=%p data=%p frames=%lu\n",
                	(void*)handle, (void*)data, (unsigned long)frames_left);
        	snd_perr("ALSA audio_device_write: snd_pcm_writei", (int)r);

        	/* If we already wrote something, return partial progress */
        	return (total_frames > 0)
            	? (int)snd_pcm_frames_to_bytes(handle, total_frames)
            	: -1;
    	}

    	/* recovered; retry write */
	}

	written_frames += total_frames;
	return (int)snd_pcm_frames_to_bytes(handle, total_frames);
}

/* Best-effort estimate of queued frames currently pending playback.
* This is used only to adjust dropped_frames during XRUN/suspend recovery.
*
* We prefer delay (distance between app and sound position), but fall back
* to status/avail when needed. */
static snd_pcm_uframes_t estimate_queued_frames_best_effort(void)
{
	snd_pcm_sframes_t delay = 0;
	snd_pcm_status_t *status;
	int err;

	if (handle == NULL)
    	return 0;

	err = snd_pcm_delay(handle, &delay);
	if (err >= 0 && delay > 0)
    	return (snd_pcm_uframes_t)delay;

	snd_pcm_status_alloca(&status);
	err = snd_pcm_status(handle, status);
	if (err < 0)
    	return 0;

	if (buffer_size_frames == 0)
    	return 0;

	/* queued ≈ buffer_size - avail (clamped) */
	{
    	snd_pcm_sframes_t avail = (snd_pcm_sframes_t)snd_pcm_status_get_avail(status);
    	snd_pcm_sframes_t q = (snd_pcm_sframes_t)buffer_size_frames - avail;
    	if (q < 0) q = 0;
    	if ((snd_pcm_uframes_t)q > buffer_size_frames) q = (snd_pcm_sframes_t)buffer_size_frames;
    	return (snd_pcm_uframes_t)q;
	}
}

long query_processed_bytes(void)
{
    if (!handle) return 0;

    snd_pcm_status_t *status;
    snd_pcm_status_alloca(&status);

    int err = snd_pcm_status(handle, status);
    if (err < 0) {
        if (recover_snd_handle(err) < 0) return _audio_device_processed_bytes;
        err = snd_pcm_status(handle, status);
        if (err < 0) return _audio_device_processed_bytes;
    }

    /* delay = frames still queued before playback catches up */
    snd_pcm_sframes_t delay_frames = snd_pcm_status_get_delay(status);
    if (delay_frames < 0) delay_frames = 0;

    snd_pcm_sframes_t played_frames = (snd_pcm_sframes_t)written_frames - delay_frames;
    if (played_frames < 0) played_frames = 0;

    long played_bytes = snd_pcm_frames_to_bytes(handle, (snd_pcm_uframes_t)played_frames);

    /* monotonic clamp */
    if (played_bytes < _audio_device_processed_bytes)
        played_bytes = _audio_device_processed_bytes;

    return played_bytes;
}

/* Number of bytes processed since opening the device. */
long audio_device_processed_bytes(void)
{
	if (handle != NULL)
    	_audio_device_processed_bytes = query_processed_bytes();

	return _audio_device_processed_bytes;
}

void audio_device_close(int drain)
{
	if (handle != NULL) {
    	int err;

    	printf("Closing the ALSA audio device\n");

    	_audio_device_processed_bytes = query_processed_bytes();

    	if (drain) {
        	err = snd_pcm_drain(handle);
        	if (err < 0) {
            	snd_perr("ALSA audio_device_close: snd_pcm_drain", err);
        	}
    	} else {
        	err = snd_pcm_drop(handle);
        	if (err < 0) {
            	snd_perr("ALSA audio_device_close: snd_pcm_drop", err);
        	}
    	}

    	err = snd_pcm_close(handle);
    	if (err < 0) {
        	snd_perr("ALSA audio_device_close: snd_pcm_close", err);
    	}

    	handle = NULL;
	}
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

	/* Use period size as stable base quantum; scale up to requested block size */
	if (handle != NULL && ps > 0) {
    	int period_bytes = snd_pcm_frames_to_bytes(handle, ps);
    	if (period_bytes > 0)
        	frame_size = period_bytes;

    	/* If caller requested larger blocks, step up in whole periods */
    	while (frame_size < playback_bytes_per_block && period_bytes > 0) {
        	frame_size += period_bytes;
    	}
	} else {
    	frame_size = 4096;
	}

	if (frame_size < 4096 && frame_size > 0) {
    	int s = frame_size;
    	while (frame_size < 4096) frame_size += s;
    	printf("ALSA audio_device_adjusted_buffer_size:%d\n", frame_size);
	}

	if (frame_size == 0) {
    	warning("Your ALSA audio device driver gives invalid information for its buffer size, defaulting to 4K bytes, this may produce strange playback results");
    	frame_size = 4096;
	}

	return frame_size;
}

int audio_device_nonblocking_write_buffer_size(int maxbufsize,
                                           	int playback_bytes_remaining)
{
	int len = 0;
	snd_pcm_sframes_t frames = snd_pcm_avail_update(handle);

	if (frames < 0) {
    	snd_perr("audio_device_nonblocking_write_buffer_size: snd_pcm_avail_update",
             	(int)frames);

    	if (recover_snd_handle((int)frames) < 0) {
        	fprintf(stderr, "audio_device_nonblocking_write_buffer_size: could not recover handle\n");
        	return -1;
    	}

    	/* Re-query after recovery */
    	frames = snd_pcm_avail_update(handle);
    	if (frames < 0) {
        	snd_perr("audio_device_nonblocking_write_buffer_size: snd_pcm_avail_update (after recover)",
                 	(int)frames);
        	return -1;
    	}
	}

	len = snd_pcm_frames_to_bytes(handle, (snd_pcm_uframes_t)frames);

	if (len > maxbufsize)
    	len = maxbufsize;

	if (len > playback_bytes_remaining)
    	len = playback_bytes_remaining;

	return len;
}