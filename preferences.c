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


/* preferences.c */
/* preference settings for GWC */

#include <gnome.h>
#include <glib.h>
#include <string.h>
#include "gwc.h"
#include "encoding.h"

extern struct encoding_prefs encoding_prefs;
static int svbr_mode, encpresets, oggencopt;

int gwc_dialog_run(GtkDialog *dlg)
{
    int dres ;

    dres = gtk_dialog_run(GTK_DIALOG(dlg));

    if (dres == GTK_RESPONSE_CANCEL)
	return 1 ;

    return 0 ;
}

void vbr_mode_window_select(GtkWidget * clist, gint row, gint column,
			    GdkEventButton * event, gpointer data)
{
    svbr_mode = row;
}

void presets_window_select(GtkWidget * clist, gint row, gint column,
			   GdkEventButton * event, gpointer data)
{
    encpresets = row;
}

void ogg_enc_window_select(GtkWidget * clist, gint row, gint column,
			   GdkEventButton * event, gpointer data)
{
    oggencopt = row;
}

void set_ogg_encoding_preferences(GtkWidget * widget, gpointer data)
{
    GtkWidget *dlg;
    GtkWidget *dialog_table;
    GtkWidget *oggquality_entry;
    GtkWidget *oggloc_entry;
    GtkWidget *oggloclabel_entry;
    GtkWidget *oggmaxbitrate_entry;
    GtkWidget *oggminbitrate_entry;
    GtkWidget *oggbitrate_entry;
    GtkWidget *useAdvBitrateAvgWindow_entry;
    GtkWidget *useAdvlowpass_entry;
    GtkWidget *useResample_entry;
    GtkWidget *downmix_entry;
    GtkWidget *AdvBitrateAvgWindow_entry;
    GtkWidget *Advlowpass_entry;
    GtkWidget *Resample_entry;
    GtkWidget *enc_opt_window_list;
    GtkWidget *oggoptlabel_entry;

    int dres;
    int row = 0;

    gchar *enc_opt_window_titles[] = { "Ogg Encoding Mode" };
    gchar *enc_opt_window_parms[4][1] = { {"Default"},
    {"Managed"},
    {"Nominal Bitrate"},
    {"Quality Level"}
    };

    load_ogg_encoding_preferences();

    dlg =
	gtk_dialog_new_with_buttons("Ogg Encoding preferences",
			NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_OK, GTK_RESPONSE_OK,
			 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, NULL, NULL);
    dialog_table = gtk_table_new(14, 3, 0);
    gtk_table_set_row_spacings(GTK_TABLE(dialog_table), 4);
    gtk_table_set_col_spacings(GTK_TABLE(dialog_table), 6);

    gtk_widget_show(dialog_table);

    enc_opt_window_list =
	gtk_clist_new_with_titles(1, enc_opt_window_titles);
    gtk_clist_set_selection_mode(GTK_CLIST(enc_opt_window_list),
				 GTK_SELECTION_SINGLE);
    gtk_clist_append(GTK_CLIST(enc_opt_window_list),
		     enc_opt_window_parms[0]);
    gtk_clist_append(GTK_CLIST(enc_opt_window_list),
		     enc_opt_window_parms[1]);
    gtk_clist_append(GTK_CLIST(enc_opt_window_list),
		     enc_opt_window_parms[2]);
    gtk_clist_append(GTK_CLIST(enc_opt_window_list),
		     enc_opt_window_parms[3]);

    gtk_clist_select_row(GTK_CLIST(enc_opt_window_list),
			 encoding_prefs.ogg_encopt, 0);

    gtk_signal_connect(GTK_OBJECT(enc_opt_window_list), "select_row",
		       GTK_SIGNAL_FUNC(ogg_enc_window_select), NULL);

    oggencopt = encoding_prefs.ogg_encopt;

    oggloc_entry = gtk_entry_new_with_max_length(255);
    oggloclabel_entry = gtk_label_new("Oggenc Location (full path):");

    oggoptlabel_entry = gtk_label_new("Enable Options");

    /* set the text */
    if ((encoding_prefs.oggloc != NULL)
	&& (strlen(encoding_prefs.oggloc) > 0)) {
	gtk_entry_set_text(GTK_ENTRY(oggloc_entry), encoding_prefs.oggloc);
    }

    downmix_entry = gtk_check_button_new_with_label("Downmix");

    if (encoding_prefs.ogg_downmix == 1) {
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(downmix_entry),
				     TRUE);
    }

    useResample_entry = gtk_check_button_new_with_label("Resample");

    if (encoding_prefs.ogg_useresample == 1) {
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(useResample_entry),
				     TRUE);
    }

    useAdvlowpass_entry = gtk_check_button_new_with_label("Adv Low Pass");

    if (encoding_prefs.ogg_useadvlowpass == 1) {
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
				     (useAdvlowpass_entry), TRUE);
    }

    useAdvBitrateAvgWindow_entry =
	gtk_check_button_new_with_label("Adv Bitrate Avg Window");

    if (encoding_prefs.ogg_useadvbravgwindow == 1) {
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
				     (useAdvBitrateAvgWindow_entry), TRUE);
    }

    oggquality_entry =
	add_number_entry_with_label_double(atof
					   (encoding_prefs.
					    ogg_quality_level),
					   "OGG Vorbis Quality Value (0.0-10.0)",
					   dialog_table, row++);
    gtk_entry_set_max_length(GTK_ENTRY(oggquality_entry), 5);	/* 5 digits */
    oggbitrate_entry =
	add_number_entry_with_label_int(atoi(encoding_prefs.ogg_bitrate),
					"Nominal Bitrate (kb/s)",
					dialog_table, row++);
    gtk_entry_set_max_length(GTK_ENTRY(oggbitrate_entry), 5);	/* 5 digits */
    oggminbitrate_entry =
	add_number_entry_with_label_int(atoi
					(encoding_prefs.ogg_minbitrate),
					"Managed Min Bitrate (kb/s)",
					dialog_table, row++);
    gtk_entry_set_max_length(GTK_ENTRY(oggminbitrate_entry), 5);	/* 5 digits */
    oggmaxbitrate_entry =
	add_number_entry_with_label_int(atoi
					(encoding_prefs.ogg_maxbitrate),
					"Managed  Max Bitrate (kb/s)",
					dialog_table, row++);
    gtk_entry_set_max_length(GTK_ENTRY(oggmaxbitrate_entry), 5);	/* 5 digits */
    Resample_entry =
	add_number_entry_with_label_int(atoi(encoding_prefs.ogg_resample),
					"Resample Rate (Hz)", dialog_table,
					row++);
    gtk_entry_set_max_length(GTK_ENTRY(Resample_entry), 5);	/* 5 digits */
    Advlowpass_entry =
	add_number_entry_with_label_int(atoi
					(encoding_prefs.
					 ogg_lowpass_frequency),
					"Adv Low Pass (kHz)", dialog_table,
					row++);
    gtk_entry_set_max_length(GTK_ENTRY(Advlowpass_entry), 5);	/* 5 digits */
    AdvBitrateAvgWindow_entry =
	add_number_entry_with_label_int(atoi
					(encoding_prefs.
					 ogg_bitrate_average_window),
					"Adv Bitrate Avg Window (s)",
					dialog_table, row++);
    gtk_entry_set_max_length(GTK_ENTRY(AdvBitrateAvgWindow_entry), 5);	/* 5 digits */

    gtk_widget_show(oggloclabel_entry);
    gtk_widget_show(oggloc_entry);
    gtk_widget_show(oggquality_entry);
    gtk_widget_show(oggoptlabel_entry);
    gtk_widget_show(downmix_entry);
    gtk_widget_show(Resample_entry);
    gtk_widget_show(useResample_entry);
    gtk_widget_show(Advlowpass_entry);
    gtk_widget_show(useAdvlowpass_entry);
    gtk_widget_show(AdvBitrateAvgWindow_entry);
    gtk_widget_show(useAdvBitrateAvgWindow_entry);
    gtk_widget_show(enc_opt_window_list);

    gtk_table_attach_defaults(GTK_TABLE(dialog_table), oggloclabel_entry,
			      0, 1, row, row + 1);
    row++;
    gtk_table_attach_defaults(GTK_TABLE(dialog_table), oggloc_entry, 0, 1,
			      row, row + 1);
    row++;
    gtk_table_attach_defaults(GTK_TABLE(dialog_table), oggoptlabel_entry,
			      0, 1, row, row + 1);
    row++;
    gtk_table_attach_defaults(GTK_TABLE(dialog_table), downmix_entry, 0, 1,
			      row, row + 1);
    row++;

    gtk_table_attach_defaults(GTK_TABLE(dialog_table), useResample_entry,
			      0, 1, row, row + 1);
    row++;
    gtk_table_attach_defaults(GTK_TABLE(dialog_table), useAdvlowpass_entry,
			      0, 1, row, row + 1);
    row++;
    gtk_table_attach_defaults(GTK_TABLE(dialog_table),
			      useAdvBitrateAvgWindow_entry, 0, 1, row,
			      row + 1);
    row++;

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox), dialog_table,
		       TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox),
		       enc_opt_window_list, TRUE, TRUE, 0);

    dres = gwc_dialog_run(GTK_DIALOG(dlg));

    if (dres == 0) {

	encoding_prefs.ogg_encopt = oggencopt;
	strcpy(encoding_prefs.ogg_quality_level,
	       gtk_entry_get_text((GtkEntry *) oggquality_entry));
	strcpy(encoding_prefs.ogg_minbitrate,
	       gtk_entry_get_text((GtkEntry *) oggminbitrate_entry));
	strcpy(encoding_prefs.ogg_maxbitrate,
	       gtk_entry_get_text((GtkEntry *) oggmaxbitrate_entry));
	strcpy(encoding_prefs.ogg_bitrate,
	       gtk_entry_get_text((GtkEntry *) oggbitrate_entry));
	encoding_prefs.ogg_downmix =
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON
					 (downmix_entry));
	strcpy(encoding_prefs.ogg_resample,
	       gtk_entry_get_text((GtkEntry *) Resample_entry));
	strcpy(encoding_prefs.ogg_lowpass_frequency,
	       gtk_entry_get_text((GtkEntry *) Advlowpass_entry));
	strcpy(encoding_prefs.ogg_bitrate_average_window,
	       gtk_entry_get_text((GtkEntry *)
				  AdvBitrateAvgWindow_entry));
	encoding_prefs.ogg_useresample =
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON
					 (useResample_entry));
	encoding_prefs.ogg_useadvlowpass =
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON
					 (useAdvlowpass_entry));
	encoding_prefs.ogg_useadvbravgwindow =
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON
					 (useAdvBitrateAvgWindow_entry));
	strcpy(encoding_prefs.oggloc,
	       gtk_entry_get_text(GTK_ENTRY(oggloc_entry)));

	main_redraw(FALSE, TRUE);
	save_ogg_encoding_preferences();

    }

    gtk_widget_destroy(dlg);
}

