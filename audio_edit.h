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

/* audio_edit.h some functions to cut, copy, paste, ...frank 4.10.03 */

#ifndef AUDIO_EDIT_H
#define AUDIO_EDIT_H

struct view;

int audioedit_has_clipdata(void);
int audioedit_cut_selection(struct view *v);
int audioedit_copy_selection(struct view *v);
int audioedit_paste_selection(struct view *v);
int audioedit_delete_selection(struct view *v);
int audioedit_insert_silence(struct view *v);
#ifdef TRUNCATE_OLD
void truncate_wavfile(struct view *v);
#else
void truncate_wavfile(struct view *v, int save_undo);
#endif
#endif /* AUDIO_EDIT_H */

