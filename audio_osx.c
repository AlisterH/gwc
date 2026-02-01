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
 *  gwc_mac
 *
 *  Created by Rob Frohne on 11/8/04.
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
#include <string.h>
#include <stdint.h>
#include <stdatomic.h>

#include "gwc.h"
#include "audio_device.h"

// Suppress deprecation warnings for CoreAudio APIs
// These APIs still work and updating to modern APIs would require significant refactoring
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

extern int wavefile_fd ;
extern int stereo;
extern int audio_is_looping;
extern int looped_count = 0;


typedef struct
{	AudioStreamBasicDescription		format ;
	UInt32 			buf_size ;
	AudioDeviceID 		device ;
	SNDFILE 		*sndfile ;
	SF_INFO 		sfinfo ;
	int 			done_playing ;
	bool			done_reading ;
} MacOSXAudioData ;

MacOSXAudioData 		audio_data ;

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
int BUFFERSIZE = 1024; // The size of the buffers we will send.
static long buff_num = 0;
static long buff_num_play = 0;
static long num_buffers = 0;

/* -------------------------
 * Meter ring buffer (small, lock-free SPSC)
 * Callback thread produces peaks, UI thread consumes them.
 * ------------------------- */
#define METER_RING_SIZE 512
/* Power-of-two required for mask indexing */
#define METER_RING_MASK (METER_RING_SIZE - 1)
#if (METER_RING_SIZE & (METER_RING_SIZE - 1)) != 0
#error "METER_RING_SIZE must be a power of two"
#endif

static gfloat meterL[METER_RING_SIZE];
static gfloat meterR[METER_RING_SIZE];
static atomic_uint_fast32_t meter_widx = 0;
static atomic_uint_fast32_t meter_ridx = 0;
static _Atomic float last_meterL = 0.0f;
static _Atomic float last_meterR = 0.0f;
/* Playback region cursor in libsndfile frames (same units as sf_seek) */
static sf_count_t play_cursor = 0;
static sf_count_t play_end	= 0; /* exclusive */

/* Total frames rendered since start of playback, including loops (UI timebase) */
static atomic_uint_fast64_t rendered_frames_abs = 0;

static inline void
meter_ring_reset(void)
{
	atomic_store_explicit(&meter_widx, 0, memory_order_relaxed);
	atomic_store_explicit(&meter_ridx, 0, memory_order_relaxed);
	atomic_store_explicit(&last_meterL, 0.0f, memory_order_relaxed);
	atomic_store_explicit(&last_meterR, 0.0f, memory_order_relaxed);
	atomic_store_explicit(&rendered_frames_abs, 0, memory_order_relaxed);
}

/* Producer: CoreAudio callback thread */
static inline void
meter_ring_push(float l, float r)
{
	/* Always update "last known" values */
	atomic_store_explicit(&last_meterL, l, memory_order_relaxed);
	atomic_store_explicit(&last_meterR, r, memory_order_relaxed);

	/* Ring indices */
	uint32_t w	= (uint32_t)atomic_load_explicit(&meter_widx, memory_order_relaxed);
	uint32_t ridx = (uint32_t)atomic_load_explicit(&meter_ridx, memory_order_acquire);

	/* If full (distance == size), drop oldest by advancing read index */
	if ((w - ridx) >= METER_RING_SIZE) {
    	atomic_store_explicit(&meter_ridx, ridx + 1, memory_order_release);
	}

	meterL[w & METER_RING_MASK] = (gfloat) l;
	meterR[w & METER_RING_MASK] = (gfloat) r;
	atomic_store_explicit(&meter_widx, w + 1, memory_order_release);
}

/* Consumer: UI thread (process_audio) */
static inline int
meter_ring_pop(float *l, float *r)
{

	uint32_t ridx = (uint32_t)atomic_load_explicit(&meter_ridx, memory_order_relaxed);
	uint32_t w	= (uint32_t)atomic_load_explicit(&meter_widx, memory_order_acquire);

	if (ridx == w) {
    	return 0; /* empty */
	}

	*l = (float) meterL[ridx & METER_RING_MASK];
	*r = (float) meterR[ridx & METER_RING_MASK];
	atomic_store_explicit(&meter_ridx, ridx + 1, memory_order_release);
	return 1;
}

