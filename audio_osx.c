/*****************************************************************************
*   Gnome Wave Cleaner Version 0.20.
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
*/
/*
 *  audio_osx.c
 *
 *  Original version created by Rob Frohne on 11/8/04.
 *  Copyright 2004  *
 */
#ifdef MAC_OS_X /* MacOSX */

#include <sndfile.h>

#include <Carbon/Carbon.h>
#include <CoreAudio/AudioHardware.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <math.h>

#include "gwc.h"
#include "audio_device.h"

// Suppress deprecation warnings for CoreAudio APIs
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

extern int stereo;
extern int audio_is_looping;

typedef struct
{
    AudioStreamBasicDescription format ;
    UInt32 buf_size ;
    AudioDeviceID device ;
    SNDFILE *sndfile ;
    SF_INFO sfinfo ;
    int done_playing ;
    bool done_reading ;
} MacOSXAudioData;

MacOSXAudioData audio_data;

extern SNDFILE *sndfile;
extern SF_INFO sfinfo;
extern int audio_state;
extern long playback_end_position ;
extern long playback_position ;
extern long playback_start_position;
extern long playback_samples_remaining;
extern long playback_total_bytes ;
extern int FRAMESIZE;
extern int PLAYBACK_FRAMESIZE;
int BUFFERSIZE = 1024;

static float *mono_tmp = NULL;
static size_t mono_tmp_frames_cap = 0;

static long buff_num = 0;
static long buff_num_play = 0;
static long num_buffers = 0;

/* Minimal meter and frame counter */
static float meterL = 0.0f;
static float meterR = 0.0f;
static sf_count_t play_cursor = 0;
static sf_count_t play_end = 0; /* exclusive */
static uint64_t rendered_frames_abs = 0;

Float64 start_sample_time;
struct timeval playback_start_time;
bool playback_just_started = FALSE;
static bool coreaudio_device_started = FALSE;
static bool coreaudio_ioproc_installed = FALSE;

/* ----------------------
 * CoreAudio callback
 * ---------------------- */
