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
 *  audio_osx.h
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

#include "gwc.h"
#include "audio_device.h"

// Suppress deprecation warnings for CoreAudio APIs
// These APIs still work and updating to modern APIs would require significant refactoring
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

extern int wavefile_fd ;
extern int stereo;


typedef struct
{	AudioStreamBasicDescription		format ;
	UInt32 			buf_size ;
	AudioDeviceID 	device ;
	SNDFILE 		*sndfile ;
	SF_INFO 		sfinfo ;
	int 			done_playing ;
	bool			done_reading ;
} MacOSXAudioData ;

MacOSXAudioData 	audio_data ;

extern SNDFILE *sndfile;
extern SF_INFO sfinfo;
extern int audio_state;
extern long playback_end_position ;
extern long playback_position ;
extern long playback_start_position;
extern long playback_samples_remaining;
extern long playback_total_bytes ;
extern int FRAMESIZE;
int BUFFERSIZE = 1024; // The size of the buffers we will send.
long buff_num; // An index to allow us to create the array to run the VU meters.
long num_buffers; // The number of buffers we will send.
long buff_num_play;  // The index of the buffer we are playing.

gfloat* pL_global;
gfloat* pR_global;
bool p_global_mem_alloced = FALSE; //Tells if we have reserved memory for the two above arrays.

Float64 start_sample_time;
struct timeval playback_start_time;
bool playback_just_started = FALSE;
static bool coreaudio_device_started = FALSE;  // Track if device has been started


