/*****************************************************************************
*   Gnome Wave Cleaner Version 0.19
*   Copyright (C) 2001 Jeffrey J. Welty
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

/* reverb.c */
#include <stdlib.h>
#include <glib.h>
#include <string.h>
#include "gwc.h"

#include "tap_reverb_common.h"
#include "tap_reverb.h"
#include "tap_reverb_file_io.h"

static char reverb_method_name[128] ;

#define BUFSIZE 10000

//looks like confusion in the previous source.  Should this default to -40.1?
static gfloat wet_level = -10 ;
//looks like confusion in the previous source.  Should this default to -0.5?
static gfloat dry_level = -1.0 ;
static gfloat decay = 1500 ;

void load_reverb_preferences(void)
{
    GKeyFile  *key_file = read_config();

    // We should probably have a separate test for each preference...
    if (g_key_file_has_group(key_file, "reverb_params") == TRUE) {
        g_strlcpy(reverb_method_name, g_key_file_get_string(key_file, "reverb_params", "reverb_method_name", NULL), 128);
        wet_level = g_key_file_get_double(key_file, "reverb_params", "wet_level", NULL);
        dry_level = g_key_file_get_double(key_file, "reverb_params", "dry_level", NULL);
        decay = g_key_file_get_double(key_file, "reverb_params", "decay", NULL);
    } else {
        // Not sure why this is set here rather than the default being declared above like the other three preferences...
	strcpy(reverb_method_name, "Ambience (Thick) - HD") ;
    }

    g_key_file_free (key_file);
}

void save_reverb_preferences(void)
{
    GKeyFile  *key_file = read_config();

    g_key_file_set_string(key_file, "reverb_params", "reverb_method_name", reverb_method_name);
    g_key_file_set_double(key_file, "reverb_params", "wet_level", wet_level);
    g_key_file_set_double(key_file, "reverb_params", "dry_level", dry_level);
    g_key_file_set_double(key_file, "reverb_params", "decay", decay);

    write_config(key_file);
}


int reverb_audio(struct sound_prefs *p, long first, long last, int channel_mask)
{
    float left[BUFSIZE], right[BUFSIZE] ;
    reverb_audio_sample_t left_out[BUFSIZE], right_out[BUFSIZE] ;
    long current, i ;
    int loops = 0 ;
    current = first ;

    push_status_text("TAP Reverb audio") ;
    update_progress_bar(0.0,PROGRESS_UPDATE_INTERVAL,TRUE) ;

    int rset = reverb_setup(p->rate, decay, wet_level, dry_level, reverb_method_name) ;
    if (rset == 1) {
        return 1 ;
    }

    {

	while(current <= last) {
	    long n = MIN(last - current + 1, BUFSIZE) ;
	    long tmplast = current + n - 1 ;
	    gfloat p = (gfloat)(current-first)/(last-first+1) ;

	    n = read_float_wavefile_data(left, right, current, tmplast) ;

	    /* tap reverb is expecting reverb_audio_sample_t, which is a float */

	    reverb_process(n, left_out, left, right_out, right) ;

	    update_progress_bar(p,PROGRESS_UPDATE_INTERVAL,FALSE) ;

	    if(channel_mask & 0x01) {
		for(i = 0 ; i < n ; i++) left[i] = left_out[i] ;
	    }

	    if(channel_mask & 0x02) {
		for(i = 0 ; i < n ; i++) right[i] = right_out[i] ;
	    }

	    write_float_wavefile_data(left, right, current, tmplast) ;

	    current += n ;

	    if(last - current < 10) loops++ ;

	    if(loops > 5) {
		warning("infinite loop in reverb_audio, programming error\n") ;
	    }
	}

	resample_audio_data(p, first, last) ;
	save_sample_block_data(p) ;
    }

    update_progress_bar(0.0,PROGRESS_UPDATE_INTERVAL,TRUE) ;
    pop_status_text() ;

    main_redraw(FALSE, TRUE) ;
    return 0 ;
}


