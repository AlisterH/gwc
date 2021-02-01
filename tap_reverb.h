/*                                                     -*- linux-c -*-
    Copyright (C) 2004 Tom Szilagyi

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    $Id: sound.h,v 1.3 2004/06/09 18:58:17 tszilagyi Exp $
*/


#ifndef _tap_reverb_h
#define _tap_reverb_h



typedef float reverb_audio_sample_t ;

void comp_coeffs(void);
void load_revtype_data(void);
void reverb_init(void);
int reverb_process(long nframes, reverb_audio_sample_t *output_L, reverb_audio_sample_t *input_L, reverb_audio_sample_t *output_R, reverb_audio_sample_t *input_R) ;
int reverb_setup(long sample_rate, double decay_d, double wet_d, double dry_d, char *name) ;
void reverb_finish(void *arg);


#endif /* _tap_reverb_h */
