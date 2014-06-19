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

    $Id: common.h,v 1.4 2004/06/12 13:56:46 tszilagyi Exp $
*/


#ifndef _tap_reverb_common_h
#define _tap_reverb_common_h


/* enlarge this if you need to */
#define MAX_SAMPLERATE    192000


#define MAXLEN            32

#define MAX_COMBS         20
#define MAX_ALLPS         20

#define MAX_DECAY         10000.0f
#define MAX_COMB_DELAY    250.0f
#define MAX_ALLP_DELAY    20.0f

#define IMPRESP_MAXLEN    10000

/* file names */
#define NAME_REVERBED ".reverbed"
#define NAME_TAP_H "tap_reverb_presets.h"
#define NAME_TAP_RDF "tap_reverb.rdf"


/* color definitions */
#define NORMAL_R 48000
#define NORMAL_G 51000
#define NORMAL_B 55000
#define ACTIVE_R 40000
#define ACTIVE_G 43000
#define ACTIVE_B 47000
#define PRELIGHT_R 50000
#define PRELIGHT_G 55000
#define PRELIGHT_B 60000

/* toggle_buttons that are green when depressed */
/* it's actually blue, heh :) */
#define G_ACTIVE_R 29000
#define G_ACTIVE_G 29000
#define G_ACTIVE_B 65535
#define G_PRELIGHT_R 39000
#define G_PRELIGHT_G 39000
#define G_PRELIGHT_B 65535

/* toggle_buttons that are red when depressed */
#define R_ACTIVE_R 65535
#define R_ACTIVE_G 15000
#define R_ACTIVE_B 15000
#define R_PRELIGHT_R 65535
#define R_PRELIGHT_G 25000
#define R_PRELIGHT_B 25000

/* widgets on the notebook */
#define N_NORMAL_R 42000
#define N_NORMAL_G 45000
#define N_NORMAL_B 49000
#define N_ACTIVE_R 34000
#define N_ACTIVE_G 37000
#define N_ACTIVE_B 41000
#define N_PRELIGHT_R 44000
#define N_PRELIGHT_G 49000
#define N_PRELIGHT_B 54000

/* color of the notebook */
#define NB_NORMAL_R 52000
#define NB_NORMAL_G 55000
#define NB_NORMAL_B 59000
#define NB_ACTIVE_R 44000
#define NB_ACTIVE_G 47000
#define NB_ACTIVE_B 51000
#define NB_PRELIGHT_R 54000
#define NB_PRELIGHT_G 59000
#define NB_PRELIGHT_B 64000

/* window backgrounds */
#define WINDOW_R 58000
#define WINDOW_G 58000
#define WINDOW_B 62535


#endif /* _tap_reverb_common_h */