void set_mp3_simple_encoding_preferences(GtkWidget * widget, gpointer data)
{
    /* new encoding preferences settings GTK window code */
    GtkWidget *dlg;
    GtkWidget *dialog_table;
    GtkWidget *quality_entry;
    GtkWidget *mp3loc_entry;
    GtkWidget *mp3loclabel_entry;
    GtkWidget *artist_entry;
    GtkWidget *artistlabel_entry;
    GtkWidget *album_entry;
    GtkWidget *albumlabel_entry;

    int dres;
    int row = 0;

    load_mp3_simple_encoding_preferences();

    mp3loc_entry = gtk_entry_new();

    encpresets = encoding_prefs.mp3presets;
    dlg =
	gtk_dialog_new_with_buttons("MP3 Simple Encoding preferences",
			NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_OK, GTK_RESPONSE_OK,
			 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, NULL, NULL);

    dialog_table = gtk_table_new(15, 3, 0);


    gtk_table_set_row_spacings(GTK_TABLE(dialog_table), 4);
    gtk_table_set_col_spacings(GTK_TABLE(dialog_table), 6);

    mp3loc_entry = gtk_entry_new_with_max_length(255);
    mp3loclabel_entry = gtk_label_new("Lame Location (full path):");

/* set the text */
    if ((encoding_prefs.mp3loc != NULL)
	&& (strlen(encoding_prefs.mp3loc) > 0)) {
	gtk_entry_set_text(GTK_ENTRY(mp3loc_entry), encoding_prefs.mp3loc);
    }


    artist_entry = gtk_entry_new_with_max_length(255);
    artistlabel_entry = gtk_label_new("Artist:");

/* set the text */
    if ((encoding_prefs.artist != NULL)
	&& (strlen(encoding_prefs.artist) > 0)) {
	gtk_entry_set_text(GTK_ENTRY(artist_entry), encoding_prefs.artist);
    }

    album_entry = gtk_entry_new_with_max_length(255);
    albumlabel_entry = gtk_label_new("Album:");

/* set the text */
    if ((encoding_prefs.album != NULL)
	&& (strlen(encoding_prefs.album) > 0)) {
	gtk_entry_set_text(GTK_ENTRY(album_entry), encoding_prefs.album);
    }


    quality_entry =
	add_number_entry_with_label_int(atoi
					(encoding_prefs.mp3_quality_level),
					"MP3 VBR Quality Value (0-9) (i.e. -V ...)",
					dialog_table, row++);
    gtk_entry_set_max_length(GTK_ENTRY(quality_entry), 1);	/* 1 digit */

    gtk_widget_show(dialog_table);
    gtk_widget_show(mp3loclabel_entry);
    gtk_widget_show(mp3loc_entry);
    gtk_widget_show(artistlabel_entry);
    gtk_widget_show(artist_entry);
    gtk_widget_show(albumlabel_entry);
    gtk_widget_show(album_entry);

    gtk_table_attach_defaults(GTK_TABLE(dialog_table), mp3loclabel_entry,
			      0, 1, row, row + 1);
    row++;
    gtk_table_attach_defaults(GTK_TABLE(dialog_table), mp3loc_entry, 0, 1,
			      row, row + 1);
    row++;

    gtk_table_attach_defaults(GTK_TABLE(dialog_table), artistlabel_entry,
			      0, 1, row, row + 1);
    row++;
    gtk_table_attach_defaults(GTK_TABLE(dialog_table), artist_entry, 0, 1,
			      row, row + 1);
    row++;

    gtk_table_attach_defaults(GTK_TABLE(dialog_table), albumlabel_entry,
			      0, 1, row, row + 1);
    row++;
    gtk_table_attach_defaults(GTK_TABLE(dialog_table), album_entry, 0, 1,
			      row, row + 1);
    row++;

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox), dialog_table,
		       TRUE, TRUE, 0);

    dres = gwc_dialog_run(GTK_DIALOG(dlg));

    if (dres == 0) {
	/* save setting changes */

	strcpy(encoding_prefs.mp3_quality_level,
	       gtk_entry_get_text(GTK_ENTRY(quality_entry)));
	strcpy(encoding_prefs.mp3loc,
	       gtk_entry_get_text(GTK_ENTRY(mp3loc_entry)));
	strcpy(encoding_prefs.artist,
	       gtk_entry_get_text(GTK_ENTRY(artist_entry)));
	strcpy(encoding_prefs.album,
	       gtk_entry_get_text(GTK_ENTRY(album_entry)));

	main_redraw(FALSE, TRUE);
	save_mp3_simple_encoding_preferences();
    }

    gtk_widget_destroy(dlg) ;
}

