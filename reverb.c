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
	strcpy(reverb_method_name, g_key_file_get_string(key_file, "reverb_params", "reverb_method_name", NULL));
        wet_level = g_key_file_get_double(key_file, "reverb_params", "wet_level", NULL);
        dry_level = g_key_file_get_double(key_file, "reverb_params", "dry_level", NULL);
        decay = g_key_file_get_double(key_file, "reverb_params", "decay", NULL);
    } else {
        // Not sure why this is set here rather than declared above like the other three preferences...
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


void reverb_audio(struct sound_prefs *p, long first, long last, int channel_mask)
{
    float left[BUFSIZE], right[BUFSIZE] ;
    reverb_audio_sample_t left_out[BUFSIZE], right_out[BUFSIZE] ;
    long current, i ;
    int loops = 0 ;

    current = first ;

    push_status_text("TAP Reverb audio") ;
    update_progress_bar(0.0,PROGRESS_UPDATE_INTERVAL,TRUE) ;

    reverb_setup(p->rate, decay, wet_level, dry_level, reverb_method_name) ;


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
}

/* If we come here, then the user has selected a row in the list. */
void reverb_selection_made( GtkWidget      *clist,
                     gint            row,
                     gint            column,
                     GdkEventButton *event,
                     gpointer        data )
{
    gchar *text;

    /* Get the text that is stored in the selected row and column
     * which was clicked in. We will receive it as a pointer in the
     * argument text.
     */
    gtk_clist_get_text(GTK_CLIST(clist), row, column, &text);

    strcpy(reverb_method_name, text) ;

    return;
}



int reverb_dialog(struct sound_prefs current, struct view *v)
{
    GtkWidget *dlg, *dialog_table ;
    GtkWidget *wet_entry ;
    GtkWidget *dry_entry ;
    GtkWidget *decay_entry ;
    GtkWidget *reverb_method_window_list ;
    GtkWidget *scrolled_window ;

    gchar *reverb_method_window_titles[] = { "TAP Reverb Name" };

    int row = 0 ;
    int dres ;

    dialog_table = gtk_table_new(5,2,0) ;

    gtk_table_set_row_spacings(GTK_TABLE(dialog_table), 4) ;
    gtk_table_set_col_spacings(GTK_TABLE(dialog_table), 6) ;
    gtk_widget_show (dialog_table);

    dlg = gtk_dialog_new_with_buttons("Reverb",
			GTK_WINDOW(main_window), GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			 GTK_STOCK_OK, GTK_RESPONSE_OK, NULL, NULL);
    gtk_dialog_set_default_response (GTK_DIALOG(dlg), GTK_RESPONSE_OK);

    row++ ;

    load_reverb_preferences() ;

    wet_entry = add_number_entry_with_label_double(wet_level, "Wet (Db) -30 to 3", dialog_table, row++) ;
    dry_entry = add_number_entry_with_label_double(dry_level, "Dry (Db) -30 to 3", dialog_table, row++) ;
    decay_entry = add_number_entry_with_label_double(decay, "(ms) 0 to 2500", dialog_table, row++) ;

    reverb_method_window_list =
        gtk_clist_new_with_titles(1, reverb_method_window_titles);
    gtk_clist_set_selection_mode(GTK_CLIST(reverb_method_window_list),
                                 GTK_SELECTION_SINGLE);

    gtk_signal_connect(GTK_OBJECT(reverb_method_window_list), "select_row",
		       GTK_SIGNAL_FUNC(reverb_selection_made),
		      NULL);

    {
	REVTYPE *revitem = get_revroot() ;

	gchar * row_text[1] ;
	gchar buf[256] ;
	row_text[0] = buf ;

	while( (revitem = get_next_revtype(revitem)) != NULL) {
	    int i, new_row ;

	    for(i = 0 ; revitem->name[i] != '\0' ; i++)
		buf[i] = (gchar) revitem->name[i] ;

	    buf[i] = '\0' ;

	    new_row = gtk_clist_append(GTK_CLIST(reverb_method_window_list), row_text);

	    if(!strcmp(reverb_method_name, revitem->name)) gtk_clist_select_row(GTK_CLIST(reverb_method_window_list), new_row, 0) ;
	}
    }

    gtk_widget_show(reverb_method_window_list);
    
    /* Create a scrolled window to pack the CList widget into */
    scrolled_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
				    GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);

    gtk_widget_show (scrolled_window);

    gtk_container_add(GTK_CONTAINER(scrolled_window), reverb_method_window_list);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox),
		       scrolled_window, TRUE, TRUE, 0);

    gtk_box_pack_start (GTK_BOX (GTK_DIALOG(dlg)->vbox), dialog_table, TRUE, TRUE, 0);

    dres = gwc_dialog_run(GTK_DIALOG(dlg)) ;

    if(dres == 0) {
	wet_level = atoi(gtk_entry_get_text((GtkEntry *)wet_entry)) ;
	dry_level = atoi(gtk_entry_get_text((GtkEntry *)dry_entry)) ;
	decay = atoi(gtk_entry_get_text((GtkEntry *)decay_entry)) ;
	save_reverb_preferences() ;
    }

    gtk_widget_destroy(dlg) ;

    if(dres == 0)
	return 1 ;

    return 0 ;
}

