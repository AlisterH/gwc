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

/* gwc audio device interface  ...frank 12.09.03 */

#ifndef AUDIO_DEVICE_H
#define AUDIO_DEVICE_H

#define AUDIO_IS_IDLE 0x00
#define AUDIO_IS_MONITOR 0x01
#define AUDIO_IS_RECORDING  0x02
#define AUDIO_IS_PLAYBACK  0x04


typedef enum {
    GWC_U8,
    GWC_S8,
    GWC_S16_BE,
    GWC_S16_LE,
    GWC_UNKNOWN
} AUDIO_FORMAT;

#define MAXBUFSIZE 32768

int audio_device_open(char *output_device);
int audio_device_set_params(AUDIO_FORMAT *format, int *channels, int *rate);
int audio_device_read(unsigned char *buffer, int buffersize);
int audio_device_write(unsigned char *buffer, int buffersize);
void audio_device_close(int);
long audio_device_processed_bytes(void);
int audio_device_best_buffer_size(int playback_bytes_per_block);
int audio_device_nonblocking_write_buffer_size(int maxbufsize,
                                               int playback_bytes_remaining);

#endif /* AUDIO_DEVICE_H */