void set_mp3_encoding_preferences(GtkWidget * widget, gpointer data)
{
    /* new encoding preferences settings GTK window code */
    GtkWidget *dlg;
    GtkWidget *dialog_table;
    GtkWidget *quality_entry;
    GtkWidget *bitrate_entry;
    GtkWidget *vbr_mode_window_list;
    GtkWidget *presets_window_list;
    GtkWidget *lame_mmx_enabled_entry;
    GtkWidget *mmx_entry;
    GtkWidget *sse_entry;
    GtkWidget *threednow_entry;
    GtkWidget *asmlabel_entry;
    GtkWidget *mp3loc_entry;
    GtkWidget *mp3loclabel_entry;
    GtkWidget *allfilter_entry;
    GtkWidget *strictiso_entry;
    GtkWidget *copyrighted_entry;
    GtkWidget *protected_entry;
    GtkWidget *uselowpass_entry;
    GtkWidget *usehighpass_entry;
    GtkWidget *highpassfreq_entry;
    GtkWidget *lowpassfreq_entry;
    GtkWidget *otheropt_entry;

    gchar *vbr_mode_window_titles[] = { "MP3 Bitrate Mode" };
    gchar *vbr_mode_window_parms[4][1] = { {"Default"},
    {"Average Bit Rate"},
    {"Constant Bit Rate"},
    {"Variable Bit Rate"}
    };

    gchar *presets_window_titles[] = { "MP3 Presets" };
    gchar *presets_window_parms[9][1] = { {"UNSELECTED"},
    {"R3MIX"},
    {"STANDARD"},
    {"MEDIUM"},
    {"EXTREME"},
    {"INSANE"},
    {"FAST STANDARD"},
    {"FAST MEDIUM"},
    {"FAST EXTREME"}
    };


    int dres;
    int row = 0;

    load_mp3_encoding_preferences();

    lame_mmx_enabled_entry =
	gtk_check_button_new_with_label("Lame MMX enabled?");

    asmlabel_entry = gtk_label_new("Use MP3 Assembly Optimizations:");

    mmx_entry = gtk_check_button_new_with_label("MMX");

    sse_entry = gtk_check_button_new_with_label("SSE");

    threednow_entry = gtk_check_button_new_with_label("3DNOW");

    otheropt_entry = gtk_label_new("Advanced Options");
    allfilter_entry = gtk_check_button_new_with_label("No Filters");
    strictiso_entry =
	gtk_check_button_new_with_label("Enforce Strict ISO");
    copyrighted_entry =
	gtk_check_button_new_with_label("Mark Copyrighted");
    protected_entry = gtk_check_button_new_with_label("Add CRC");
    uselowpass_entry =
	gtk_check_button_new_with_label("Use Lowpass Filter");
    usehighpass_entry =
	gtk_check_button_new_with_label("Use Highpass Filter");


    mp3loc_entry = gtk_entry_new();

    vbr_mode_window_list =
	gtk_clist_new_with_titles(1, vbr_mode_window_titles);
    gtk_clist_set_selection_mode(GTK_CLIST(vbr_mode_window_list),
				 GTK_SELECTION_SINGLE);
    gtk_clist_append(GTK_CLIST(vbr_mode_window_list),
		     vbr_mode_window_parms[0]);
    gtk_clist_append(GTK_CLIST(vbr_mode_window_list),
		     vbr_mode_window_parms[1]);
    gtk_clist_append(GTK_CLIST(vbr_mode_window_list),
		     vbr_mode_window_parms[2]);
    gtk_clist_append(GTK_CLIST(vbr_mode_window_list),
		     vbr_mode_window_parms[3]);

    presets_window_list =
	gtk_clist_new_with_titles(1, presets_window_titles);
    gtk_clist_set_selection_mode(GTK_CLIST(presets_window_list),
				 GTK_SELECTION_SINGLE);
    gtk_clist_append(GTK_CLIST(presets_window_list),
		     presets_window_parms[0]);
    gtk_clist_append(GTK_CLIST(presets_window_list),
		     presets_window_parms[1]);
    gtk_clist_append(GTK_CLIST(presets_window_list),
		     presets_window_parms[2]);
    gtk_clist_append(GTK_CLIST(presets_window_list),
		     presets_window_parms[3]);
    gtk_clist_append(GTK_CLIST(presets_window_list),
		     presets_window_parms[4]);
    gtk_clist_append(GTK_CLIST(presets_window_list),
		     presets_window_parms[5]);
    gtk_clist_append(GTK_CLIST(presets_window_list),
		     presets_window_parms[6]);
    gtk_clist_append(GTK_CLIST(presets_window_list),
		     presets_window_parms[7]);
    gtk_clist_append(GTK_CLIST(presets_window_list),
		     presets_window_parms[8]);


    gtk_clist_select_row(GTK_CLIST(vbr_mode_window_list),
			 encoding_prefs.mp3_br_mode, 0);

    gtk_clist_select_row(GTK_CLIST(presets_window_list),
			 encoding_prefs.mp3presets, 0);

    if (encoding_prefs.mp3_lame_mmx_enabled == 1)
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
				     (lame_mmx_enabled_entry), TRUE);

    if (encoding_prefs.mp3_mmx == 1)
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mmx_entry), TRUE);

    if (encoding_prefs.mp3_sse == 1)
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sse_entry), TRUE);

    if (encoding_prefs.mp3_threednow == 1)
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(threednow_entry),
				     TRUE);

    if (encoding_prefs.mp3_copyrighted == 1)
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(copyrighted_entry),
				     TRUE);

    if (encoding_prefs.mp3_add_crc == 1)
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(protected_entry),
				     TRUE);

    if (encoding_prefs.mp3_strict_iso == 1)
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(strictiso_entry),
				     TRUE);

    if (encoding_prefs.mp3_nofilters == 1)
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(allfilter_entry),
				     TRUE);

    if (encoding_prefs.mp3_use_lowpass == 1)
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(uselowpass_entry),
				     TRUE);

    if (encoding_prefs.mp3_use_highpass == 1)
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(usehighpass_entry),
				     TRUE);

    gtk_signal_connect(GTK_OBJECT(vbr_mode_window_list), "select_row",
		       GTK_SIGNAL_FUNC(vbr_mode_window_select), NULL);
    gtk_signal_connect(GTK_OBJECT(presets_window_list), "select_row",
		       GTK_SIGNAL_FUNC(presets_window_select), NULL);

    encpresets = encoding_prefs.mp3presets;
    svbr_mode = encoding_prefs.mp3_br_mode;
    gtk_widget_show(vbr_mode_window_list);
    gtk_widget_show(presets_window_list);
    dlg =
	gtk_dialog_new_with_buttons("MP3 Encoding preferences",
			NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_OK, GTK_RESPONSE_OK,
			 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, NULL, NULL);

    dialog_table = gtk_table_new(15, 3, 0);


    gtk_table_set_row_spacings(GTK_TABLE(dialog_table), 4);
    gtk_table_set_col_spacings(GTK_TABLE(dialog_table), 6);


    mp3loc_entry = gtk_entry_new_with_max_length(255);
    mp3loclabel_entry = gtk_label_new("Lame Location (full path):");

