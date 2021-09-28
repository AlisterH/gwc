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

/* amplify.c */
#include <stdlib.h>
#include "gwc.h"

#define BUFSIZE 10000

static gfloat amount_first_l[2] = {1.0,0.0} ;
static gfloat amount_last_l[2] = {1.0,0.0} ;
static gfloat amount_first_r[2] = {0.0,1.0} ;
static gfloat amount_last_r[2] = {0.0,1.0} ;
static int feather_width = 20 ;

void simple_amplify_audio(struct sound_prefs *p, long first, long last, int channel_mask, double amount)
{
    long left[BUFSIZE], right[BUFSIZE] ;
    long current, i ;
    int loops = 0 ;

    current = first ;

    push_status_text("Amplifying audio") ;
    update_progress_bar(0.0,PROGRESS_UPDATE_INTERVAL,TRUE) ;

    {

	while(current <= last) {
	    long n = MIN(last - current + 1, BUFSIZE) ;
	    long tmplast = current + n - 1 ;
	    gfloat p = (gfloat)(current-first)/(last-first+1) ;

	    n = read_wavefile_data(left, right, current, tmplast) ;

	    update_progress_bar(p,PROGRESS_UPDATE_INTERVAL,FALSE) ;

	    for(i = 0 ; i < n ; i++) {
		if(channel_mask & 0x01) {
		    left[i] = lrint(amount*left[i]) ;
		}

		if(channel_mask & 0x02) {
		    right[i] = lrint(amount*right[i]) ;

		}
	    }

	    write_wavefile_data(left, right, current, tmplast) ;

	    current += n ;

	    if(last - current < 10) loops++ ;

	    if(loops > 5) {
		warning("infinite loop in amplify_audio, programming error\n") ;
	    }
	}

	resample_audio_data(p, first, last) ;
	save_sample_block_data(p) ;
    }

    update_progress_bar(0.0,PROGRESS_UPDATE_INTERVAL,TRUE) ;
    pop_status_text() ;

    main_redraw(FALSE, TRUE) ;
}


void amplify_audio(struct sound_prefs *p, long first, long last, int channel_mask)
{
    long left[BUFSIZE], right[BUFSIZE] ;
    long current, i ;
    int loops = 0 ;

    current = first ;

    push_status_text("Amplifying audio") ;
    update_progress_bar(0.0,PROGRESS_UPDATE_INTERVAL,TRUE) ;

    {

	while(current <= last) {
	    long n = MIN(last - current + 1, BUFSIZE) ;
	    long tmplast = current + n - 1 ;
	    gfloat p = (gfloat)(current-first)/(last-first+1) ;

	    n = read_wavefile_data(left, right, current, tmplast) ;

	    update_progress_bar(p,PROGRESS_UPDATE_INTERVAL,FALSE) ;

	    for(i = 0 ; i < n ; i++) {
		long icurrent = current + i ;
		double p_last = (double)(icurrent-first+1)/(double)(last-first+1) ;
		double p_first = 1.0 - p_last ;
		double feather_p = 1.0 ;
		double wet_left, wet_right ;

		if(icurrent - first < feather_width)
			feather_p = (double)(icurrent-first)/(double)(feather_width) ;

		if(last - icurrent < feather_width)
			feather_p = (double)(last - icurrent)/(double)(feather_width) ;

		if(channel_mask & 0x01) {
		    wet_left = ((double)left[i]*(amount_first_l[0]*p_first+amount_last_l[0]*p_last)) ;
		    wet_left += ((double)right[i]*(amount_first_r[0]*p_first+amount_last_r[0]*p_last)) ;
		}


		if(channel_mask & 0x02) {
		    wet_right = ((double)left[i]*(amount_first_l[1]*p_first+amount_last_l[1]*p_last)) ;
		    wet_right += ((double)right[i]*(amount_first_r[1]*p_first+amount_last_r[1]*p_last)) ;
		}

		if(channel_mask & 0x01) {
		    left[i] = lrint((double)left[i]*(1.0-feather_p) + wet_left*feather_p) ;
		}

		if(channel_mask & 0x02) {
		    right[i] = lrint((double)right[i]*(1.0-feather_p) + wet_right*feather_p) ;
		}
	    }

	    write_wavefile_data(left, right, current, tmplast) ;

	    current += n ;

	    if(last - current < 10) loops++ ;

	    if(loops > 5) {
		warning("infinite loop in amplify_audio, programming error\n") ;
	    }
	}

	resample_audio_data(p, first, last) ;
	save_sample_block_data(p) ;
    }

    update_progress_bar(0.0,PROGRESS_UPDATE_INTERVAL,TRUE) ;
    pop_status_text() ;

    main_redraw(FALSE, TRUE) ;
}

