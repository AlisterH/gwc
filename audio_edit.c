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

/* audio_edit.c some functions to cut, copy, paste, ...frank 4.10.03 */

#include <stdio.h>
#include "audio_edit.h"
#include "gwc.h"
#include "soundfile.h"

#ifndef TRUNCATE_OLD

extern struct sound_prefs prefs;

#define CLIPBOARD_FILE "gwc_intclip.dat"

static void adjust_view(struct view *v)
{
    prefs.n_samples = soundfile_count_samples();

    v->n_samples = prefs.n_samples;

    if (v->last_sample > v->n_samples - 1) {
        v->first_sample = v->n_samples - 1 - (v->last_sample - v->first_sample);
        v->last_sample  = v->n_samples - 1;
    }

    if (v->first_sample < 0)
        v->first_sample = 0;

    set_scroll_bar(prefs.n_samples - 1, v->first_sample, v->last_sample);
}

static void begin_operation(char *status_text)
{
    push_status_text(status_text);
    update_status_bar(0.0, STATUS_UPDATE_INTERVAL, TRUE) ;
}

static void end_operation(void)
{
    pop_status_text();
    update_status_bar(0.0, STATUS_UPDATE_INTERVAL, TRUE);
}

static void resample(long first, long last)
{
    begin_operation("Resampling audio data");
    resample_audio_data(&prefs, first, last);
    end_operation();
    save_sample_block_data(&prefs);
    display_times();
}

long audioedit_count_samples_in_clipdata(void)
{
    return soundfile_count_samples_in_file(CLIPBOARD_FILE);
}

int audioedit_has_clipdata(void)
{
    return soundfile_count_samples_in_file(CLIPBOARD_FILE) > 0;
}

int audioedit_cut_selection(struct view *v)
{
    int rc = audioedit_copy_selection(v);
    if (rc == 0) {
        rc = audioedit_delete_selection(v);
    }
    return rc;
}

int audioedit_copy_selection(struct view *v)
{
    int rc;
    long first, last;
    get_region_of_interest(&first, &last, v);

    begin_operation("Saving internal clipboard");
    rc = soundfile_save_file(CLIPBOARD_FILE, first, last - first + 1, 1);
    end_operation();

    return rc;
}

int audioedit_paste_selection(struct view *v)
{
    int rc = -1;
    long sample_count;

    sample_count = audioedit_count_samples_in_clipdata();
    if (sample_count > 0)
    {
        long first, last;
        char undo_label[200];
        get_region_of_interest(&first, &last, v);

        sprintf(undo_label, "Paste audio data from %ld to %ld.", first, last);
        if (start_save_undo(undo_label, v) < 0)
            return -1;
        save_undo_data_insert(first, last, 1);
        close_undo();

        begin_operation("Inserting space for clipboard data") ;
        rc = soundfile_shift_samples_right(first, sample_count, 1);
        end_operation();

        if (rc == 0) {
            begin_operation("Inserting clipboard data");
            rc = soundfile_load_file(CLIPBOARD_FILE, first, sample_count, 1);
            end_operation();

            adjust_view(v);
            v->selection_region = FALSE;

            resample(first, prefs.n_samples-1);
        }
    }

    return rc;
}

int audioedit_delete_selection(struct view *v)
{
    int rc = -1;
    long first, last;
    char undo_label[200];

    get_region_of_interest(&first, &last, v);

    sprintf(undo_label, "Delete audio data from %ld to %ld.", first, last);
    if (start_save_undo(undo_label, v) < 0)
        return -1;
    rc = save_undo_data_remove(first, last, 1);
    close_undo();
    if (rc == 1) /* canceled */
        return -1;

    begin_operation("Deleting audio data") ;
    rc = soundfile_remove_samples(first, last - first + 1, 1);
    end_operation();

    if (rc == 0) {
        adjust_view(v);
        v->selection_region = FALSE;
        resample(first, prefs.n_samples-1);
    }
    return rc;
}

int audioedit_insert_silence(struct view *v)
{
    long first, last;

    get_region_of_interest(&first, &last, v);
    if (v->selection_region) {
        char undo_label[200];

        sprintf(undo_label, "Insert silence from %ld to %ld.", first, last);
        if (start_save_undo(undo_label, v) < 0)
            return -1;
        save_undo_data_insert(first, last, 1);
        close_undo();

        begin_operation("Inserting silence") ;
        soundfile_insert_silence(first, last - first + 1, 1);
        end_operation();

        adjust_view(v);
        resample(first, prefs.n_samples-1);
        return 0;
    }
    return -1;
}

void truncate_wavfile(struct view *v, int save_undo)
{
    int rc;
    long first, last;
    long end_pos = soundfile_count_samples();
    char undo_label[200];

    get_region_of_interest(&first, &last, v);

    if (save_undo) {
        /* split undo-data to undo separately in case of failure */
        sprintf(undo_label, "Delete audio data from %ld to %ld.", last+1, end_pos);
        if (start_save_undo(undo_label, v) < 0)
            return;
        rc = save_undo_data_remove(last+1, end_pos, 1);
        close_undo();
        if (rc == 1) /* canceled */
            return;
    }

    begin_operation("Deleting audio data") ;
    rc = soundfile_remove_samples(last+1, end_pos, 1);
    end_operation();

    if (rc != 0) {
        /* failure; FIX: needs cleanup of undo and more */
        return;
    }

    if (save_undo) {
        /* second part of undo */
        sprintf(undo_label, "Delete audio data from 0 to %ld.", first-1);
        if (start_save_undo(undo_label, v) < 0)
            return;
        rc = save_undo_data_remove(0, first-1, 1);
        close_undo();
        /* cancel ignored */
    }

    begin_operation("Deleting audio data") ;
    rc = soundfile_remove_samples(0, first-1, 1);
    end_operation();

    if (rc != 0) {
        /* failure; FIX: needs cleanup of undo and more */
        return;
    }

    adjust_view(v);

    v->first_sample = 0;
    v->last_sample  = v->n_samples - 1;
    v->selection_region = FALSE;

    resample(0, prefs.n_samples-1);
}

#endif /* !TRUNCATE_OLD */