Float64 start_sample_time;
struct timeval playback_start_time;
bool playback_just_started = FALSE;
static bool coreaudio_device_started = FALSE;  // Track if device has been started
static bool coreaudio_ioproc_installed = FALSE; // Track IOProc install state


static OSStatus
macosx_audio_out_callback (AudioDeviceID device, const AudioTimeStamp* current_time,
						   const AudioBufferList* data_in, const AudioTimeStamp* time_in,
						   AudioBufferList*	data_out, const AudioTimeStamp* time_out,
						   void* client_data)
{	
	UInt32              	size_bytes;
	void					*out_ptr;
	float            		*p_float;
	UInt32              	out_samples;
	UInt32              	out_frames;
	int                 	ch;
	sf_count_t           	frames_to_read;
	sf_count_t           	frames_read;
	float                	maxl = 0.0f, maxr = 0.0f;
	
	if (playback_just_started)
	{
		playback_just_started = FALSE;
		start_sample_time = time_out->mSampleTime;
	}
	
	MacOSXAudioData *audio_data = (MacOSXAudioData*) client_data ;
	if (!audio_data || !audio_data->sndfile) {
    	/* No file – output silence */
    	size_bytes = data_out->mBuffers[0].mDataByteSize;
    	memset(data_out->mBuffers[0].mData, 0, size_bytes);
    	return noErr;
	}

 	/* Output buffer */
	size_bytes = data_out->mBuffers[0].mDataByteSize;
	out_ptr = data_out->mBuffers[0].mData;

	if (!out_ptr || size_bytes == 0) {
    	return noErr;
	}

	/* Determine channel count (expected 1 or 2) */
	ch = audio_data->sfinfo.channels;
	if (ch < 1) ch = 1;
	if (ch > 2) ch = 2;

	out_samples = size_bytes / sizeof(float);
	out_frames  = (ch > 0) ? (out_samples / (UInt32)ch) : 0;
	p_float 	= (float*)out_ptr;

	if (out_frames == 0) {
    	return noErr;
	}
	/*
 	* Fill output buffer respecting region [play_cursor, play_end).
 	* If looping is enabled, wrap and continue filling within the same callback.
 	*/
	sf_count_t frames_needed = (sf_count_t)out_frames;
	sf_count_t frames_written_total = 0;
	sf_count_t out_off = 0; /* frame offset into p_float */

	/* If already done and not looping, output silence */
	if (audio_data->done_reading && !audio_is_looping) {
    	memset(p_float, 0, size_bytes);
    	meter_ring_push(0.0f, 0.0f);
    	return noErr;
	}

	while (frames_needed > 0) {
    	/* End reached? */
    	if (play_end > 0 && play_cursor >= play_end) {
        	if (audio_is_looping) {
            	/* Wrap to start of selection */
            	sf_seek(audio_data->sndfile, playback_start_position, SEEK_SET);
            	play_cursor = (sf_count_t)playback_start_position;
            	looped_count++;
            	audio_data->done_reading = FALSE;
        	} else {
            	/* Not looping: pad remainder with zeros */
            	memset(p_float + out_off * ch, 0, (size_t)(frames_needed * ch) * sizeof(float));
            	audio_data->done_reading = TRUE;
            	break;
        	}
    	}

    	/* Determine how many frames we can read before region end */
    	frames_to_read = frames_needed;
    	if (play_end > 0 && play_cursor + frames_to_read > play_end) {
        	frames_to_read = play_end - play_cursor;
    	}

    	/* Read FRAMES */
    	frames_read = sf_readf_float(audio_data->sndfile,
                                 	p_float + out_off * ch,
                                 	frames_to_read);

    	/* Short read => EOF: if looping, wrap next iteration; else pad zeros */
    	if (frames_read < frames_to_read) {
        	sf_count_t remain = frames_to_read - frames_read;
        	memset(p_float + (out_off + frames_read) * ch, 0, (size_t)(remain * ch) * sizeof(float));
        	audio_data->done_reading = TRUE;
    	}

    	/* Peaks over frames_read (only actual samples) */
    	if (frames_read > 0) {
        	if (ch == 1) {
            	for (sf_count_t f = 0; f < frames_read; f++) {
                	float v = fabsf(p_float[out_off + f]);
                	if (v > maxl) maxl = v;
            	}
            	if (maxl > maxr) maxr = maxl;
        	} else {
            	for (sf_count_t f = 0; f < frames_read; f++) {
                	float vl = fabsf(p_float[(out_off + f) * 2 + 0]);
                	float vr = fabsf(p_float[(out_off + f) * 2 + 1]);
                	if (vl > maxl) maxl = vl;
                	if (vr > maxr) maxr = vr;
            	}
        	}
    	}

    	play_cursor += frames_read;
    	frames_written_total += frames_read;
    	out_off += frames_to_read; 	/* we filled frames_to_read (rest may be zeros) */
    	frames_needed -= frames_to_read;

    	/* If we padded zeros due to short read and not looping, stop */
    	if (audio_data->done_reading && !audio_is_looping) {
        	/* already padded zeros above, or will be padded on next loop */
        	break;
    	}
	}
	/* Update UI timebase using actual frames written (not zeros) */
	if (frames_written_total > 0) {
    	atomic_fetch_add_explicit(&rendered_frames_abs,
                              	(uint_fast64_t)frames_written_total,
                              	memory_order_relaxed);
	}
	/* Publish meters for UI */
	meter_ring_push(maxl, maxr);

	return noErr;
}

