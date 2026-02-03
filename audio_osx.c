/*****************************************************************************
*   Gnome Wave Cleaner Version 0.20.
*   Copyright (C) 2003 Jeffrey J. Welty
*
*   This program is free software; you can redistribute it and/or
*   modify it under the terms of the GNU General Public License
*   as published by the Free Software Foundation; either version 2
*   of the License, or (at your option) any later version.
*
* audio_osx.c — macOS audio backend for GTK Wave Cleaner
*
* Supports:
*   - Legacy CoreAudio HAL (AudioDeviceAddIOProc)  [USE_LEGACY_HAL=1]
*   - Modern AudioUnit output (DefaultOutput)  	[USE_LEGACY_HAL=0]
*
* Notes:
*   - Audio is always rendered as interleaved stereo float (2ch).
*   - Looping/end-of-region logic is based on play_cursor/play_end.
*   - Progress reporting uses rendered_file_frames_abs (file frames produced).
*****************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sndfile.h>
#include <time.h>

#include "gwc.h"
#include "audio_device.h"

/* If your configure system doesn't define this, default to legacy HAL. */
#ifndef USE_LEGACY_HAL
#define USE_LEGACY_HAL 1
#endif

#if USE_LEGACY_HAL
  #include <CoreAudio/AudioHardware.h>
#else
  #include <AudioUnit/AudioUnit.h>
  #include <AudioToolbox/AudioToolbox.h>
#endif

/* ----------------------
* External globals (from app)
* ---------------------- */
extern int stereo;
extern int audio_is_looping;

extern SNDFILE *sndfile;
extern SF_INFO sfinfo;
extern int audio_state;

extern long playback_end_position;
extern long playback_position;
extern long playback_start_position;

extern int FRAMESIZE;
extern int PLAYBACK_FRAMESIZE;

/* ----------------------
* Backend state
* ---------------------- */
typedef struct {
	AudioStreamBasicDescription format;
	UInt32 buf_size;
#if USE_LEGACY_HAL
	AudioDeviceID device;
#endif
	SNDFILE *sndfile;
	SF_INFO sfinfo;
	int done_playing;
	bool done_reading;
} MacOSXAudioData;

static MacOSXAudioData audio_data;

/* Tunables */
int BUFFERSIZE = 1024;

/* Mono->stereo temp buffer */
static float  *mono_tmp = NULL;
static size_t  mono_tmp_frames_cap = 0;

/* Meters + counters */
static float	meterL = 0.0f;
static float	meterR = 0.0f;
static uint64_t rendered_file_frames_abs = 0; /* file frames actually produced */
static sf_count_t play_cursor = 0;        	/* file frame cursor */
static sf_count_t play_end	= 0;        	/* exclusive end in file frames */

Float64 start_sample_time;
bool playback_just_started = false;

#if USE_LEGACY_HAL
static bool coreaudio_device_started   = false;
static bool coreaudio_ioproc_installed = false;
#else
static AudioUnit outputUnit = NULL;
static bool audiounit_inited  = false;
static bool audiounit_started = false;
#endif