static OSStatus
macosx_audio_out_callback (AudioDeviceID device, const AudioTimeStamp* current_time,
                           const AudioBufferList* data_in, const AudioTimeStamp* time_in,
                           AudioBufferList* data_out, const AudioTimeStamp* time_out,
                           void* client_data)
{
    UInt32 size_bytes;
    void *out_ptr;
    float *p_float;
    UInt32 out_samples;
    UInt32 out_frames;
    int ch, file_ch;
    sf_count_t frames_to_read, frames_read;
    float maxl = 0.0f, maxr = 0.0f;

    if (playback_just_started)
    {
        playback_just_started = FALSE;
        start_sample_time = time_out->mSampleTime;
    }

    MacOSXAudioData *audio_data = (MacOSXAudioData*) client_data;
    if (!audio_data || !audio_data->sndfile) {
        size_bytes = data_out->mBuffers[0].mDataByteSize;
        memset(data_out->mBuffers[0].mData, 0, size_bytes);
        return noErr;
    }

    size_bytes = data_out->mBuffers[0].mDataByteSize;
    out_ptr = data_out->mBuffers[0].mData;
    if (!out_ptr || size_bytes == 0) return noErr;

    ch = 2;
    file_ch = audio_data->sfinfo.channels;
    if (file_ch < 1) file_ch = 1;
    if (file_ch > 2) file_ch = 2;

    out_samples = size_bytes / sizeof(float);
    out_frames = out_samples / ch;
    p_float = (float*)out_ptr;

    if (out_frames == 0) return noErr;

    sf_count_t frames_needed = out_frames;
    sf_count_t frames_written_total = 0;
    sf_count_t out_off = 0;

    if (audio_data->done_reading && !audio_is_looping) {
        memset(p_float, 0, size_bytes);
        meterL = 0.0f;
        meterR = 0.0f;
        return noErr;
    }

    while (frames_needed > 0) {
        if (play_end > 0 && play_cursor >= play_end) {
            if (audio_is_looping) {
                sf_seek(audio_data->sndfile, playback_start_position, SEEK_SET);
                play_cursor = playback_start_position;
                audio_data->done_reading = FALSE;
            } else {
                memset(p_float + out_off * ch, 0, frames_needed * ch * sizeof(float));
                audio_data->done_reading = TRUE;
                break;
            }
        }

        frames_to_read = frames_needed;
        if (play_end > 0 && play_cursor + frames_to_read > play_end)
            frames_to_read = play_end - play_cursor;

        if (file_ch == 2) {
            frames_read = sf_readf_float(audio_data->sndfile, p_float + out_off * ch, frames_to_read);
        } else {
            if (mono_tmp_frames_cap < (size_t)frames_to_read) {
                float *nb = (float*)realloc(mono_tmp, frames_to_read * sizeof(float));
                if (!nb) {
                    memset(p_float + out_off * ch, 0, frames_to_read * ch * sizeof(float));
                    frames_read = 0;
                } else {
                    mono_tmp = nb;
                    mono_tmp_frames_cap = frames_to_read;
                    frames_read = sf_readf_float(audio_data->sndfile, mono_tmp, frames_to_read);
                }
            } else {
                frames_read = sf_readf_float(audio_data->sndfile, mono_tmp, frames_to_read);
            }

            for (sf_count_t f = 0; f < frames_read; f++) {
                float v = mono_tmp[f];
                p_float[(out_off + f) * 2 + 0] = v;
                p_float[(out_off + f) * 2 + 1] = v;
            }
        }

        if (frames_read < frames_to_read) {
            sf_count_t remain = frames_to_read - frames_read;
            memset(p_float + (out_off + frames_read) * ch, 0, remain * ch * sizeof(float));
            audio_data->done_reading = TRUE;
        }

        for (sf_count_t f = 0; f < frames_read; f++) {
            float vl = fabsf(p_float[(out_off + f) * 2 + 0]);
            float vr = fabsf(p_float[(out_off + f) * 2 + 1]);
            if (vl > maxl) maxl = vl;
            if (vr > maxr) maxr = vr;
        }

        play_cursor += frames_read;
        frames_written_total += frames_read;
        out_off += frames_to_read;
        frames_needed -= frames_to_read;

        if (audio_data->done_reading && !audio_is_looping) break;
    }

    meterL = maxl;
    meterR = maxr;
    rendered_frames_abs += frames_written_total;

    return noErr;
}

/* ----------------------
 * Minimal process_audio
 * ---------------------- */
int process_audio(gfloat *pL, gfloat *pR)
{
    if (pL) *pL = meterL;
    if (pR) *pR = meterR;

    return (audio_state == AUDIO_IS_PLAYBACK) ? 0 : 1;
}

/* ----------------------
 * Audio device management
 * ---------------------- */
int audio_device_open(char *output_device)
{
    OSStatus err;
    UInt32 count;

    audio_data.device = kAudioDeviceUnknown;
    count = sizeof(AudioDeviceID);
    if ((err = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice,
                                        &count, &audio_data.device)) != noErr) {
        printf("AudioHardwareGetProperty failed: %d\n", (int)err);
        return -1;
    }

    return 0;
}