/* set the text */
    if ((encoding_prefs.mp3loc != NULL)
	&& (strlen(encoding_prefs.mp3loc) > 0)) {
	gtk_entry_set_text(GTK_ENTRY(mp3loc_entry), encoding_prefs.mp3loc);
    }

    bitrate_entry =
	add_number_entry_with_label_int(atoi(encoding_prefs.mp3_bitrate),
					"MP3 Encoding Bitrate (kbps)",
					dialog_table, row++);
    gtk_entry_set_max_length(GTK_ENTRY(bitrate_entry), 5);	/* 5 digits */
    quality_entry =
	add_number_entry_with_label_int(atoi
					(encoding_prefs.mp3_quality_level),
					"MP3 Quality Value (0-9)",
					dialog_table, row++);
    gtk_entry_set_max_length(GTK_ENTRY(quality_entry), 1);	/* 1 digit */

    lowpassfreq_entry =
	add_number_entry_with_label(encoding_prefs.mp3_lowpass_freq,
				    "Lowpass Filter (kHz)", dialog_table,
				    row++);
    gtk_entry_set_max_length(GTK_ENTRY(lowpassfreq_entry), 5);	/* 5 digits */

    highpassfreq_entry =
	add_number_entry_with_label(encoding_prefs.mp3_highpass_freq,
				    "Highpass Filter (kHz)", dialog_table,
				    row++);
    gtk_entry_set_max_length(GTK_ENTRY(highpassfreq_entry), 5);	/* 5 digits */


    gtk_widget_show(dialog_table);
    gtk_widget_show(mp3loclabel_entry);
    gtk_widget_show(mp3loc_entry);
    gtk_widget_show(lame_mmx_enabled_entry);
    gtk_widget_show(asmlabel_entry);
    gtk_widget_show(sse_entry);
    gtk_widget_show(mmx_entry);
    gtk_widget_show(threednow_entry);
    gtk_widget_show(otheropt_entry);
    gtk_widget_show(allfilter_entry);
    gtk_widget_show(uselowpass_entry);
    gtk_widget_show(usehighpass_entry);
    gtk_widget_show(protected_entry);
    gtk_widget_show(copyrighted_entry);
    gtk_widget_show(strictiso_entry);
    gtk_widget_show(bitrate_entry);
    gtk_widget_show(lowpassfreq_entry);
    gtk_widget_show(highpassfreq_entry);

    gtk_table_attach_defaults(GTK_TABLE(dialog_table), mp3loclabel_entry,
			      0, 1, row, row + 1);
    row++;
    gtk_table_attach_defaults(GTK_TABLE(dialog_table), mp3loc_entry, 0, 1,
			      row, row + 1);
    row++;
    gtk_table_attach_defaults(GTK_TABLE(dialog_table),
			      lame_mmx_enabled_entry, 0, 1, row, row + 1);
    row++;
    gtk_table_attach_defaults(GTK_TABLE(dialog_table), asmlabel_entry, 0,
			      1, row, row + 1);
    row++;
    gtk_table_attach_defaults(GTK_TABLE(dialog_table), mmx_entry, 0, 1,
			      row, row + 1);
    gtk_table_attach_defaults(GTK_TABLE(dialog_table), sse_entry, 1, 2,
			      row, row + 1);
    gtk_table_attach_defaults(GTK_TABLE(dialog_table), threednow_entry, 2,
			      3, row, row + 1);
    row++;
    gtk_table_attach_defaults(GTK_TABLE(dialog_table), otheropt_entry, 0,
			      1, row, row + 1);
    row++;
    gtk_table_attach_defaults(GTK_TABLE(dialog_table), allfilter_entry, 0,
			      1, row, row + 1);
    gtk_table_attach_defaults(GTK_TABLE(dialog_table), uselowpass_entry, 1,
			      2, row, row + 1);
    gtk_table_attach_defaults(GTK_TABLE(dialog_table), usehighpass_entry,
			      2, 3, row, row + 1);
    row++;
    gtk_table_attach_defaults(GTK_TABLE(dialog_table), copyrighted_entry,
			      0, 1, row, row + 1);
    gtk_table_attach_defaults(GTK_TABLE(dialog_table), protected_entry, 1,
			      2, row, row + 1);
    gtk_table_attach_defaults(GTK_TABLE(dialog_table), strictiso_entry, 2,
			      3, row, row + 1);
    row++;
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox), dialog_table,
		       TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox),
		       presets_window_list, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox),
		       vbr_mode_window_list, TRUE, TRUE, 0);

    dres = gwc_dialog_run(GTK_DIALOG(dlg));
    if (dres == 0) {
	/* save setting changes */

	encoding_prefs.mp3_br_mode = svbr_mode;
	encoding_prefs.mp3presets = encpresets;
	strcpy(encoding_prefs.mp3_bitrate,
	       gtk_entry_get_text(GTK_ENTRY(bitrate_entry)));
	strcpy(encoding_prefs.mp3_quality_level,
	       gtk_entry_get_text(GTK_ENTRY(quality_entry)));
	encoding_prefs.mp3_lame_mmx_enabled =
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON
					 (lame_mmx_enabled_entry));
	encoding_prefs.mp3_sse =
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(sse_entry));
	encoding_prefs.mp3_mmx =
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mmx_entry));
	encoding_prefs.mp3_threednow =
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON
					 (threednow_entry));
	encoding_prefs.mp3_copyrighted =
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON
					 (copyrighted_entry));
	encoding_prefs.mp3_add_crc =
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON
					 (protected_entry));
	encoding_prefs.mp3_strict_iso =
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON
					 (strictiso_entry));
	encoding_prefs.mp3_nofilters =
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON
					 (allfilter_entry));
	encoding_prefs.mp3_use_lowpass =
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON
					 (uselowpass_entry));
	encoding_prefs.mp3_use_highpass =
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON
					 (usehighpass_entry));
	strcpy(encoding_prefs.mp3loc,
	       gtk_entry_get_text(GTK_ENTRY(mp3loc_entry)));
	strcpy(encoding_prefs.mp3_lowpass_freq,
	       gtk_entry_get_text(GTK_ENTRY(lowpassfreq_entry)));
	strcpy(encoding_prefs.mp3_highpass_freq,
	       gtk_entry_get_text(GTK_ENTRY(highpassfreq_entry)));
	main_redraw(FALSE, TRUE);
	save_mp3_encoding_preferences();
    }

    gtk_widget_destroy(dlg) ;
}


/*  int preferences_dialog(void)  */
void set_misc_preferences(GtkWidget * widget, gpointer data)
{
    extern double stop_key_highlight_interval;
    extern double song_key_highlight_interval;
    extern double song_mark_silence;
    extern int sonogram_log;
    GtkWidget *dlg;
    GtkWidget *stop_interval_entry;
    GtkWidget *song_interval_entry;
    GtkWidget *dialog_table;
    GtkWidget *normalize_entry;
    GtkWidget *silence_entry;
    GtkWidget *sonogram_log_entry;
    GtkWidget *audio_device_entry;
    extern char audio_device[];
    extern int denoise_normalize;
    int dres;
    int row = 0;

    dlg =
	gtk_dialog_new_with_buttons("Miscellaneous preferences",
			NULL, GTK_DIALOG_DESTROY_WITH_PARENT,

			 GTK_STOCK_OK, GTK_RESPONSE_OK, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			 NULL, NULL);

    dialog_table = gtk_table_new(6, 2, 0);


    gtk_table_set_row_spacings(GTK_TABLE(dialog_table), 5);
    gtk_table_set_col_spacings(GTK_TABLE(dialog_table), 6);
    gtk_widget_show(dialog_table);

    stop_interval_entry =
	add_number_entry_with_label_double(stop_key_highlight_interval,
					   "Seconds of audio pre-selected when \"s\" key is struck",
					   dialog_table, row++);
    song_interval_entry =
	add_number_entry_with_label_double(song_key_highlight_interval,
					   "Seconds of audio highlighted around song marker when markers are \"shown\"",
					   dialog_table, row++);
    normalize_entry =
	add_number_entry_with_label_int(denoise_normalize,
					"Normalize values for declick, denoise?",
					dialog_table, row++);
    silence_entry =
	add_number_entry_with_label_double(song_mark_silence,
					   "Silence estimate in seconds for marking songs",
					   dialog_table, row++);

    sonogram_log_entry =
	gtk_check_button_new_with_label("Log frequency in sonogram");
    if (sonogram_log)
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sonogram_log_entry),
				     TRUE);
    gtk_widget_show(sonogram_log_entry);
    gtk_table_attach_defaults(GTK_TABLE(dialog_table), sonogram_log_entry,
			      0, 1, row, row + 1);
    row++;

    audio_device_entry =
	add_number_entry_with_label(audio_device,
			   "Audio device try (/dev/dsp for OSS) (default, hw:0,0 or hw:1,0 ... for ALSA)", dialog_table, row++);


    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox), dialog_table,
		       TRUE, TRUE, 0);

    dres = gwc_dialog_run(GTK_DIALOG(dlg));

    if (dres == 0) {
	stop_key_highlight_interval =
	    atof(gtk_entry_get_text((GtkEntry *) stop_interval_entry));
	song_key_highlight_interval =
	    atof(gtk_entry_get_text((GtkEntry *) song_interval_entry));
	song_mark_silence =
	    atof(gtk_entry_get_text((GtkEntry *) silence_entry));
	denoise_normalize =
	    atoi(gtk_entry_get_text((GtkEntry *) normalize_entry));
	sonogram_log =
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON
					 (sonogram_log_entry));
	strcpy(audio_device, 
	    gtk_entry_get_text(((GtkEntry *) audio_device_entry)));

	save_preferences();

	main_redraw(FALSE, TRUE);
    }

    gtk_widget_destroy(dlg) ;
}