int process_audio(gfloat *pL, gfloat *pR)  //This function must be called repeatedly from the gint play_a_block until the section is played. 
{	//The pointers pL and pR passed in above return the levels for the VU meters.
	static int process_audio_call_count = 0;
	process_audio_call_count++;
	
	// Debug: show audio state for first few calls
	if (process_audio_call_count <= 5) {
		printf("DEBUG: process_audio() call #%d - audio_state=%d (IDLE=0, PLAYBACK=4)\n", 
		       process_audio_call_count, audio_state);
	}
	
	if(audio_state == AUDIO_IS_IDLE) 
	{
		d_print("process_audio says AUDIO_IS_IDLE is going on.\n") ;
		return 1 ;
    }
   	else if(audio_state == AUDIO_IS_PLAYBACK) 
	{
    	float l = 0.0f, r = 0.0f;

    	/* Consume a meter sample from the ring; if empty use last known */
    	if (!meter_ring_pop(&l, &r)) {
        	l = atomic_load_explicit(&last_meterL, memory_order_relaxed);
        	r = atomic_load_explicit(&last_meterR, memory_order_relaxed);
    	}

    	if (pL) *pL = (gfloat) l;
    	if (pR) *pR = (gfloat) r;
		return 0 ;
	}
	return 1 ;
}

int audio_device_open(char *output_device) 
{	
	OSStatus		err ;
	UInt32			count;
	
	printf("DEBUG: Opening CoreAudio device...\n");
	audio_data.device = kAudioDeviceUnknown ;
	
	/*  get the default output device for the HAL */
	count = sizeof (AudioDeviceID) ;
	if ((err = AudioHardwareGetProperty (kAudioHardwarePropertyDefaultOutputDevice,
										 &count, (void *) &(audio_data.device))) != noErr)
	{	printf ("AudioHardwareGetProperty failed with error: %d\n", (int)err) ;
		return -1 ;  // return of -1 means it didn't open
	}
	printf("DEBUG: CoreAudio device ID: %u\n", (unsigned int)audio_data.device);
	return 0; //All went well.  
}