/* ----------------------
* Shared render helper
* Fills out_frames (interleaved stereo floats).
* Updates meters + play_cursor + rendered_file_frames_abs.
* ---------------------- */
static void
render_interleaved_stereo_float(MacOSXAudioData *ad, float *out, UInt32 out_frames)
{
	const UInt32 ch = 2;

	if (!ad || !ad->sndfile || !out || out_frames == 0) {
    	if (out && out_frames) memset(out, 0, (size_t)out_frames * ch * sizeof(float));
    	meterL = meterR = 0.0f;
    	return;
	}

	/* If we've already hit EOF/end and not looping, just output silence */
	if (ad->done_reading && !audio_is_looping) {
    	memset(out, 0, (size_t)out_frames * ch * sizeof(float));
    	meterL = meterR = 0.0f;
    	return;
	}

	int file_ch = ad->sfinfo.channels;
	if (file_ch < 1) file_ch = 1;
	if (file_ch > 2) file_ch = 2;

	float maxl = 0.0f, maxr = 0.0f;

	sf_count_t frames_left = (sf_count_t)out_frames;
	sf_count_t out_off 	= 0;
	sf_count_t frames_written_total = 0;

	while (frames_left > 0) {

    	/* End-of-region handling */
    	if (play_end > 0 && play_cursor >= play_end) {
        	if (audio_is_looping) {
            	sf_seek(ad->sndfile, playback_start_position, SEEK_SET);
            	play_cursor = playback_start_position;
            	ad->done_reading = false;
        	} else {
            	memset(out + (size_t)out_off * ch, 0,
                   	(size_t)frames_left * ch * sizeof(float));
            	ad->done_reading = true;
            	break;
        	}
    	}

    	sf_count_t frames_to_read = frames_left;
    	if (play_end > 0 && (play_cursor + frames_to_read) > play_end)
        	frames_to_read = play_end - play_cursor;

    	sf_count_t frames_read = 0;

    	if (file_ch == 2) {
        	frames_read = sf_readf_float(ad->sndfile,
                                     	out + (size_t)out_off * ch,
                                     	frames_to_read);
    	} else {
        	/* Read mono into temp and duplicate */
        	if (mono_tmp_frames_cap < (size_t)frames_to_read) {
            	float *nb = (float *)realloc(mono_tmp, (size_t)frames_to_read * sizeof(float));
            	if (!nb) {
                	memset(out + (size_t)out_off * ch, 0,
                       	(size_t)frames_to_read * ch * sizeof(float));
                	frames_read = 0;
            	} else {
                	mono_tmp = nb;
                	mono_tmp_frames_cap = (size_t)frames_to_read;
                	frames_read = sf_readf_float(ad->sndfile, mono_tmp, frames_to_read);
            	}
        	} else {
            	frames_read = sf_readf_float(ad->sndfile, mono_tmp, frames_to_read);
        	}

        	for (sf_count_t f = 0; f < frames_read; f++) {
            	float v = mono_tmp[f];
            	out[((size_t)out_off + (size_t)f) * 2 + 0] = v;
            	out[((size_t)out_off + (size_t)f) * 2 + 1] = v;
        	}
    	}

    	/* Zero-pad if short read */
    	if (frames_read < frames_to_read) {
        	sf_count_t remain = frames_to_read - frames_read;
        	memset(out + ((size_t)out_off + (size_t)frames_read) * ch, 0,
               	(size_t)remain * ch * sizeof(float));
        	ad->done_reading = true;
    	}

    	/* Meter */
    	for (sf_count_t f = 0; f < frames_read; f++) {
        	float vl = fabsf(out[((size_t)out_off + (size_t)f) * 2 + 0]);
        	float vr = fabsf(out[((size_t)out_off + (size_t)f) * 2 + 1]);
        	if (vl > maxl) maxl = vl;
        	if (vr > maxr) maxr = vr;
    	}

    	play_cursor += frames_read;
    	frames_written_total += frames_read;

    	/* We always advance output by frames_to_read (since we filled or zeroed) */
    	out_off += frames_to_read;
    	frames_left -= frames_to_read;

    	if (ad->done_reading && !audio_is_looping)
        	break;
	}

	meterL = maxl;
	meterR = maxr;
	rendered_file_frames_abs += (uint64_t)frames_written_total;
}

/* ----------------------
* Legacy HAL callback
* ---------------------- */
#if USE_LEGACY_HAL
static OSStatus
macosx_audio_out_callback(AudioDeviceID device,
                      	const AudioTimeStamp *current_time,
                      	const AudioBufferList *data_in,
                      	const AudioTimeStamp *time_in,
                      	AudioBufferList *data_out,
                      	const AudioTimeStamp *time_out,
                      	void *client_data)
{
	(void)device; (void)current_time; (void)data_in; (void)time_in;

	MacOSXAudioData *ad = (MacOSXAudioData *)client_data;
	if (!data_out || data_out->mNumberBuffers < 1) return noErr;

	AudioBuffer *b0 = &data_out->mBuffers[0];
	UInt32 size_bytes = b0->mDataByteSize;
	float *out = (float *)b0->mData;

	if (playback_just_started) {
    	playback_just_started = false;
    	start_sample_time = time_out ? time_out->mSampleTime : 0.0;
	}

	if (!out || size_bytes == 0) return noErr;

	const UInt32 ch = 2;
	UInt32 out_frames = (UInt32)(size_bytes / (sizeof(float) * ch));

	render_interleaved_stereo_float(ad, out, out_frames);
	return noErr;
}
#endif /* USE_LEGACY_HAL */