int amplify_dialog(struct sound_prefs current, struct view *v)
{
    GtkWidget *dlg, *maxtext, *dialog_table, *leftframe, *rightframe, *l_tbl, *r_tbl  ;
    GtkWidget *amount_first_entry_l[2] ;
    GtkWidget *amount_last_entry_l[2] ;
    GtkWidget *amount_first_entry_r[2] ;
    GtkWidget *amount_last_entry_r[2] ;
    GtkWidget *feather_width_entry ;
    int row = 0 ;
    int dres ;
    char buf[200] ;

/*      Alister: everything using gtkcurve has been commented out since gwc-0.20-09 and it appears prior to that  */
/*      the implementation was never actually completed i.e. the widget worked, but it didn't do anything except  */
/*      print values to stdout.                                                                                   */

/*      GtkWidget *curve ;  */
/*      gfloat curve_data[20] ;  */

    dialog_table = gtk_table_new(3,1,0) ;
    l_tbl = gtk_table_new(5,2,0) ;
    r_tbl = gtk_table_new(5,2,0) ;

    gtk_table_set_row_spacings(GTK_TABLE(dialog_table), 12) ;
    gtk_table_set_col_spacings(GTK_TABLE(dialog_table), 6) ;
    gtk_widget_show (dialog_table);

    gtk_table_set_row_spacings(GTK_TABLE(l_tbl), 4) ;
    gtk_table_set_col_spacings(GTK_TABLE(l_tbl), 6) ;
    gtk_container_set_border_width(GTK_CONTAINER(l_tbl), 5) ;
    gtk_widget_show (l_tbl);

    gtk_table_set_row_spacings(GTK_TABLE(r_tbl), 4) ;
    gtk_table_set_col_spacings(GTK_TABLE(r_tbl), 6) ;
    gtk_container_set_border_width(GTK_CONTAINER(r_tbl), 5) ;
    gtk_widget_show (r_tbl);

    dlg = gtk_dialog_new_with_buttons("Amplify",
			GTK_WINDOW(main_window), GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			 GTK_STOCK_OK, GTK_RESPONSE_OK, NULL, NULL);
    gtk_dialog_set_default_response (GTK_DIALOG(dlg), GTK_RESPONSE_OK);

    sprintf(buf, "Maximum amplification without clipping is %6.2f.\n", (double)1.0/(double)current.max_value) ;

    maxtext = gtk_label_new (buf);
    gtk_widget_show (maxtext);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG(dlg)->vbox), maxtext, TRUE, TRUE, 0);

/*      curve = gtk_curve_new ();  */
/*      gtk_box_pack_start (GTK_BOX (GTK_DIALOG(dlg)->vbox), curve, TRUE, TRUE, row++);  */
/*      gtk_widget_show (curve);  */

