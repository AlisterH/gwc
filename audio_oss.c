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

#include <string.h>
#include <errno.h>
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
#include "gwc.h"

static int audio_fd = -1 ;

int audio_device_open(char *output_device)
{
    int flags = O_WRONLY; /* safest if read() exists / some devices are quirky */
    audio_fd = open(output_device, flags);
    if (audio_fd == -1) {
        char buf[512];
        snprintf(buf, sizeof(buf),
                 "Failed to open OSS audio device %s: %s",
                 output_device ? output_device : "(null)",
                 strerror(errno));
        warning(buf);
        return -1;
    }
    return 0;
}

int audio_device_set_params(AUDIO_FORMAT *format, int *channels, int *rate)
{
    int oss_format;

    switch (*format)
    {
    case GWC_U8:     oss_format = AFMT_U8; break;
    case GWC_S8:     oss_format = AFMT_S8; break;
    case GWC_S16_BE: oss_format = AFMT_S16_BE; break;
    default:
    case GWC_S16_LE: oss_format = AFMT_S16_LE; break;
    }

    if (ioctl(audio_fd, SNDCTL_DSP_SETFMT, &oss_format) == -1) {
        warning("Failed to set audio format.");
        return -1;
    }

    switch (oss_format)
    {
    case AFMT_U8:     *format = GWC_U8; break;
    case AFMT_S8:     *format = GWC_S8; break;
    case AFMT_S16_BE: *format = GWC_S16_BE; break;
    case AFMT_S16_LE: *format = GWC_S16_LE; break;
    default:          *format = GWC_UNKNOWN; break;
    }

    
    if (ioctl(audio_fd, SNDCTL_DSP_CHANNELS, channels) == -1) {
        warning("Failed to set audio channels.");
        return -1;
    }

    if (ioctl(audio_fd, SNDCTL_DSP_SPEED, rate) == -1) {
        warning("Failed to set audio speed.");
        return -1;
    }

    return 0;
}

int audio_device_read(unsigned char *buffer, int buffersize)
{
    ssize_t len;
    do {
        len = read(audio_fd, buffer, (size_t)buffersize);
    } while (len == -1 && errno == EINTR);

    if (len == -1) {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "Error reading from OSS audio device: %s",
                 strerror(errno));
        warning(buf);
        return -1;
    }
    return (int)len;
}

int audio_device_write(unsigned char *buffer, int buffersize)
{
    int total = 0;

    while (total < buffersize) {
        ssize_t n = write(audio_fd,
                          buffer + total,
                          (size_t)(buffersize - total));

        if (n > 0) {
            total += (int)n;
            continue;
        }
        if (n == -1 && errno == EINTR)
            continue;

        if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            /* Non-blocking: report partial progress */
            return total;
        }

        {
            char buf[256];
            snprintf(buf, sizeof(buf),
                     "Error writing to OSS audio device: %s",
                     strerror(errno));
            warning(buf);
        }
        return -1;
    }
    return total;
}

void audio_device_close(int drain)
{
    if (audio_fd != -1) {
        int arg = 0;
        if (drain) {
	    /*
	    * Zero-length write "kick":
	    * Some OSS emulations (padsp / PulseAudio) fail to notice
	    * end-of-stream unless a final write occurs.
	    * This is harmless on real OSS and may help ensure SYNC drains.
	    */
            (void)write(audio_fd, NULL, 0);
            (void)ioctl(audio_fd, SNDCTL_DSP_SYNC, &arg);
        } else {
            (void)ioctl(audio_fd, SNDCTL_DSP_RESET, &arg);
        }
        close(audio_fd);
        audio_fd = -1;
    }
}
/* Number of bytes processed since opening the device. */
long audio_device_processed_bytes(void)
{
    count_info info;

    if (audio_fd != -1) {
	if (ioctl(audio_fd, SNDCTL_DSP_GETOPTR, &info) == -1) {
            warning("Error getting processed bytes from audio device.");
            return 0;
        }
	return info.bytes;
    }

    return 0;
}

int audio_device_best_buffer_size(int playback_bytes_per_block)
{
    int bufsize;
    audio_buf_info oss_info;

    if (ioctl(audio_fd, SNDCTL_DSP_GETOSPACE, &oss_info) == -1) {
        warning("Error getting buffer space from audio device.");
        return 0;
    }

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

    if (ioctl(audio_fd, SNDCTL_DSP_GETOSPACE, &info) == -1) {
        warning("Error getting buffer space from audio device.");
        return 0;
    }

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