/* ----------------------
* AudioUnit render callback (DefaultOutput)
* ---------------------- */
#if !USE_LEGACY_HAL
static OSStatus
audiounit_render_callback(void *inRefCon,
                      	AudioUnitRenderActionFlags *ioActionFlags,
                      	const AudioTimeStamp *inTimeStamp,
                      	UInt32 inBusNumber,
                      	UInt32 inNumberFrames,
                      	AudioBufferList *ioData)
{
	(void)ioActionFlags; (void)inBusNumber;

	MacOSXAudioData *ad = (MacOSXAudioData *)inRefCon;
	if (!ioData || ioData->mNumberBuffers < 1) return noErr;

	/* Configured as interleaved stereo float => one buffer */
	AudioBuffer *b0 = &ioData->mBuffers[0];
	float *out = (float *)b0->mData;

	if (playback_just_started) {
    	playback_just_started = false;
    	start_sample_time = inTimeStamp ? inTimeStamp->mSampleTime : 0.0;
	}

	if (!out || inNumberFrames == 0) {
    	return noErr;
	}

	render_interleaved_stereo_float(ad, out, inNumberFrames);

	/* Ensure reported byte size matches frames requested */
	b0->mDataByteSize = (UInt32)(inNumberFrames * 2 * sizeof(float));
	return noErr;
}

static void
audiounit_teardown(void)
{
	if (outputUnit) {
    	if (audiounit_started) {
        	AudioOutputUnitStop(outputUnit);
        	audiounit_started = false;
    	}
    	if (audiounit_inited) {
        	AudioUnitUninitialize(outputUnit);
        	audiounit_inited = false;
    	}
    	AudioComponentInstanceDispose(outputUnit);
    	outputUnit = NULL;
	}
}
#endif /* !USE_LEGACY_HAL */

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
#if USE_LEGACY_HAL
	OSStatus err;
	UInt32 count = sizeof(AudioDeviceID);

	audio_data.device = kAudioDeviceUnknown;
	if ((err = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice,
                                    	&count, &audio_data.device)) != noErr) {
    	printf("AudioHardwareGetProperty failed: %d\n", (int)err);
    	return -1;
	}
#else
	(void)output_device;
#endif
	return 0;
}