/*      //gtk_curve_set_curve_type(GTK_CURVE(curve),GTK_CURVE_TYPE_LINEAR) ;  */
/*      gtk_curve_set_range(GTK_CURVE(curve),0.0,100.0,0.0,100.0) ;  */
/*      gtk_curve_reset(GTK_CURVE(curve)) ;  */
/*      //gtk_curve_set_vector(GTK_CURVE(curve),2,curve_data) ;  */

    leftframe = gtk_frame_new ("Left channel source");
    gtk_container_add(GTK_CONTAINER(leftframe), l_tbl) ;
    gtk_widget_show (leftframe);
    gtk_table_attach_defaults(GTK_TABLE(dialog_table), leftframe,  0, 2, row, row+1) ;
    row++ ;

    amount_first_entry_l[0] = add_number_entry_with_label_double(amount_first_l[0], "Left Channel beginning:", l_tbl, 0) ;
    amount_last_entry_l[0] = add_number_entry_with_label_double(amount_last_l[0], "Left Channel end:", l_tbl, 1) ;
    amount_first_entry_r[0] = add_number_entry_with_label_double(amount_first_r[0], "Right Channel beginning:", l_tbl, 2) ;
    amount_last_entry_r[0] = add_number_entry_with_label_double(amount_last_r[0], "Right Channel end:", l_tbl, 3) ;

    rightframe = gtk_frame_new ("Right channel source");
    gtk_container_add(GTK_CONTAINER(rightframe), r_tbl) ;
    gtk_widget_show (rightframe);
    gtk_table_attach_defaults(GTK_TABLE(dialog_table), rightframe,  0, 2, row, row+1) ;
    row++ ;

    amount_first_entry_l[1] = add_number_entry_with_label_double(amount_first_l[1], "Left Channel beginning:", r_tbl, 0) ;
    amount_last_entry_l[1] = add_number_entry_with_label_double(amount_last_l[1], "Left Channel end:", r_tbl, 1) ;
    amount_first_entry_r[1] = add_number_entry_with_label_double(amount_first_r[1], "Right Channel beginning:", r_tbl, 2) ;
    amount_last_entry_r[1] = add_number_entry_with_label_double(amount_last_r[1], "Right Channel end:", r_tbl, 3) ;

    feather_width_entry = add_number_entry_with_label_int(feather_width, "Feather width", dialog_table, row++) ;

    gtk_box_pack_start (GTK_BOX (GTK_DIALOG(dlg)->vbox), dialog_table, TRUE, TRUE, 0);

    dres = gwc_dialog_run(GTK_DIALOG(dlg)) ;

    if(dres == 0) {
	int i ;
	for(i = 0 ; i < 2 ; i++) {
	    amount_first_l[i] = atof(gtk_entry_get_text((GtkEntry *)amount_first_entry_l[i])) ;
	    amount_last_l[i] = atof(gtk_entry_get_text((GtkEntry *)amount_last_entry_l[i])) ;
	    amount_first_r[i] = atof(gtk_entry_get_text((GtkEntry *)amount_first_entry_r[i])) ;
	    amount_last_r[i] = atof(gtk_entry_get_text((GtkEntry *)amount_last_entry_r[i])) ;
	}
	feather_width = atoi(gtk_entry_get_text((GtkEntry *)feather_width_entry)) ;
    }

/*      {  */
/*  	int i ;  */
/*    */
/*  	gtk_curve_get_vector(GTK_CURVE(curve), 10, curve_data) ;  */
/*    */
/*  	for(i = 0 ; i < 10 ; i++) {  */
/*  	    printf("%lg %lg\n", curve_data[i*2], curve_data[i*2+1]) ;  */
/*  	}  */
/*    */
/*      }  */

    gtk_widget_destroy(dlg) ;

    if(dres == 0)
	return 1 ;

    return 0 ;
}

void batch_normalize(struct sound_prefs *p, long first, long last, int channel_mask)
{
	amount_first_l[0] = (double)1.0/(double)p->max_value;
	amount_last_l[0] = amount_first_l[0] ;
	amount_first_r[1] = amount_first_l[0] ;
	amount_last_r[1] = amount_first_l[0] ;
    feather_width = 2000;
    //fprintf(stderr, "Maximum amplification without clipping is %f \n", amount_first_l[0]) ;
    amplify_audio(p,first,last,channel_mask);
}
