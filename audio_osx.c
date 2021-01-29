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

#include </System/Library/Frameworks/Carbon.framework/Versions/A/Headers/Carbon.h>
#include <CoreAudio/AudioHardware.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <time.h>

#include "gwc.h"
#include "audio_device.h"

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
extern long playback_end_frame ;
extern long playback_read_frame_position ;
extern long playback_start_frame;
extern long playback_frames_not_output;
extern long playback_total_bytes ;
extern int FRAMESIZE;
int BUFFERSIZE = 1024; // The size of the buffers we will send.
long buff_num; // An index to allow us to create the array to run the VU meters.
long num_buffers; // The number of buffers we will send.
long buff_num_play;  // The index of the buffer we are playing.

Float64 start_sample_time;
struct timeval playback_start_time;
bool playback_just_started = FALSE;


static OSStatus
macosx_audio_out_callback (AudioDeviceID device, const AudioTimeStamp* current_time,
						   const AudioBufferList* data_in, const AudioTimeStamp* time_in,
						   AudioBufferList*	data_out, const AudioTimeStamp* time_out,
						   void* client_data)
{	
	MacOSXAudioData	*audio_data ;
	int	size, sample_count, read_count, i ;
	
	float maxl = 0, maxr = 0;
	float *p_float;
	
	audio_data = (MacOSXAudioData*) client_data ;
	
	size = data_out->mBuffers[0].mDataByteSize ;  
	sample_count = size / sizeof (float) ;  // The number of bytes to send.
	
	p_float = (float*) data_out->mBuffers [0].mData ;  // Makes buffer point to the data.
	if((!(audio_data->done_reading))&&(buff_num < num_buffers))
	{
		read_count = sf_read_float (audio_data->sndfile, p_float, sample_count) ;
		if(read_count < sample_count)
		{
			memset (&(p_float [read_count]), 0, (sample_count - read_count) * sizeof (float)) ; //set the rest of the buffer to 0.
			audio_data->done_reading = SF_TRUE;
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

			extern gint n_frames_in_led_levels ;
			extern int audio_is_looping ;
			extern gint totblocks_in_led_levels ;
			extern int buffered_looped_count ;
			extern gfloat *led_levels_l ;
			extern gfloat *led_levels_r ;
			extern gint LED_LEVEL_FRAME_SIZE ;

			if(audio_is_looping == FALSE || buffered_looped_count == 0) {
			    // if looping, only need to do this for first time through the loop
			    int current_level_block=(n_frames_in_led_levels-1)/LED_LEVEL_FRAME_SIZE ;
			    if(current_level_block > totblocks_in_led_levels-1) {
				fprintf(stderr, "Uh-oh, current_level_block:%d, max:%d\n", current_level_block, totblocks_in_led_levels-1) ;
				current_level_block = totblocks_in_led_levels-1 ;
			    }

			    if(vl < 0) vl = -vl ;

			    // note -- since the data was read using sf_read_float, the data is normalized to [-1,1], so
			    // there is no division by (/maxpossible) as is done in audio_util.c
			    if(led_levels_l[current_level_block] < (gfloat)vl)
				led_levels_l[current_level_block] = (gfloat)vl ;

			    if(stereo) {
				if(vr < 0) vr = -vr ;
				if(led_levels_r[current_level_block] < (gfloat)vr)
				    led_levels_r[current_level_block] = (gfloat)vr ;
			    } else {
				led_levels_r[current_level_block] = led_levels_l[current_level_block] ;
			    }

			    n_frames_in_led_levels++ ;
			}
		}
		
		return noErr ;
	}
	return noErr;
} 

int process_audio(gfloat *pL, gfloat *pR)
{
	if(audio_state == AUDIO_IS_IDLE) 
	{
		d_print("process_audio says AUDIO_IS_IDLE is going on.\n") ;
		return 1 ;
	}
   	else if(audio_state&(AUDIO_IS_BUFFERING|AUDIO_IS_RECORDING))
	{
		return 0 ;
	}
	return 1 ;
}