int audio_device_set_params(AUDIO_FORMAT *format, int *channels, int *rate)
{
	(void)format;

	OSStatus err = noErr;

	audio_data.sfinfo = sfinfo;
	audio_data.sndfile = sndfile;

	/* Shared init/reset */
	meterL = meterR = 0.0f;
	rendered_file_frames_abs = 0;

	play_cursor = playback_start_position;
	play_end	= playback_end_position;

	start_sample_time = 0.0;
	playback_just_started = true;

	audio_data.done_playing = 0;
	audio_data.done_reading = false;

#if USE_LEGACY_HAL
	UInt32 count;
	/* Tear down any previous run */
	if (coreaudio_device_started) {
    	AudioDeviceStop(audio_data.device, macosx_audio_out_callback);
    	coreaudio_device_started = false;
	}
	if (coreaudio_ioproc_installed) {
    	AudioDeviceRemoveIOProc(audio_data.device, macosx_audio_out_callback);
    	coreaudio_ioproc_installed = false;
	}

	/* Get current stream format */
	count = sizeof(AudioStreamBasicDescription);
	err = AudioDeviceGetProperty(audio_data.device, 0, false,
                             	kAudioDevicePropertyStreamFormat,
                             	&count, &audio_data.format);
	if (err != noErr) {
    	printf("AudioDeviceGetProperty(StreamFormat) failed: %d\n", (int)err);
    	goto fail;
	}

	/* Force device to file rate + stereo (HAL requires matching device) */
	audio_data.format.mSampleRate = (Float64)audio_data.sfinfo.samplerate;
	audio_data.format.mChannelsPerFrame = 2;

	err = AudioDeviceSetProperty(audio_data.device, NULL, 0, false,
                             	kAudioDevicePropertyStreamFormat,
                             	sizeof(AudioStreamBasicDescription),
                             	&audio_data.format);
	if (err != noErr) {
    	printf("AudioDeviceSetProperty(StreamFormat) failed: %d\n", (int)err);
    	goto fail;
	}

	/* Best-effort nominal sample rate */
	{
    	Float64 sr = (Float64)audio_data.sfinfo.samplerate;
    	count = sizeof(Float64);
    	err = AudioDeviceSetProperty(audio_data.device, NULL, 0, false,
                                 	kAudioDevicePropertyNominalSampleRate,
                                 	count, &sr);
    	if (err != noErr) {
        	/* Not fatal */
        	err = noErr;
    	}
	}

	/* Install and start IOProc */
	err = AudioDeviceAddIOProc(audio_data.device, macosx_audio_out_callback, &audio_data);
	if (err != noErr) {
    	printf("AudioDeviceAddIOProc failed: %d\n", (int)err);
    	goto fail;
	}
	coreaudio_ioproc_installed = true;

	err = AudioDeviceStart(audio_data.device, macosx_audio_out_callback);
	if (err != noErr) {
    	printf("AudioDeviceStart failed: %d\n", (int)err);
    	AudioDeviceRemoveIOProc(audio_data.device, macosx_audio_out_callback);
    	coreaudio_ioproc_installed = false;
    	goto fail;
	}
	coreaudio_device_started = true;

	/* Report actual device format */
	if (rate) 	*rate 	= (int)audio_data.format.mSampleRate;
	if (channels) *channels = (int)audio_data.format.mChannelsPerFrame;

#else
	/* ---- AudioUnit DefaultOutput path ---- */
	audiounit_teardown();

	AudioComponentDescription desc;
	memset(&desc, 0, sizeof(desc));
	desc.componentType = kAudioUnitType_Output;
	desc.componentSubType = kAudioUnitSubType_DefaultOutput;
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;

	AudioComponent comp = AudioComponentFindNext(NULL, &desc);
	if (!comp) {
    	printf("AudioComponentFindNext(DefaultOutput) failed\n");
    	goto fail;
	}

	err = AudioComponentInstanceNew(comp, &outputUnit);
	if (err != noErr || !outputUnit) {
    	printf("AudioComponentInstanceNew failed: %d\n", (int)err);
    	goto fail;
	}

	/* Set render callback */
	AURenderCallbackStruct cb;
	cb.inputProc = audiounit_render_callback;
	cb.inputProcRefCon = &audio_data;

	err = AudioUnitSetProperty(outputUnit,
                           	kAudioUnitProperty_SetRenderCallback,
                           	kAudioUnitScope_Input,
                           	0,
                           	&cb,
                           	sizeof(cb));
	if (err != noErr) {
    	printf("AudioUnitSetProperty(SetRenderCallback) failed: %d\n", (int)err);
    	goto fail;
	}

	/* We supply float interleaved stereo at FILE rate.
   	The output unit will convert to the hardware rate if needed. */
	AudioStreamBasicDescription asbd;
	memset(&asbd, 0, sizeof(asbd));
	asbd.mSampleRate   	= (Float64)audio_data.sfinfo.samplerate;
	asbd.mFormatID     	= kAudioFormatLinearPCM;
	asbd.mFormatFlags  	= kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
	asbd.mFramesPerPacket  = 1;
	asbd.mChannelsPerFrame = 2;
	asbd.mBitsPerChannel   = 32;
	asbd.mBytesPerFrame	= (UInt32)(2 * sizeof(float));
	asbd.mBytesPerPacket   = asbd.mBytesPerFrame;

	err = AudioUnitSetProperty(outputUnit,
                           	kAudioUnitProperty_StreamFormat,
                           	kAudioUnitScope_Input,
                           	0,
                           	&asbd,
                           	sizeof(asbd));
	if (err != noErr) {
    	printf("AudioUnitSetProperty(StreamFormat) failed: %d\n", (int)err);
    	goto fail;
	}

	/* Optional hint: maximum slice size */
	{
    	UInt32 maxFrames = (UInt32)BUFFERSIZE;
    	(void)AudioUnitSetProperty(outputUnit,
                               	kAudioUnitProperty_MaximumFramesPerSlice,
                               	kAudioUnitScope_Global,
                               	0,
                               	&maxFrames,
                               	sizeof(maxFrames));
	}

	err = AudioUnitInitialize(outputUnit);
	if (err != noErr) {
    	printf("AudioUnitInitialize failed: %d\n", (int)err);
    	goto fail;
	}
	audiounit_inited = true;

	err = AudioOutputUnitStart(outputUnit);
	if (err != noErr) {
    	printf("AudioOutputUnitStart failed: %d\n", (int)err);
    	goto fail;
	}
	audiounit_started = true;

	/* Report logical (file) format */
	if (rate) 	*rate 	= (int)audio_data.sfinfo.samplerate;
	if (channels) *channels = 2;
#endif

	return 0;

fail:
	/* Cleanup and mark playback done */
#if USE_LEGACY_HAL
	if (coreaudio_device_started) {
    	AudioDeviceStop(audio_data.device, macosx_audio_out_callback);
    	coreaudio_device_started = false;
	}
	if (coreaudio_ioproc_installed) {
    	AudioDeviceRemoveIOProc(audio_data.device, macosx_audio_out_callback);
    	coreaudio_ioproc_installed = false;
	}
#else
	audiounit_teardown();
#endif

	audio_data.sndfile = NULL;
	audio_data.done_playing = 1;
	audio_data.done_reading = true;
	meterL = meterR = 0.0f;
	return -1;
}