static OSStatus
macosx_audio_out_callback (AudioDeviceID device, const AudioTimeStamp* current_time,
						   const AudioBufferList* data_in, const AudioTimeStamp* time_in,
						   AudioBufferList*	data_out, const AudioTimeStamp* time_out,
						   void* client_data)
{	
	static int callback_count = 0;
	callback_count++;
	if (callback_count <= 20) {  // Increase the limit to see more callbacks
		printf("DEBUG: Audio callback called #%d\n", callback_count);
	}
	
	MacOSXAudioData	*audio_data ;
	int	size, sample_count, read_count, i ;
	
	float maxl = 0, maxr = 0;
	float *p_float;
	
	if (playback_just_started)
	{
		playback_just_started = FALSE;
		start_sample_time = time_out->mSampleTime;
		printf("DEBUG: Playback started, sample time: %f\n", start_sample_time);
	}
	
	audio_data = (MacOSXAudioData*) client_data ;
	
	if (callback_count <= 5) {
		printf("DEBUG: Callback - audio_data=%p, sndfile=%p\n", audio_data, audio_data ? audio_data->sndfile : NULL);
		
		// Get file position and info for debugging
		if (audio_data && audio_data->sndfile) {
			sf_count_t pos = sf_seek(audio_data->sndfile, 0, SEEK_CUR);
			printf("DEBUG: Current file position: %lld\n", (long long)pos);
			
			// Check if file is valid
			if (sf_error(audio_data->sndfile) != SF_ERR_NO_ERROR) {
				printf("DEBUG: libsndfile error: %s\n", sf_strerror(audio_data->sndfile));
			}
			
			// Get file info
			SF_INFO info;
			memset(&info, 0, sizeof(info));
			if (sf_command(audio_data->sndfile, SFC_GET_CURRENT_SF_INFO, &info, sizeof(info)) == SF_TRUE) {
				printf("DEBUG: File info - frames: %lld, channels: %d, samplerate: %d\n", 
				       (long long)info.frames, info.channels, info.samplerate);
			} else {
				printf("DEBUG: Could not get file info\n");
			}
		}
	}
	
	size = data_out->mBuffers[0].mDataByteSize ;  
	sample_count = size / sizeof (float) ;  // The number of bytes to send.
	
	p_float = (float*) data_out->mBuffers [0].mData ;  // Makes buffer point to the data.
	if((!(audio_data->done_reading))&&(buff_num < num_buffers))
	{
		read_count = sf_read_float (audio_data->sndfile, p_float, sample_count) ;
		if (callback_count <= 5) {
			printf("DEBUG: sf_read_float returned %d samples (requested %d)\n", read_count, sample_count);
			printf("DEBUG: Callback #%d - Writing %d samples to output buffer\n", callback_count, read_count);
		}
		if(read_count < sample_count)
		{
			memset (&(p_float [read_count]), 0, (sample_count - read_count) * sizeof (float)) ; //set the rest of the buffer to 0.
			audio_data->done_reading = SF_TRUE;
			if (callback_count <= 5) {
				printf("DEBUG: Reached end of audio data\n");
			}
		}
		for(i = 0; i < read_count; i++) //Find the level for the VU meters
		{  
			float vl, vr;
			vl = p_float[i];
			vr = p_float[i+1];
			
			if(vl > maxl) maxl = vl ;
			if(-vl > maxl) maxl = -vl ;
			
			if(stereo) {
				i++ ;
				if(vr > maxr) maxr = vr ;
				if(-vr > maxr) maxr = -vr ;
			} else {
				maxr = maxl ;
			}
		}
		pL_global[buff_num] = (gfloat) maxl;
		pR_global[buff_num] = (gfloat) maxr;
		buff_num++;
		
		return noErr ;
	} else {
		if (callback_count <= 5) {
			printf("DEBUG: Audio callback - no data to play (done_reading=%d, buff_num=%ld, num_buffers=%ld)\n",
			       audio_data->done_reading, buff_num, num_buffers);
		}
		// Fill buffer with silence
		memset(p_float, 0, size);
	}
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
		// Start CoreAudio device on first process_audio call if not already started
		if (!coreaudio_device_started) {
			OSStatus err;
			printf("DEBUG: Starting CoreAudio device from process_audio()...\n");
			err = AudioDeviceStart(audio_data.device, macosx_audio_out_callback);
			if (err != noErr) {
				printf("ERROR: AudioDeviceStart failed with error: %d (0x%x)\n", (int)err, (unsigned int)err);
				return 1;
			}
			printf("DEBUG: CoreAudio device started successfully from process_audio()\n");
			
			// Verify the device is actually running
			UInt32 isRunning = 0;
			UInt32 size = sizeof(UInt32);
			err = AudioDeviceGetProperty(audio_data.device, 0, false, kAudioDevicePropertyDeviceIsRunning, &size, &isRunning);
			if (err == noErr) {
				printf("DEBUG: Device running verification: %s\n", isRunning ? "YES" : "NO");
			} else {
				printf("DEBUG: Could not verify device running status, error: %d\n", (int)err);
			}
			
			coreaudio_device_started = TRUE;
			
			// Give the device a moment to fully initialize
			usleep(10000); // 10ms delay
		}
		
		if (process_audio_call_count <= 5) {
			printf("DEBUG: process_audio() call #%d - buff_num_play=%ld\n", process_audio_call_count, buff_num_play);
		}
		
		printf("DEBUG: In PLAYBACK mode, returning VU levels: pL=%f, pR=%f\n", 
		       pL_global[buff_num_play], pR_global[buff_num_play]);
		
		*pL = pL_global[buff_num_play];
		*pR = pR_global[buff_num_play];
		buff_num_play++;
		return 0 ;
	}
	return 1 ;
}

