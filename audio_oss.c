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

/* oss interface impl.  ...frank 12.09.03 */

#include <sys/ioctl.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

#if defined(__OpenBSD__) || defined(__NetBSD__)
#include <soundcard.h>
#else
#include <sys/soundcard.h>
#endif

#include "audio_device.h"

static int audio_fd = -1 ;
static int OSS_FRAMESIZE = 4 ;


int audio_device_open(char *output_device)
{
    if( (audio_fd = open(output_device, O_WRONLY)) == -1) {
	return -1;
    }
    return 0;
}

int audio_device_set_params(AUDIO_FORMAT *format, int *channels, int *rate)
{
    int oss_format;
    int sample_size ;

    switch (*format)
    {
    case GWC_U8:     oss_format = AFMT_U8; break;
    case GWC_S8:     oss_format = AFMT_S8; break;
    case GWC_S16_BE: oss_format = AFMT_S16_BE; break;
    default:
    case GWC_S16_LE: oss_format = AFMT_S16_LE; break;
    }

    if (ioctl(audio_fd, SNDCTL_DSP_SETFMT, &oss_format) == -1) {
        return -1;
    }

    switch (oss_format)
    {
    case AFMT_U8:     *format = GWC_U8; sample_size = 1 ; break;
    case AFMT_S8:     *format = GWC_S8; sample_size = 1 ; break;
    case AFMT_S16_BE: *format = GWC_S16_BE; sample_size = 2 ; break;
    case AFMT_S16_LE: *format = GWC_S16_LE; sample_size = 2 ; break;
    default:          *format = GWC_UNKNOWN; sample_size = 2 ; break;
    }

    OSS_FRAMESIZE=sample_size*(*channels) ;

    
    if (ioctl(audio_fd, SNDCTL_DSP_CHANNELS, channels) == -1) {
        return -1;
    }

    if (ioctl(audio_fd, SNDCTL_DSP_SPEED, rate) == -1) {
        return -1;
    }

    return 0;
}

int audio_device_read(unsigned char *buffer, int buffersize)
{
    int len = read(audio_fd, buffer, buffersize);
    if (len == -1)
        return -1;
    return len;
}

int audio_device_write(unsigned char *buffer, int buffersize)
{
    int len = write(audio_fd, buffer, buffersize);
    if (len == -1)
        return -1;
    return len;
}

void audio_device_close(int drain)
{
    if(audio_fd != -1) {
	ioctl(audio_fd, SNDCTL_DSP_RESET, NULL) ;
	close(audio_fd) ;
	audio_fd = -1 ;
    }
}

/* Number of bytes processed since opening the device. */
long audio_device_processed_bytes(void)
{
    count_info info;

    if (audio_fd != -1) {
	ioctl(audio_fd, SNDCTL_DSP_GETOPTR, &info);
	return info.bytes;
    }

    return 0;
}

/* Number of frames processed since opening the device. */
long audio_device_processed_frames(void)
{
    return audio_device_processed_bytes() / OSS_FRAMESIZE ;
}

int audio_device_best_buffer_size(int playback_bytes_per_block)
{
    int bufsize;
    audio_buf_info oss_info;

    ioctl(audio_fd, SNDCTL_DSP_GETOSPACE, &oss_info);

    for (bufsize = oss_info.fragsize;
         bufsize < oss_info.fragsize*oss_info.fragstotal/2;
         bufsize += oss_info.fragsize)
    {
	if (bufsize >= playback_bytes_per_block)
	    break;
    }
    return bufsize;
}

int audio_device_nonblocking_write_buffer_size(int maxbufsize,
                                               int playback_bytes_remaining)
{
    audio_buf_info info;
    int len = 0;

    ioctl(audio_fd, SNDCTL_DSP_GETOSPACE, &info);

/*  g_print("fragsize:%d\n", info.fragsize) ;  */
/*  g_print("fragstotal:%d\n", info.fragstotal) ;  */
/*  g_print("bytes:%ld\n", info.bytes) ;  */
	
    len = info.fragsize*info.fragments;
    while(len > maxbufsize) len -= info.fragsize;

    if (len > playback_bytes_remaining) {
	len = playback_bytes_remaining;
    }
/*  g_print("len:%d\n", len) ;  */
    if(len > info.bytes) {
/*      g_print("No free audio buffers\n") ;  */
        return 0 ;
    }
    return len;
}