void declick_set_preferences(GtkWidget * widget, gpointer data)
{
    extern double weak_declick_sensitivity;
    extern double strong_declick_sensitivity;
    extern double weak_fft_declick_sensitivity;
    extern double strong_fft_declick_sensitivity;
    extern int declick_iterate_flag ;
    extern int declick_detector_type ;
    GtkWidget *dlg;
    GtkWidget *dc_weak_entry;
    GtkWidget *dc_strong_entry;
    GtkWidget *iterate_widget;
    GtkWidget *dc_fft_weak_entry;
    GtkWidget *dc_fft_strong_entry;
    GtkWidget *method_widget;
    GtkWidget *dialog_table;
    int dres;
    int row = 1 ;

    dlg =
	gtk_dialog_new_with_buttons("Declicking preferences",
			NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_OK, GTK_RESPONSE_OK,
			 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, NULL, NULL);

    dialog_table = gtk_table_new(5, 2, 0);

    gtk_table_set_row_spacings(GTK_TABLE(dialog_table), 4);
    gtk_table_set_col_spacings(GTK_TABLE(dialog_table), 6);

    gtk_widget_show(dialog_table);

    dc_weak_entry =
	add_number_entry_with_label_double(weak_declick_sensitivity,
					   "Weak Declick Sensitivity (default = 1.0) ",
					   dialog_table, row++);
    dc_strong_entry =
	add_number_entry_with_label_double(strong_declick_sensitivity,
					   "Strong Declick Sensitivity (default = 0.75) ",
					   dialog_table, row++);

    dc_fft_weak_entry =
	add_number_entry_with_label_double(weak_fft_declick_sensitivity,
					   "FFT Weak Declick Sensitivity (default = 3.0) ",
					   dialog_table, row++);
    dc_fft_strong_entry =
	add_number_entry_with_label_double(strong_fft_declick_sensitivity,
					   "FFT Strong Declick Sensitivity (default = 5.0) ",
					   dialog_table, row++);

    method_widget = gtk_check_button_new_with_label ("Use FFT click detector");
    gtk_toggle_button_set_active((GtkToggleButton *) method_widget, declick_detector_type == FFT_DETECT ? TRUE : FALSE);
    gtk_widget_show(method_widget);
    gtk_table_attach_defaults(GTK_TABLE(dialog_table), method_widget, 0, 2, row, row+1);
    row += 2 ;

    iterate_widget = gtk_check_button_new_with_label ("Iterate in repair clicks until all repaired");
    gtk_toggle_button_set_active((GtkToggleButton *) iterate_widget, declick_iterate_flag == 1 ? TRUE : FALSE);
    gtk_widget_show(iterate_widget);
    gtk_table_attach_defaults(GTK_TABLE(dialog_table), iterate_widget, 0, 2, row, row+1);
    row += 2 ;


    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox), dialog_table,
		       TRUE, TRUE, 0);

    dres = gwc_dialog_run(GTK_DIALOG(dlg));

    if (dres == 0) {
	weak_declick_sensitivity = atof(gtk_entry_get_text((GtkEntry *) dc_weak_entry));
	strong_declick_sensitivity = atof(gtk_entry_get_text((GtkEntry *) dc_strong_entry));
	weak_fft_declick_sensitivity = atof(gtk_entry_get_text((GtkEntry *) dc_fft_weak_entry));
	strong_fft_declick_sensitivity = atof(gtk_entry_get_text((GtkEntry *) dc_fft_strong_entry));
	declick_iterate_flag = gtk_toggle_button_get_active((GtkToggleButton *) iterate_widget) ==
	    TRUE ? 1 : 0;
	declick_detector_type = gtk_toggle_button_get_active((GtkToggleButton *) method_widget) ==
	    TRUE ? FFT_DETECT : HPF_DETECT ;
    }

    gtk_widget_destroy(dlg) ;
}

void decrackle_set_preferences(GtkWidget * widget, gpointer data)
{
    extern double decrackle_level;
    extern gint decrackle_window, decrackle_average;
    GtkWidget *dlg;
    GtkWidget *dcr_entry, *dcw_entry, *dca_entry;
    GtkWidget *dialog_table;

    int dres;

    dlg =
	gtk_dialog_new_with_buttons("Decrackling preferences",
			NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_OK, GTK_RESPONSE_OK,
			 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, NULL, NULL);

    dialog_table = gtk_table_new(3, 2, 0);

    gtk_table_set_row_spacings(GTK_TABLE(dialog_table), 4);
    gtk_table_set_col_spacings(GTK_TABLE(dialog_table), 6);

    gtk_widget_show(dialog_table);

    dcr_entry =
	add_number_entry_with_label_double(decrackle_level,
					   "Decrackle level (default = 0.2) ",
					   dialog_table, 1);
    dcw_entry =
	add_number_entry_with_label_int(decrackle_window,
					"Decrackling window (default = 2000)",
					dialog_table, 2);
// What on earth does "3 [7]" mean?
    dca_entry =
	add_number_entry_with_label_int(decrackle_average,
					"Decrackling average window (default = 3 [7])",
					dialog_table, 3);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox), dialog_table,
		       TRUE, TRUE, 0);

    dres = gwc_dialog_run(GTK_DIALOG(dlg));

    if (dres == 0) {
	decrackle_level =
	    atof(gtk_entry_get_text((GtkEntry *) dcr_entry));
	decrackle_window =
	    atoi(gtk_entry_get_text((GtkEntry *) dcw_entry));
	decrackle_average =
	    atoi(gtk_entry_get_text((GtkEntry *) dca_entry));
    }

    gtk_widget_destroy(dlg);
}



void load_mp3_simple_encoding_preferences(void)
{
    GKeyFile  *key_file = read_config();

    if (g_key_file_has_group(key_file, "mp3_simple_encoding_params") == TRUE) {
/* MP3 */

    if (g_key_file_get_string(key_file, "mp3_simple_encoding_params", "enc_quality_level", NULL) != NULL)
	strcpy(encoding_prefs.mp3_quality_level,
	       g_key_file_get_string(key_file, "mp3_simple_encoding_params", "enc_quality_level", NULL));

    if (g_key_file_get_string(key_file, "mp3_simple_encoding_params", "mp3_location", NULL) != NULL)
	strcpy(encoding_prefs.mp3loc,
	       g_key_file_get_string(key_file, "mp3_simple_encoding_params", "mp3_location", NULL));

    if (g_key_file_get_string(key_file, "mp3_simple_encoding_params", "artist", NULL) != NULL)
	strcpy(encoding_prefs.artist,
	       g_key_file_get_string(key_file, "mp3_simple_encoding_params", "artist", NULL));

    if (g_key_file_get_string(key_file, "mp3_simple_encoding_params", "album", NULL) != NULL)
	strcpy(encoding_prefs.album,
	       g_key_file_get_string(key_file, "mp3_simple_encoding_params", "album", NULL));

    }
    
    g_key_file_free (key_file);
}