int audio_device_open(char *output_device) 
{   
	OSStatus	err ;
	UInt32		count ;
	
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
	
	OSStatus	err ;
	UInt32		count;
	
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
	
	rate = (int *) &(audio_data.format.mSampleRate);
	channels = (int *) &(audio_data.format.mChannelsPerFrame);
	
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
	rate = (int *) &(audio_data.format.mSampleRate);
	channels = (int *) &(audio_data.format.mChannelsPerFrame);
	
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
	
	printf("DEBUG: Audio setup - start_pos=%ld, end_pos=%ld, buffersize=%d, num_buffers=%ld\n",
	       playback_start_position, playback_end_position, BUFFERSIZE, num_buffers);
	       
	// CRITICAL FIX: Position file to playback start position
	printf("DEBUG: Seeking file to playback start position: %ld\n", playback_start_position);
	sf_count_t seek_result = sf_seek(audio_data.sndfile, playback_start_position, SEEK_SET);
	printf("DEBUG: File seek result: %lld (should equal %ld)\n", (long long)seek_result, playback_start_position);
	
	if (!p_global_mem_alloced)
	{
		p_global_mem_alloced = TRUE;
		pL_global = (gfloat*) malloc(num_buffers*sizeof(gfloat)); // When do I need to free this?
		pR_global = (gfloat*) malloc(num_buffers*sizeof(gfloat));
	}
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
	   
	   /* Fire off the device. */
	if ((err = AudioDeviceAddIOProc (audio_data.device, macosx_audio_out_callback,
									 (void *) &audio_data)) != noErr)
	{	printf ("AudioDeviceAddIOProc failed with error: %d\n", (int)err) ;
		return -1;
	} 
	printf("DEBUG: AudioDeviceAddIOProc completed successfully\n");
	
	// Test: Try to immediately start the device to see if callback gets triggered
	printf("DEBUG: Testing immediate device start...\n");
	err = AudioDeviceStart(audio_data.device, macosx_audio_out_callback);
	if (err != noErr) {
		printf("DEBUG: Immediate AudioDeviceStart failed with error: %d\n", (int)err);
	} else {
		printf("DEBUG: Immediate AudioDeviceStart succeeded - KEEPING DEVICE STARTED\n");
		// Give it a moment to see if callback gets called
		usleep(100000); // 100ms
		printf("DEBUG: After 100ms delay, checking if callback was called...\n");
		// DON'T stop the device - keep it running!
		coreaudio_device_started = TRUE; // Mark as started so process_audio won't try to start again
	}
	
	printf("DEBUG: CoreAudio device configured but NOT started yet (will start on first process_audio call)\n");
	// DON'T start the device here - let process_audio() start it when needed
	// err = AudioDeviceStart (audio_data.device, macosx_audio_out_callback) ;
	playback_just_started = TRUE;
	audio_data.done_playing = SF_FALSE ;
	audio_data.done_reading = FALSE;
	return 0; //All went well.  
}

int audio_device_read(unsigned char *buffer, int buffersize){return 0;} // Leave this stub function because we don't want to read data.
int audio_device_write(unsigned char *buffer, int buffersize){return 0;} // Not quite what the title says in OS X.
long audio_device_processed_bytes(void)
{
	AudioTimeStamp this_time;
	OSStatus err;
	UInt32 num_processes;
	UInt32		count;
	extern int audio_playback ;
	
	count = sizeof(UInt32);
	if ((err = AudioDeviceGetProperty (audio_data.device, 0, false, kAudioDevicePropertyDeviceIsRunning,
									   &count, &num_processes)) != noErr)
	{	printf ("AudioDeviceGetProperty (AudioDeviceGetProperty) failed.  The device probably isn't running.\n") ;
		return -1;
	} 
	else
	{
		printf("DEBUG: Device is running check - num_processes=%u\n", (unsigned int)num_processes);
		
		if((err = AudioDeviceGetCurrentTime(audio_data.device, &this_time)) != noErr)
		{
			printf("Could not get the current time.  The error number is: %i (device running: %u)\n", err, (unsigned int)num_processes);
			// If we can't get time, just increment playback_position manually
			playback_position += BUFFERSIZE; // Rough estimate
		}
		else
		{
			playback_position = (long) (this_time.mSampleTime - start_sample_time);//*FRAMESIZE;
																				   //led_bar_light_percent(dial[0], l);  
																				   //led_bar_light_percent(dial[1], r);
		}
	}
	if (playback_position >= playback_end_position)  //We are done playing.
	{	
		/* Tell the main application to terminate. */
		audio_data.done_playing = SF_TRUE ;
		audio_playback = FALSE;
	}
	
	return playback_position*FRAMESIZE
		;
}  // This is used to set the cursor.  We need to make this return a number controlled by a timer.

int audio_device_best_buffer_size(int playback_bytes_per_block)  //The result of this doesn't make any difference.
{
	OSStatus	err ;
	UInt32		count, buffer_size ;
	
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
	OSStatus	err ;
	if(p_global_mem_alloced)
	{
		free(pL_global);  
		free(pR_global);
		p_global_mem_alloced = FALSE;
	}
	if ((err = AudioDeviceStop (audio_data.device, macosx_audio_out_callback)) != noErr)
	{	//printf ("AudioDeviceStop failed.\n") ;  //Need to comment out this line in deployment build.
		return ;
	} ;
	
	err = AudioDeviceRemoveIOProc (audio_data.device, macosx_audio_out_callback) ;
	if (err != noErr)
	{	printf ("AudioDeviceRemoveIOProc failed.\n") ;//Need to comment out this line in deployment build.
		return ;
	} ;
}

#pragma clang diagnostic pop

#endif /* MAC_OS_X */