int audio_device_set_params(AUDIO_FORMAT *format, int *channels, int *rate) //And start the audio playing.  (This is different from linux.)
{
	//stereo is 1 if it is stereo
	//playback_bits is the number of bits per sample
	//rate is the number of samples per second	
	
	OSStatus		err ;
	UInt32			count;
	
	printf("DEBUG: Setting audio parameters...\n");
	audio_data.sfinfo = sfinfo;
	audio_data.sndfile = sndfile;
	
	printf("DEBUG: sfinfo channels: %d, samplerate: %d\n", sfinfo.channels, sfinfo.samplerate);
	
	/*  get a description of the data format used by the default device */
	count = sizeof (AudioStreamBasicDescription) ;
	if ((err = AudioDeviceGetProperty (audio_data.device, 0, false, kAudioDevicePropertyStreamFormat,
										   &count, &(audio_data.format))) != noErr)
	{	printf ("AudioDeviceGetProperty (kAudioDevicePropertyStreamFormat) failed with error: %d\n", (int)err) ;
		return -1 ;
	} 

	/* FIX: return values to caller (do NOT reassign the pointer parameters) */
	if (rate) 	*rate 	= (int)audio_data.format.mSampleRate;
	if (channels) *channels = (int)audio_data.format.mChannelsPerFrame;
	
	printf("DEBUG: Device format - Sample rate: %f, Channels: %d\n", 
	       audio_data.format.mSampleRate, (int)audio_data.format.mChannelsPerFrame);
	
	//Don't mess with the format.  OS X uses floats which don't match GWC_S16_LE which is what is called for.
	
	
	/* Base setup completed. Now play. */
	if (audio_data.sfinfo.channels < 1 || audio_data.sfinfo.channels > 2)
	{	printf ("Error : channels = %d.\n", audio_data.sfinfo.channels) ;
		return -1;
	} 
	
	audio_data.format.mSampleRate = audio_data.sfinfo.samplerate ;
	audio_data.format.mChannelsPerFrame = audio_data.sfinfo.channels ;

	if (rate) 	*rate 	= (int)audio_data.format.mSampleRate;
	if (channels) *channels = (int)audio_data.format.mChannelsPerFrame;
	
	if ((err = AudioDeviceSetProperty (audio_data.device, NULL, 0, false, kAudioDevicePropertyStreamFormat,
										   sizeof (AudioStreamBasicDescription), &(audio_data.format))) != noErr)
	{	printf ("AudioDeviceSetProperty (kAudioDevicePropertyStreamFormat) failed.\n") ;
		return -1;
	} ;
	
	/*  we want linear pcm */
	if (audio_data.format.mFormatID != kAudioFormatLinearPCM)
	{	printf ("Data is not PCM.\n") ;
		return -1;
	} 
	
	
	/*We want to set the buffer size so that we can get one point per buffer size to run the VU meters. */
	buff_num = 0;
	buff_num_play = 0;
	num_buffers = (playback_end_position - playback_start_position)/BUFFERSIZE; 

	/* Reset meter ring for new playback */
	meter_ring_reset();	
	/* Set region cursor in sndfile frames (end is exclusive) */
	play_cursor = (sf_count_t)playback_start_position;
	play_end	= (sf_count_t)playback_end_position;
	/* Ensure timebase is re-initialised for cursor logic */
	start_sample_time = 0.0;
	playback_just_started = TRUE;

	printf("DEBUG: Audio setup - start_pos=%ld, end_pos=%ld, buffersize=%d, num_buffers=%ld\n",
	       playback_start_position, playback_end_position, BUFFERSIZE, num_buffers);
	       
	// CRITICAL FIX: Position file to playback start position
	printf("DEBUG: Seeking file to playback start position: %ld\n", playback_start_position);
	sf_count_t seek_result = sf_seek(audio_data.sndfile, playback_start_position, SEEK_SET);
	printf("DEBUG: File seek result: %lld (should equal %ld)\n", (long long)seek_result, playback_start_position);

	UInt32 bufferSize = BUFFERSIZE;
	if((err = AudioDeviceSetProperty( audio_data.device,
									  NULL, 0,
									  false,
									  kAudioDevicePropertyBufferFrameSize,
									  sizeof(UInt32),
									  &bufferSize)) != noErr)
	{
		printf("AudioDeviceAddIOProc failed to set buffer size. \n");
	}
	   
	/* If we are reconfiguring, stop and remove the old IOProc cleanly */
	if (coreaudio_device_started) {
    	AudioDeviceStop(audio_data.device, macosx_audio_out_callback);
    	coreaudio_device_started = FALSE;
	}
	if (coreaudio_ioproc_installed) {
    	AudioDeviceRemoveIOProc(audio_data.device, macosx_audio_out_callback);
    	coreaudio_ioproc_installed = FALSE;
	}

	/* Install IOProc once per configuration */
	if ((err = AudioDeviceAddIOProc(audio_data.device, macosx_audio_out_callback,
                                	(void *)&audio_data)) != noErr) {
    	printf("AudioDeviceAddIOProc failed with error: %d\n", (int)err);
    	return -1;
	}
	coreaudio_ioproc_installed = TRUE;
	printf("DEBUG: AudioDeviceAddIOProc completed successfully\n");

	/* Start the device once */
	if (!coreaudio_device_started) {
    	printf("DEBUG: Starting CoreAudio device from audio_device_set_params()...\n");
    	err = AudioDeviceStart(audio_data.device, macosx_audio_out_callback);
    	if (err != noErr) {
        	printf("ERROR: AudioDeviceStart failed with error: %d (0x%x)\n", (int)err, (unsigned int)err);
        	AudioDeviceRemoveIOProc(audio_data.device, macosx_audio_out_callback);
        	coreaudio_ioproc_installed = FALSE;
        	return -1;
    	}
    	coreaudio_device_started = TRUE;
	}
	
	/*
 	* IMPORTANT: initialise the cursor timebase immediately so the UI
 	* can move the cursor / stop playback even before the first callback.
 	*/
	{
    	AudioTimeStamp ts;
    	if (AudioDeviceGetCurrentTime(audio_data.device, &ts) == noErr) {
        	start_sample_time = ts.mSampleTime;
    	}
	}
	audio_data.done_playing = SF_FALSE ;
	audio_data.done_reading = FALSE;
	return 0;
}