void load_mp3_encoding_preferences(void)
{
    GKeyFile  *key_file = read_config();

    if (g_key_file_has_group(key_file, "mp3_encoding_params") == TRUE) {
/* MP3 */

     if (g_key_file_get_string(key_file, "mp3_encoding_params", "enc_bitrate", NULL) != NULL)
	strcpy(encoding_prefs.mp3_bitrate,
	       g_key_file_get_string(key_file, "mp3_encoding_params", "enc_bitrate", NULL));

    if (g_key_file_get_string(key_file, "mp3_encoding_params", "enc_quality_level", NULL) != NULL)
	strcpy(encoding_prefs.mp3_quality_level,
	       g_key_file_get_string(key_file, "mp3_encoding_params", "enc_quality_level", NULL));

    if (g_key_file_get_string(key_file, "mp3_encoding_params", "lowpass_freq", NULL) != NULL)
	strcpy(encoding_prefs.mp3_lowpass_freq,
	       g_key_file_get_string(key_file, "mp3_encoding_params", "lowpass_freq", NULL));

    if (g_key_file_get_string(key_file, "mp3_encoding_params", "highpass_freq", NULL) != NULL)
	strcpy(encoding_prefs.mp3_highpass_freq,
	       g_key_file_get_string(key_file, "mp3_encoding_params", "highpass_freq", NULL));

    encoding_prefs.mp3_br_mode = g_key_file_get_integer(key_file, "mp3_encoding_params", "br_mode", NULL);
    encoding_prefs.mp3presets = g_key_file_get_integer(key_file, "mp3_encoding_params", "presets", NULL);
    encoding_prefs.mp3_sse = g_key_file_get_integer(key_file, "mp3_encoding_params", "sse", NULL);
    encoding_prefs.mp3_threednow = g_key_file_get_integer(key_file, "mp3_encoding_params", "threednow", NULL);
    encoding_prefs.mp3_lame_mmx_enabled =
	g_key_file_get_integer(key_file, "mp3_encoding_params", "lame_mmx_enabled", NULL);
    encoding_prefs.mp3_mmx = g_key_file_get_integer(key_file, "mp3_encoding_params", "mmx", NULL);
    encoding_prefs.mp3_copyrighted = g_key_file_get_integer(key_file, "mp3_encoding_params", "copyrighted", NULL);
    encoding_prefs.mp3_add_crc = g_key_file_get_integer(key_file, "mp3_encoding_params", "protected", NULL);
    encoding_prefs.mp3_strict_iso = g_key_file_get_integer(key_file, "mp3_encoding_params", "strictiso", NULL);
    encoding_prefs.mp3_nofilters = g_key_file_get_integer(key_file, "mp3_encoding_params", "nofilters", NULL);
    encoding_prefs.mp3_use_lowpass = g_key_file_get_integer(key_file, "mp3_encoding_params", "uselowpass", NULL);
    encoding_prefs.mp3_use_highpass = g_key_file_get_integer(key_file, "mp3_encoding_params", "usehighpass", NULL);

    if (g_key_file_get_string(key_file, "mp3_encoding_params", "mp3_location", NULL) != NULL)
	strcpy(encoding_prefs.mp3loc,
	       g_key_file_get_string(key_file, "mp3_encoding_params", "mp3_location", NULL));
    }
    
    g_key_file_free (key_file);
}

void load_ogg_encoding_preferences(void)
{
/* OGG */
    GKeyFile  *key_file = read_config();

    if (g_key_file_has_group(key_file, "ogg_encoding_params") == TRUE) {

    if (g_key_file_get_string(key_file, "ogg_encoding_params", "ogg_quality_level", NULL) != NULL)
	strcpy(encoding_prefs.ogg_quality_level,
	       g_key_file_get_string(key_file, "ogg_encoding_params", "ogg_quality_level", NULL));

    if (g_key_file_get_string(key_file, "ogg_encoding_params", "ogg_location", NULL) != NULL)
	strcpy(encoding_prefs.oggloc,
	       g_key_file_get_string(key_file, "ogg_encoding_params", "ogg_location", NULL));

    if (g_key_file_get_string(key_file, "ogg_encoding_params", "ogg_bitrate", NULL) != NULL)
	strcpy(encoding_prefs.ogg_bitrate,
	       g_key_file_get_string(key_file, "ogg_encoding_params", "ogg_bitrate", NULL));

    if (g_key_file_get_string(key_file, "ogg_encoding_params", "ogg_maxbitrate", NULL) != NULL)
	strcpy(encoding_prefs.ogg_maxbitrate,
	       g_key_file_get_string(key_file, "ogg_encoding_params", "ogg_maxbitrate", NULL));

    if (g_key_file_get_string(key_file, "ogg_encoding_params", "ogg_minbitrate", NULL) != NULL)
	strcpy(encoding_prefs.ogg_minbitrate,
	       g_key_file_get_string(key_file, "ogg_encoding_params", "ogg_minbitrate", NULL));

    encoding_prefs.ogg_downmix = g_key_file_get_integer(key_file, "ogg_encoding_params", "ogg_downmix", NULL);

    if (g_key_file_get_string(key_file, "ogg_encoding_params", "ogg_resample", NULL) != NULL)
	strcpy(encoding_prefs.ogg_resample,
	       g_key_file_get_string(key_file, "ogg_encoding_params", "ogg_resample", NULL));

    if (g_key_file_get_string(key_file, "ogg_encoding_params", "ogg_lowpass", NULL) != NULL)
	strcpy(encoding_prefs.ogg_lowpass_frequency,
	       g_key_file_get_string(key_file, "ogg_encoding_params", "ogg_lowpass", NULL));

    if (g_key_file_get_string(key_file, "ogg_encoding_params", "ogg_bitrateavgwindow", NULL) != NULL)
	strcpy(encoding_prefs.ogg_bitrate_average_window,
	       g_key_file_get_string(key_file, "ogg_encoding_params", "ogg_bitrateavgwindow", NULL));

    encoding_prefs.ogg_useadvbravgwindow =
	g_key_file_get_integer(key_file, "ogg_encoding_params", "ogg_useadvbravgwindow", NULL);
    encoding_prefs.ogg_useresample =
	g_key_file_get_integer(key_file, "ogg_encoding_params", "ogg_useresample", NULL);
    encoding_prefs.ogg_useadvlowpass =
	g_key_file_get_integer(key_file, "ogg_encoding_params", "ogg_useadvlowpass", NULL);
    encoding_prefs.ogg_useadvlowpass =
	g_key_file_get_integer(key_file, "ogg_encoding_params", "ogg_uselowpass", NULL);
    encoding_prefs.ogg_encopt = g_key_file_get_integer(key_file, "ogg_encoding_params", "ogg_encopt", NULL);
    }
    
    g_key_file_free (key_file);
}

void save_mp3_simple_encoding_preferences(void)
{
/* MP3 */
    GKeyFile  *key_file = read_config();

    g_key_file_set_string(key_file, "mp3_simple_encoding_params", "enc_quality_level",
			    encoding_prefs.mp3_quality_level);
    g_key_file_set_string(key_file, "mp3_simple_encoding_params", "mp3_location", encoding_prefs.mp3loc);
    g_key_file_set_string(key_file, "mp3_simple_encoding_params", "artist", encoding_prefs.artist);
    g_key_file_set_string(key_file, "mp3_simple_encoding_params", "album", encoding_prefs.album);

    write_config(key_file);
}

void save_mp3_encoding_preferences(void)
{
/* MP3 */
    GKeyFile  *key_file = read_config();

    g_key_file_set_string(key_file, "mp3_encoding_params", "enc_bitrate", encoding_prefs.mp3_bitrate);
    g_key_file_set_string(key_file, "mp3_encoding_params", "enc_quality_level",
			    encoding_prefs.mp3_quality_level);
    g_key_file_set_string(key_file, "mp3_encoding_params", "lowpass_freq",
			    encoding_prefs.mp3_lowpass_freq);
    g_key_file_set_string(key_file, "mp3_encoding_params", "highpass_freq",
			    encoding_prefs.mp3_highpass_freq);
    g_key_file_set_integer(key_file, "mp3_encoding_params", "br_mode", encoding_prefs.mp3_br_mode);
    g_key_file_set_integer(key_file, "mp3_encoding_params", "presets", encoding_prefs.mp3presets);
    g_key_file_set_integer(key_file, "mp3_encoding_params", "lame_mmx_enabled",
			 encoding_prefs.mp3_lame_mmx_enabled);
    g_key_file_set_integer(key_file, "mp3_encoding_params", "sse", encoding_prefs.mp3_sse);
    g_key_file_set_integer(key_file, "mp3_encoding_params", "mmx", encoding_prefs.mp3_mmx);
    g_key_file_set_integer(key_file, "mp3_encoding_params", "threednow", encoding_prefs.mp3_threednow);
    g_key_file_set_integer(key_file, "mp3_encoding_params", "nofilters", encoding_prefs.mp3_nofilters);
    g_key_file_set_integer(key_file, "mp3_encoding_params", "uselowpass", encoding_prefs.mp3_use_lowpass);
    g_key_file_set_integer(key_file, "mp3_encoding_params", "usehighpass", encoding_prefs.mp3_use_highpass);
    g_key_file_set_integer(key_file, "mp3_encoding_params", "copyrighted", encoding_prefs.mp3_copyrighted);
    g_key_file_set_integer(key_file, "mp3_encoding_params", "protected", encoding_prefs.mp3_add_crc);
    g_key_file_set_integer(key_file, "mp3_encoding_params", "strictiso", encoding_prefs.mp3_strict_iso);
    g_key_file_set_string(key_file, "mp3_encoding_params", "mp3_location", encoding_prefs.mp3loc);

    write_config(key_file);
}

