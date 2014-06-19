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

/* soundfile.h some functions to manipulate the sound file ...frank 4.10.03 */

#ifndef SOUNDFILE_H
#define SOUNDFILE_H

/* functions for soundfile file */

long soundfile_count_samples_in_file(char *filename);
int  soundfile_save_file(char *filename, long pos, long sample_count, int status_info);
int  soundfile_load_file(char *filename, long pos, long sample_count, int status_info);

/* functions for the manipulating of the current loaded soundfile */

int soundfile_shift_samples_right(long first_pos, long sample_count, int status_info);
int soundfile_shift_samples_left(long first_pos, long sample_count, int status_info);

long soundfile_count_samples(void);
int  soundfile_insert_silence(long insert_pos, long sample_count, int status_info);
int  soundfile_insert_samples(long insert_pos, long sample_count, int *sample_data, int status_info);
int  soundfile_remove_samples(long first_pos, long sample_count, int status_info);

#endif /* SOUNDFILE_H */