static void preset_combo_changed(GtkComboBox *combo, gpointer user_data)
{
    const gchar *name = gtk_combo_box_get_active_text(combo);
    if (name) {
        strncpy(reverb_method_name, name, 127);
        reverb_method_name[127] = '\0';   // ensure termination
    }
}


int reverb_dialog(struct sound_prefs current, struct view *v)
{
    GtkWidget *dlg, *dialog_table ;
    GtkWidget *wet_entry ;
    GtkWidget *dry_entry ;
    GtkWidget *decay_entry ;
    GtkWidget *preset_combo, *preset_label ;


    int dclose = 0 ;
    int row = 0 ;
    int dres ;
    char buf[200] ;


    dialog_table = gtk_table_new(5,2,0) ;

    gtk_table_set_row_spacings(GTK_TABLE(dialog_table), 4) ;
    gtk_table_set_col_spacings(GTK_TABLE(dialog_table), 6) ;

    dlg = gtk_dialog_new_with_buttons("Reverb",
			GTK_WINDOW(main_window), GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OK, GTK_RESPONSE_OK, NULL, NULL);
    gtk_dialog_set_default_response (GTK_DIALOG(dlg), GTK_RESPONSE_OK);

    row++ ;

    load_reverb_preferences() ;

    preset_combo = gtk_combo_box_text_new();
    /* Populate dropdown using TAP preset list */
    {
        REVTYPE *revitem = get_revroot();
        while ((revitem = get_next_revtype(revitem)) != NULL) {
            gtk_combo_box_text_append_text(
                GTK_COMBO_BOX_TEXT(preset_combo),
                revitem->name
            );
        }
    }

    /* Set current selection based on saved preference (or first item) */
    {
        int idx = 0;
        int match_index = -1;
        REVTYPE *revitem = get_revroot();
        while ((revitem = get_next_revtype(revitem)) != NULL) {
            if (!strcmp(reverb_method_name, revitem->name)) {
                match_index = idx;
                break;
            }
            idx++;
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(preset_combo),
                                 (match_index >= 0) ? match_index : 0);
    }

    /* Attach combo to column 1 of the same row */
    gtk_table_attach(GTK_TABLE(dialog_table), preset_combo,
                     0, 1, row, row+1,
                     GTK_EXPAND | GTK_FILL, GTK_FILL, 2, 2);
    preset_label = gtk_label_new("TAP Reverb Preset");
    /* left-align label text within its cell */
    gtk_misc_set_alignment(GTK_MISC(preset_label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(dialog_table), preset_label,
                     1, 2, row, row+1,
                     GTK_FILL, GTK_FILL, 2, 2);
    row++;


    /* Capture selection change */
	g_signal_connect(preset_combo, "changed",
					 G_CALLBACK(preset_combo_changed),
					 NULL);

    wet_entry = add_number_entry_with_label_double(wet_level, "Wet Level (Db) -30 to 3", dialog_table, row++) ;
    dry_entry = add_number_entry_with_label_double(dry_level, "Dry Level (Db) -30 to 3", dialog_table, row++) ;
    decay_entry = add_number_entry_with_label_double(decay, "Decay (ms) 0 to 2500", dialog_table, row++) ;

    gtk_box_pack_start (GTK_BOX (GTK_DIALOG(dlg)->vbox), dialog_table, TRUE, TRUE, 0);
    /* Ensure all widgets are visible before running the dialog */
    gtk_widget_show_all(dlg);


    dres = gwc_dialog_run(GTK_DIALOG(dlg)) ;

    if(dres == 0) {
	int i ;
	wet_level = atoi(gtk_entry_get_text((GtkEntry *)wet_entry)) ;
	dry_level = atoi(gtk_entry_get_text((GtkEntry *)dry_entry)) ;
	decay = atoi(gtk_entry_get_text((GtkEntry *)decay_entry)) ;
	save_reverb_preferences() ;
	dclose = 1 ;
    }

    gtk_widget_destroy(dlg) ;

    if(dres == 0)
	return 1 ;

    return 0 ;
}