void save_ogg_encoding_preferences(void)
{
/* OGG */
    GKeyFile  *key_file = read_config();

    g_key_file_set_string(key_file, "ogg_encoding_params", "ogg_quality_level",
			    encoding_prefs.ogg_quality_level);
    g_key_file_set_string(key_file, "ogg_encoding_params", "ogg_location", encoding_prefs.oggloc);
    g_key_file_set_string(key_file, "ogg_encoding_params", "ogg_bitrate", encoding_prefs.ogg_bitrate);
    g_key_file_set_string(key_file, "ogg_encoding_params", "ogg_minbitrate",
			    encoding_prefs.ogg_minbitrate);
    g_key_file_set_string(key_file, "ogg_encoding_params", "ogg_maxbitrate",
			    encoding_prefs.ogg_maxbitrate);
    g_key_file_set_integer(key_file, "ogg_encoding_params", "ogg_downmix", encoding_prefs.ogg_downmix);
    g_key_file_set_string(key_file, "ogg_encoding_params", "ogg_resample", encoding_prefs.ogg_resample);
    g_key_file_set_string(key_file, "ogg_encoding_params", "ogg_bitrateavgwindow",
			    encoding_prefs.ogg_bitrate_average_window);
    g_key_file_set_string(key_file, "ogg_encoding_params", "ogg_lowpass",
			    encoding_prefs.ogg_lowpass_frequency);
    g_key_file_set_integer(key_file, "ogg_encoding_params", "ogg_uselowpass",
			 encoding_prefs.ogg_useadvlowpass);
    g_key_file_set_integer(key_file, "ogg_encoding_params", "ogg_useresample",
			 encoding_prefs.ogg_useresample);
    g_key_file_set_integer(key_file, "ogg_encoding_params", "ogg_useadvbravgwindow",
			 encoding_prefs.ogg_useadvbravgwindow);
    g_key_file_set_integer(key_file, "ogg_encoding_params", "ogg_encopt", encoding_prefs.ogg_encopt);

    write_config(key_file);
}

extern struct denoise_prefs denoise_prefs;
static int noise_suppression_method, window_type;

void load_denoise_preferences(void)
{
    denoise_prefs.noise_suppression_method = 3 ;
    denoise_prefs.window_type = 2 ;
    denoise_prefs.smoothness = 11;
    denoise_prefs.FFT_SIZE = 4096 ;
    denoise_prefs.n_noise_samples = 16 ;
    denoise_prefs.amount = 0.5 ;
    denoise_prefs.dn_gamma = 0.95 ;
    denoise_prefs.randomness = 0.0 ;
    denoise_prefs.min_sample_freq = 0.0 ;
    denoise_prefs.max_sample_freq = 44100.0 ;
    denoise_prefs.freq_filter = 0 ;
    denoise_prefs.estimate_power_floor = 0 ;

    GKeyFile  *key_file = read_config();
    
    if (g_key_file_has_group(key_file, "denoise_params") == TRUE) {
    denoise_prefs.n_noise_samples =
	g_key_file_get_integer(key_file, "denoise_params", "n_noise_samples", NULL);
    denoise_prefs.smoothness = g_key_file_get_integer(key_file, "denoise_params", "smoothness", NULL);
    denoise_prefs.FFT_SIZE = g_key_file_get_integer(key_file, "denoise_params", "FFT_SIZE", NULL);
    denoise_prefs.amount = g_key_file_get_double(key_file, "denoise_params", "amount", NULL);
    denoise_prefs.dn_gamma = g_key_file_get_double(key_file, "denoise_params", "dn_gamma", NULL);
    denoise_prefs.randomness = g_key_file_get_double(key_file, "denoise_params", "randomness", NULL);
    denoise_prefs.window_type = g_key_file_get_integer(key_file, "denoise_params", "window_type", NULL);
    denoise_prefs.freq_filter = g_key_file_get_integer(key_file, "denoise_params", "freq_filter", NULL);
    denoise_prefs.estimate_power_floor = g_key_file_get_integer(key_file, "denoise_params", "estimate_power_floor", NULL);
    denoise_prefs.min_sample_freq = g_key_file_get_double(key_file, "denoise_params", "min_sample_freq", NULL);
    denoise_prefs.max_sample_freq = g_key_file_get_double(key_file, "denoise_params", "max_sample_freq", NULL);
    denoise_prefs.noise_suppression_method =
    g_key_file_get_integer(key_file, "denoise_params", "noise_suppression_method", NULL);
    }
    
    g_key_file_free (key_file);
}

void save_denoise_preferences(void)
{
    GKeyFile  *key_file = read_config();

    g_key_file_set_integer(key_file, "denoise_params", "n_noise_samples", denoise_prefs.n_noise_samples);
    g_key_file_set_integer(key_file, "denoise_params", "smoothness", denoise_prefs.smoothness);
    g_key_file_set_integer(key_file, "denoise_params", "FFT_SIZE", denoise_prefs.FFT_SIZE);
    g_key_file_set_double(key_file, "denoise_params", "amount", denoise_prefs.amount);
    g_key_file_set_double(key_file, "denoise_params", "dn_gamma", denoise_prefs.dn_gamma);
    g_key_file_set_double(key_file, "denoise_params", "randomness", denoise_prefs.randomness);
    g_key_file_set_integer(key_file, "denoise_params", "window_type", denoise_prefs.window_type);

    g_key_file_set_integer(key_file, "denoise_params", "freq_filter", denoise_prefs.freq_filter);
    g_key_file_set_integer(key_file, "denoise_params", "estimate_power_floor", denoise_prefs.estimate_power_floor);
    g_key_file_set_double(key_file, "denoise_params", "min_sample_freq", denoise_prefs.min_sample_freq);
    g_key_file_set_double(key_file, "denoise_params", "max_sample_freq", denoise_prefs.max_sample_freq);

    g_key_file_set_integer(key_file, "denoise_params", "noise_suppression_method",
			 denoise_prefs.noise_suppression_method);

    write_config(key_file);
}


void fft_window_select(GtkWidget * clist, gint row, gint column,
		       GdkEventButton * event, gpointer data)
{
    if (row == 0)
	window_type = DENOISE_WINDOW_BLACKMAN;
    if (row == 1)
	window_type = DENOISE_WINDOW_BLACKMAN_HYBRID;
    if (row == 2)
	window_type = DENOISE_WINDOW_HANNING_OVERLAP_ADD;
#ifdef DENOISE_TRY_ONE_SAMPLE
    if (row == 3)
	window_type = DENOISE_WINDOW_ONE_SAMPLE;
    if (row == 4)
	window_type = DENOISE_WINDOW_WELTY;
#else
    if (row == 3)
	window_type = DENOISE_WINDOW_WELTY;
#endif
}

void noise_method_window_select(GtkWidget * clist, gint row, gint column,
				GdkEventButton * event, gpointer data)
{
    if (row == 0)
	noise_suppression_method = DENOISE_WEINER;
    if (row == 1)
	noise_suppression_method = DENOISE_POWER_SPECTRAL_SUBTRACT;
    if (row == 2)
	noise_suppression_method = DENOISE_EM;
    if (row == 3)
	noise_suppression_method = DENOISE_LORBER;
    if (row == 4)
	noise_suppression_method = DENOISE_WOLFE_GODSILL;
    if (row == 5)
	noise_suppression_method = DENOISE_EXPERIMENTAL ;
}