int audio_device_read(unsigned char *buffer, int buffersize)  { (void)buffer; (void)buffersize; return 0; }
int audio_device_write(unsigned char *buffer, int buffersize) { (void)buffer; (void)buffersize; return 0; }

/* This drives your UI position/progress logic. */
long audio_device_processed_bytes(void)
{
	long region_len = (playback_end_position - playback_start_position);
	if (region_len < 1) region_len = 1;

	if (audio_is_looping) {
    	playback_position = playback_start_position +
        	(long)(rendered_file_frames_abs % (uint64_t)region_len);
	} else {
    	long advanced = (long)rendered_file_frames_abs;
    	long pos = playback_start_position + advanced;
    	if (pos > playback_end_position) pos = playback_end_position;
    	playback_position = pos;
    	if (advanced >= region_len) audio_data.done_playing = 1;
	}

	return (long)(rendered_file_frames_abs * (uint64_t)PLAYBACK_FRAMESIZE);
}

int audio_device_best_buffer_size(int playback_bytes_per_block)
{
	(void)playback_bytes_per_block;
#if USE_LEGACY_HAL
	OSStatus err;
	UInt32 count = sizeof(UInt32);
	UInt32 buffer_size = 0;

	err = AudioDeviceGetProperty(audio_data.device, 0, false,
                             	kAudioDevicePropertyBufferSize,
                             	&count, &buffer_size);
	if (err != noErr) {
    	printf("AudioDeviceGetProperty(BufferSize) failed.\n");
    	return -1;
	}
	return (int)buffer_size;
#else
	/* Return something reasonable in bytes for AU path */
	return (int)(BUFFERSIZE * PLAYBACK_FRAMESIZE);
#endif
}

int audio_device_nonblocking_write_buffer_size(int maxbufsize, int playback_bytes_remaining)
{
	(void)maxbufsize; (void)playback_bytes_remaining;
	return 1;
}

void audio_device_close(int drain)
{
	(void)drain;
#if USE_LEGACY_HAL
	if (coreaudio_device_started) {
    	AudioDeviceStop(audio_data.device, macosx_audio_out_callback);
    	coreaudio_device_started = false;
	}
	if (coreaudio_ioproc_installed) {
    	AudioDeviceRemoveIOProc(audio_data.device, macosx_audio_out_callback);
    	coreaudio_ioproc_installed = false;
	}
#else
	audiounit_teardown();
#endif
}