int audio_device_read(unsigned char *buffer, int buffersize){return 0;} // Leave this stub function because we don't want to read data.
int audio_device_write(unsigned char *buffer, int buffersize){return 0;} // Not quite what the title says in OS X.
long audio_device_processed_bytes(void)
{
	extern int audio_playback;

	/* Use callback-driven counter for deterministic cursor/stop behaviour */
	uint_fast64_t frames_abs = atomic_load_explicit(&rendered_frames_abs, memory_order_relaxed);
	long region_len = (playback_end_position - playback_start_position);
	if (region_len < 1) region_len = 1;

	/* playback_position is a frame index in the file */
	if (audio_is_looping) {
    	playback_position = playback_start_position + (long)(frames_abs % (uint_fast64_t)region_len);
	} else {
    	long advanced = (long)frames_abs;
    	long pos = playback_start_position + advanced;
    	if (pos > playback_end_position) pos = playback_end_position;
    	playback_position = pos;
    	if (advanced >= region_len) {
        	audio_data.done_playing = SF_TRUE;
        	audio_playback = FALSE;
    	}
	}

	return (long)(frames_abs * (uint_fast64_t)PLAYBACK_FRAMESIZE);
}  // This is used to set the cursor.  We need to make this return a number controlled by a timer.

int audio_device_best_buffer_size(int playback_bytes_per_block)  //The result of this doesn't make any difference.
{
	OSStatus		err ;
	UInt32			count, buffer_size ;
	
	/*  get the buffersize that the default device uses for IO */
	count = sizeof (UInt32) ;
	if ((err = AudioDeviceGetProperty (audio_data.device, 0, false, kAudioDevicePropertyBufferSize,
									   &count, &buffer_size)) != noErr)
	{
		printf ("AudioDeviceGetProperty (AudioDeviceGetProperty) failed.\n") ;
		return -1;
	} ;
	return (int) buffer_size;
}

int audio_device_nonblocking_write_buffer_size(int maxbufsize,    //Normally returns the number of bytes the send buffer is ready for.
										   int playback_bytes_remaining)
{
	return 1;  // This allows the process_audio to move the VU meters.
}

void audio_device_close(int drain)  //Reminder: check to make sure this works when no device has been opened.
{
	OSStatus		err ;
	if (coreaudio_device_started) {
    	err = AudioDeviceStop(audio_data.device, macosx_audio_out_callback);
    	/* Even on success we must clear the flag */
    	coreaudio_device_started = FALSE;
	}
	
	if (coreaudio_ioproc_installed) {
    	err = AudioDeviceRemoveIOProc(audio_data.device, macosx_audio_out_callback);
    	coreaudio_ioproc_installed = FALSE;
    	if (err != noErr) {
        	printf("AudioDeviceRemoveIOProc failed.\n");
        	return;
    	}
	}

}

#pragma clang diagnostic pop

#endif /* MAC_OS_X */