void denoise_set_preferences(GtkWidget * widget, gpointer data)
{
    GtkWidget *dlg;
    GtkWidget *fft_size_entry;
    GtkWidget *amount_entry;
    GtkWidget *gamma_entry;
    GtkWidget *smoothness_entry;
    GtkWidget *n_noise_entry;
    GtkWidget *dialog_table;
    GtkWidget *freq_filter_entry;
    GtkWidget *estimate_power_floor_entry;
    GtkWidget *min_sample_freq_entry;
    GtkWidget *max_sample_freq_entry;

    int dres;

    GtkWidget *fft_window_list;

    gchar *fft_window_titles[] = { "Windowing Function" };
#ifdef DENOISE_TRY_ONE_SAMPLE
    gchar *fft_window_parms[4][1] = { {"Blackman"},
    {"Hybrid Blackman-Full Pass"},
    {"Hanning-overlap-add (Best)"},
    {"Hanning-one-sample-shift (Experimental)"}
#else
    gchar *fft_window_parms[3][1] = { {"Blackman"},
    {"Hybrid Blackman-Full Pass"},
    {"Hanning-overlap-add (Best)"}
#endif
    };

    GtkWidget *noise_method_window_list;

    gchar *noise_method_window_titles[] = { "Noise Suppresion Method" };
    gchar *noise_method_window_parms[6][1] = {
						{"Weiner"},
						{"Power Spectral Subtraction"},
						{"Ephraim-Malah 1984"},
						{"Lorber & Hoeldrich (Best)"},
						{"Wolfe & Godsill (Experimental)"},
						{"Extremely Experimental"}
					    };

    load_denoise_preferences();


    fft_window_list = gtk_clist_new_with_titles(1, fft_window_titles);
    gtk_clist_set_selection_mode(GTK_CLIST(fft_window_list),
				 GTK_SELECTION_SINGLE);
    gtk_clist_append(GTK_CLIST(fft_window_list), fft_window_parms[0]);
    gtk_clist_append(GTK_CLIST(fft_window_list), fft_window_parms[1]);
    gtk_clist_append(GTK_CLIST(fft_window_list), fft_window_parms[2]);
#ifdef DENOISE_TRY_ONE_SAMPLE
    gtk_clist_append(GTK_CLIST(fft_window_list), fft_window_parms[3]);
#endif
    gtk_clist_select_row(GTK_CLIST(fft_window_list),
			 denoise_prefs.window_type, 0);
    gtk_signal_connect(GTK_OBJECT(fft_window_list), "select_row",
		       GTK_SIGNAL_FUNC(fft_window_select), NULL);
    window_type = denoise_prefs.window_type;
    gtk_widget_show(fft_window_list);

    noise_method_window_list =
	gtk_clist_new_with_titles(1, noise_method_window_titles);
    gtk_clist_set_selection_mode(GTK_CLIST(noise_method_window_list),
				 GTK_SELECTION_SINGLE);
    gtk_clist_append(GTK_CLIST(noise_method_window_list),
		     noise_method_window_parms[0]);
    gtk_clist_append(GTK_CLIST(noise_method_window_list),
		     noise_method_window_parms[1]);
    gtk_clist_append(GTK_CLIST(noise_method_window_list),
		     noise_method_window_parms[2]);
    gtk_clist_append(GTK_CLIST(noise_method_window_list),
		     noise_method_window_parms[3]);
    gtk_clist_append(GTK_CLIST(noise_method_window_list),
		     noise_method_window_parms[4]);
    gtk_clist_append(GTK_CLIST(noise_method_window_list),
		     noise_method_window_parms[5]);
    gtk_clist_select_row(GTK_CLIST(noise_method_window_list),
			 denoise_prefs.noise_suppression_method, 0);
    gtk_signal_connect(GTK_OBJECT(noise_method_window_list), "select_row",
		       GTK_SIGNAL_FUNC(noise_method_window_select), NULL);
    noise_suppression_method = denoise_prefs.noise_suppression_method;
    gtk_widget_show(noise_method_window_list);

    dlg =
	gtk_dialog_new_with_buttons("Denoise",
			NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_OK, GTK_RESPONSE_OK,
			 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, NULL, NULL);

    dialog_table = gtk_table_new(9, 2, 0);

    gtk_table_set_row_spacings(GTK_TABLE(dialog_table), 4);
    gtk_table_set_col_spacings(GTK_TABLE(dialog_table), 6);

    gtk_widget_show(dialog_table);

    fft_size_entry =
	add_number_entry_with_label_int(denoise_prefs.FFT_SIZE,
					"FFT_SIZE (4096 for 44.1kHz sample rate)", dialog_table, 0);
    amount_entry =
	add_number_entry_with_label_double(denoise_prefs.amount,
					   "Reduction (0.0-1.0)",
					   dialog_table, 1);
    smoothness_entry =
	add_number_entry_with_label_int(denoise_prefs.smoothness,
					"(Smoothness for Blackman window (2-11)",
					dialog_table, 2);
    n_noise_entry =
	add_number_entry_with_label_int(denoise_prefs.n_noise_samples,
					"# noise samples (2-16)",
					dialog_table, 3);

    gamma_entry =
	add_number_entry_with_label_double(denoise_prefs.dn_gamma,
					   "gamma -- for Lorber & Hoelrich or Ephraim-Malah , (0.9-1, try 0.98)",
					   dialog_table, 4);

    freq_filter_entry =
	add_number_entry_with_label_int(denoise_prefs.freq_filter,
					"Apply freq filter (0,1)", dialog_table, 5);

    estimate_power_floor_entry =
	add_number_entry_with_label_int(denoise_prefs.estimate_power_floor,
					"Estimate power floor (0,1)", dialog_table, 6);

    min_sample_freq_entry =
	add_number_entry_with_label_int(denoise_prefs.min_sample_freq,
					"Minimum frequency to use in noise sample (Hz)", dialog_table, 7);

    max_sample_freq_entry =
	add_number_entry_with_label_int(denoise_prefs.max_sample_freq,
					"Maximum frequency to use in noise sample (Hz)", dialog_table, 8);


/*      combo_entry1 = gnome_number_entry_gtk_entry (GNOME_NUMBER_ENTRY (numberentry1));  */
/*      gtk_widget_show (combo_entry1);  */

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox), dialog_table,
		       TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox), fft_window_list,
		       TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox),
		       noise_method_window_list, TRUE, TRUE, 0);

    dres = gwc_dialog_run(GTK_DIALOG(dlg));

    if (dres == 0) {
	int i;
	i = atoi(gtk_entry_get_text((GtkEntry *) fft_size_entry));
	for (denoise_prefs.FFT_SIZE = 8;
	     denoise_prefs.FFT_SIZE < i
	     && denoise_prefs.FFT_SIZE < DENOISE_MAX_FFT;
	     denoise_prefs.FFT_SIZE *= 2);
	denoise_prefs.amount =
	    atof(gtk_entry_get_text((GtkEntry *) amount_entry));
	denoise_prefs.smoothness =
	    atoi(gtk_entry_get_text((GtkEntry *) smoothness_entry));
	denoise_prefs.n_noise_samples =
	    atoi(gtk_entry_get_text((GtkEntry *) n_noise_entry));
	denoise_prefs.dn_gamma =
	    atof(gtk_entry_get_text((GtkEntry *) gamma_entry));
	denoise_prefs.noise_suppression_method =
	    noise_suppression_method;
	denoise_prefs.window_type = window_type;
	denoise_prefs.freq_filter = atoi(gtk_entry_get_text((GtkEntry *) freq_filter_entry));
	denoise_prefs.estimate_power_floor = atoi(gtk_entry_get_text((GtkEntry *) estimate_power_floor_entry));
	denoise_prefs.min_sample_freq = atof(gtk_entry_get_text((GtkEntry *) min_sample_freq_entry));
	denoise_prefs.max_sample_freq = atof(gtk_entry_get_text((GtkEntry *) max_sample_freq_entry));
	save_denoise_preferences();
    }

    gtk_widget_destroy(dlg) ;
}