int audio_device_open(char *output_device) 
{   
	OSStatus	err ;
	UInt32		count ;
	
	audio_data.device = kAudioDeviceUnknown ;
	
	/*  get the default output device for the HAL */
	count = sizeof (AudioDeviceID) ;
	if ((err = AudioHardwareGetProperty (kAudioHardwarePropertyDefaultOutputDevice,
										 &count, (void *) &(audio_data.device))) != noErr)
	{	printf ("AudioHardwareGetProperty failed.\n") ;
		return -1 ;  // return of -1 means it didn't open
	}
	return 0; //All went well.  
}

/* coreaudio apparently has a "frame" clock running, that does not necessarily start at 0 when the audio device itself starts (via audio_device_open())
   this function retrieves that frame number
*/
Float64 coreaudio_running_clock_time(void)
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
		
		if((err = AudioDeviceGetCurrentTime(audio_data.device, &this_time)) != noErr)
		{
			printf("Could not get the current time.  The error number is: %i \n", err);
			return -1 ;
		}
		else
		{
			// it appears that mSampleTime really is the sample frame number, it is not a time.   Just a poorly named variable in coreaudio
			return this_time.mSampleTime ;
		}
	}
}


int audio_device_set_params(AUDIO_FORMAT *format, int *channels, int *rate) //And start the audio playing.  (This is different from linux.)
{
	//stereo is 1 if it is stereo
	//playback_bits is the number of bits per sample
	//rate is the number of samples per second	
	
	OSStatus	err ;
	UInt32		count;
	
	audio_data.sfinfo = sfinfo;
	audio_data.sndfile = sndfile;
	
	
	/*  get a description of the data format used by the default device */
	count = sizeof (AudioStreamBasicDescription) ;
	if ((err = AudioDeviceGetProperty (audio_data.device, 0, false, kAudioDevicePropertyStreamFormat,
									   &count, &(audio_data.format))) != noErr)
	{	printf ("AudioDeviceGetProperty (kAudioDevicePropertyStreamFormat) failed.\n") ;
		return -1 ;
	} 
	
	rate = (int *) &(audio_data.format.mSampleRate);
	channels = (int *) &(audio_data.format.mChannelsPerFrame);
	
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
	}
	
	/*  we want linear pcm */
	if (audio_data.format.mFormatID != kAudioFormatLinearPCM)
	{	printf ("Data is not PCM.\n") ;
		return -1;
	} 
	
	
	/*We want to set the buffer size so that we can get one point per buffer size to run the VU meters. */
	buff_num = 0;
	buff_num_play = 0;
	num_buffers = (playback_end_frame - playback_start_frame)/BUFFERSIZE; 
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
	{	printf ("AudioDeviceAddIOProc failed.\n") ;
		return -1;
	} 
	
	err = AudioDeviceStart (audio_data.device, macosx_audio_out_callback) ;
	if	(err != noErr) return -1;

	audio_data.done_playing = SF_FALSE ;
	audio_data.done_reading = FALSE;
	
	// ideally a few microseconds of latency would be added to this, let's
	// see how it works -- JJW 01/28/2021
	start_sample_time = coreaudio_running_clock_time() ;

	return 0; //All went well.  
}

int audio_device_read(unsigned char *buffer, int buffersize){return 0;} // Leave this stub function because we don't want to read data.
int audio_device_write(unsigned char *buffer, int buffersize){return 0;} // Not quite what the title says in OS X.

long audio_device_processed_frames(void)
{
	Float64 running_frame_number = coreaudio_running_clock_time() ;

	if(running_frame_number < 0.)
	    return (long)running_frame_number ;

	playback_read_frame_position = (long)(running_frame_number - start_sample_time);

	if (playback_read_frame_position >= playback_end_frame)  //We are done playing.
	{	
		/* Tell the main application to terminate. */
		audio_data.done_playing = SF_TRUE ;
		audio_playback = FALSE;
	}
	
	printf("DEBUG:============ OSX says current Frame # is %ld\n", playback_read_frame_position) ;
	return playback_read_frame_position ;
}  // This is used to set the cursor.  We need to make this return a number controlled by a timer.

long audio_device_processed_bytes(void)
{
    return audio_device_processed_frames()*FRAMESIZE ;
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


#endif /* MacOSX */