int audio_device_set_params(AUDIO_FORMAT *format, int *channels, int *rate)
{
    OSStatus err;
    UInt32 count;

    audio_data.sfinfo = sfinfo;
    audio_data.sndfile = sndfile;

    count = sizeof(AudioStreamBasicDescription);
    if ((err = AudioDeviceGetProperty(audio_data.device, 0, false, kAudioDevicePropertyStreamFormat,
                                      &count, &audio_data.format)) != noErr) {
        printf("AudioDeviceGetProperty failed: %d\n", (int)err);
        return -1;
    }

    audio_data.format.mSampleRate = audio_data.sfinfo.samplerate;
    audio_data.format.mChannelsPerFrame = 2; /* force stereo */

	err = AudioDeviceSetProperty(audio_data.device,
	                             NULL,
	                             0,
	                             false,
	                             kAudioDevicePropertyStreamFormat,
	                             sizeof(AudioStreamBasicDescription),
	                             &audio_data.format);

	if (err != noErr) {
	    printf("AudioDeviceSetProperty(StreamFormat) failed: %d\n", (int)err);
	    return -1;
	}

    if (rate) *rate = (int)audio_data.format.mSampleRate;
    if (channels) *channels = (int)audio_data.format.mChannelsPerFrame;

    buff_num = 0;
    buff_num_play = 0;
    num_buffers = (playback_end_position - playback_start_position)/BUFFERSIZE;

    meterL = 0.0f;
    meterR = 0.0f;
    rendered_frames_abs = 0;
    play_cursor = playback_start_position;
    play_end = playback_end_position;
    start_sample_time = 0.0;
    playback_just_started = TRUE;

    if (coreaudio_device_started) {
        AudioDeviceStop(audio_data.device, macosx_audio_out_callback);
        coreaudio_device_started = FALSE;
    }
    if (coreaudio_ioproc_installed) {
        AudioDeviceRemoveIOProc(audio_data.device, macosx_audio_out_callback);
        coreaudio_ioproc_installed = FALSE;
    }

	Float64 sr = (Float64)audio_data.sfinfo.samplerate;
	count = sizeof(Float64);

	err = AudioDeviceSetProperty(audio_data.device,
	                             NULL,
	                             0,
	                             false,
	                             kAudioDevicePropertyNominalSampleRate,
	                             count,
	                             &sr);

	if (err != noErr) {
	    printf("Failed to set device sample rate to %.0f Hz (err=%d)\n", sr, (int)err);
	    /* Not fatal — device may refuse, but at least we tried */
	}

    if ((err = AudioDeviceAddIOProc(audio_data.device, macosx_audio_out_callback, &audio_data)) != noErr) {
        printf("AudioDeviceAddIOProc failed: %d\n", (int)err);
        return -1;
    }
    coreaudio_ioproc_installed = TRUE;

    if (!coreaudio_device_started) {
        if ((err = AudioDeviceStart(audio_data.device, macosx_audio_out_callback)) != noErr) {
            printf("AudioDeviceStart failed: %d\n", (int)err);
            AudioDeviceRemoveIOProc(audio_data.device, macosx_audio_out_callback);
            coreaudio_ioproc_installed = FALSE;
            return -1;
        }
        coreaudio_device_started = TRUE;
    }

    audio_data.done_playing = 0;
    audio_data.done_reading = FALSE;
    return 0;
}

int audio_device_read(unsigned char *buffer, int buffersize){ return 0; }
int audio_device_write(unsigned char *buffer, int buffersize){ return 0; }

long audio_device_processed_bytes(void)
{
    extern int audio_playback;
    long region_len = (playback_end_position - playback_start_position);
    if (region_len < 1) region_len = 1;

    if (audio_is_looping) {
        playback_position = playback_start_position + (long)(rendered_frames_abs % region_len);
    } else {
        long advanced = (long)rendered_frames_abs;
        long pos = playback_start_position + advanced;
        if (pos > playback_end_position) pos = playback_end_position;
        playback_position = pos;
        if (advanced >= region_len) audio_data.done_playing = 1;
    }

    return (long)(rendered_frames_abs * (uint64_t)PLAYBACK_FRAMESIZE);
}

int audio_device_best_buffer_size(int playback_bytes_per_block)
{
    OSStatus err;
    UInt32 count, buffer_size;

    count = sizeof(UInt32);
    if ((err = AudioDeviceGetProperty(audio_data.device, 0, false, kAudioDevicePropertyBufferSize,
                                      &count, &buffer_size)) != noErr) {
        printf("AudioDeviceGetProperty failed.\n");
        return -1;
    }
    return (int)buffer_size;
}

int audio_device_nonblocking_write_buffer_size(int maxbufsize, int playback_bytes_remaining)
{
    return 1;
}

void audio_device_close(int drain)
{
    OSStatus err;

    if (coreaudio_device_started) {
        err = AudioDeviceStop(audio_data.device, macosx_audio_out_callback);
        if (err != noErr)
            printf("AudioDeviceStop failed: %d\n", (int)err);
        coreaudio_device_started = FALSE;
    }

    if (coreaudio_ioproc_installed) {
        err = AudioDeviceRemoveIOProc(audio_data.device, macosx_audio_out_callback);
        if (err != noErr)
            printf("AudioDeviceRemoveIOProc failed: %d\n", (int)err);
        coreaudio_ioproc_installed = FALSE;
    }
}

#pragma clang diagnostic pop
#endif /* MAC_OS_X */