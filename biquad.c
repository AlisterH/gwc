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

/* biquad.c */
#include <stdlib.h>
#include <glib.h>
#include "gtkledbar.h"
#include "gwc.h"

struct {
    int filter_type ;
    int feather_width ;
    double dbGain ;
    double Fc ;
    double bandwidth ;
    int harmonics ;
    } filter_prefs ;

#define BIQUAD

#ifdef BIQUAD
#include "biquad.h"
#else
#include "iir_lp.h"
#endif

#define BUFSIZE 10000

static gfloat dbGain;
static gfloat bandwidth;
static gfloat Fc;
static int filter_type;
static int feather_width;

int row2filter(int row)
{
    if(row == 0) return LPF ;
    if(row == 1) return HPF ;
    if(row == 2) return NOTCH ;
    if(row == 3) return BPF ;
    if(row == 4) return PEQ ;
    if(row == 5) return LSH ;
    if(row == 6) return HSH ;
    return 0 ;
}

int filter2row(gint filter_type)
{
    if(filter_type == LPF) return 0 ;
    if(filter_type == HPF) return 1 ;
    if(filter_type == NOTCH) return 2 ;
    if(filter_type == BPF) return 3 ;
    if(filter_type == PEQ) return 4 ;
    if(filter_type == LSH) return 5 ;
    if(filter_type == HSH) return 6 ;
    return 0 ;
}

void load_filter_preferences(void)
{
    //looks like confusion in the previous source.  Should this default to NOTCH?
    filter_prefs.filter_type = 0;
    filter_prefs.feather_width = 20;
    //looks like confusion in the previous source.  Should this default to 3?
    filter_prefs.dbGain = 2;
    filter_prefs.Fc = 120;
    filter_prefs.bandwidth = 0.5;

    GKeyFile  *key_file = read_config();
    int row ;

    // We should probably have a separate test for each preference...
    //if (g_key_file_get_string(key_file, "filter_params", "filter_type", NULL) != NULL) {
    if (g_key_file_has_group(key_file, "filter_params") == TRUE) {
        row = g_key_file_get_integer(key_file, "filter_params", "filter_type", NULL);
    filter_prefs.filter_type = row2filter(row) ;

        filter_prefs.feather_width = g_key_file_get_integer(key_file, "filter_params", "feather_width", NULL);
        filter_prefs.dbGain = g_key_file_get_double(key_file, "filter_params", "dbGain", NULL);
        filter_prefs.Fc = g_key_file_get_double(key_file, "filter_params", "Fc", NULL);
        filter_prefs.bandwidth = g_key_file_get_double(key_file, "filter_params", "bandwidth", NULL);
    }
    g_key_file_free (key_file);
}

void save_filter_preferences(void)
{
    GKeyFile  *key_file = read_config();
    int row = filter2row(filter_prefs.filter_type) ;

    g_key_file_set_integer(key_file, "filter_params", "filter_type", row) ;
    g_key_file_set_integer(key_file, "filter_params", "feather_width", filter_prefs.feather_width);
    g_key_file_set_double(key_file, "filter_params", "dbGain", filter_prefs.dbGain);
    g_key_file_set_double(key_file, "filter_params", "Fc", filter_prefs.Fc);
    g_key_file_set_double(key_file, "filter_params", "bandwidth", filter_prefs.bandwidth);

    write_config(key_file);
}


void filter_audio(struct sound_prefs *p, long first, long last, int channel_mask)
{
    long left[BUFSIZE], right[BUFSIZE] ;
    long x_left[3], x_right[3] ;
    long y_left[3], y_right[3] ;
    long current, i, f ;
    int loops = 0 ;
    long ring_buffer_length ;
    double rb_left[BUFSIZE], rb_right[BUFSIZE] ;

    load_filter_preferences() ;

    filter_type = filter_prefs.filter_type ;
    feather_width = filter_prefs.feather_width ;
    dbGain = filter_prefs.dbGain ;
    Fc = filter_prefs.Fc ;
    bandwidth = filter_prefs.bandwidth ;

    switch(filter_type) {
	case LPF: g_print("LPF") ; break ;
	case HPF: g_print("HPF") ; break ;
	case BPF: g_print("BPF") ; break ;
	case NOTCH: g_print("NOTCH") ; break ;
	case PEQ: g_print("PEQ") ; break ;
	case LSH: g_print("LSH") ; break ;
	case HSH: g_print("HSH") ; break ;
	default: g_print("UNKNOWN! ") ; break ;
    }

    g_print(" Fc:%lg bandwidth:%lg srate:%d\n", Fc, bandwidth,p->rate) ;


/*  filtered_sample = current_sample - ring_buffer[j];  */
/*  ring_buffer[j] = ring_buffer[j] * 0.9 + current_sample * 0.1;  */
/*  j++;  */
/*  j %= ring_buffer_length;  */

#define MAXH 5
#ifdef BIQUAD_CALL
extern biquad *BiQuad_new(int type, smp_type dbGain, /* gain of filter */
                          smp_type freq,             /* center frequency */
                          smp_type srate,            /* sampling rate */
                          smp_type bandwidth);       /* bandwidth in octaves */
#endif

#ifdef BIQUAD
    biquad *iir_left, *iir_right ;

    iir_left  = BiQuad_new(filter_type, dbGain, Fc, p->rate, bandwidth) ;
    iir_right = BiQuad_new(filter_type, dbGain, Fc, p->rate, bandwidth) ;

    ring_buffer_length = p->rate / Fc + 0.5 ;
    for(i = 0 ; i < ring_buffer_length ; i++) {
	rb_left[i] = 0.0 ;
	rb_right[i] = 0.0 ;
    }
#else
    FILTER iir_left, iir_right ;

    get_iir_lp_coefs(Fc, &iir_left, p->rate) ;
    get_iir_lp_coefs(Fc, &iir_right, p->rate) ;
#endif

    /* testing for filter response */
    {
	double freq, d_freq = 10 ;

	g_print("left a0:%lg\n", iir_left->a0) ;
	g_print("left a1:%lg\n", iir_left->a1) ;
	g_print("left a2:%lg\n", iir_left->a2) ;
	g_print("left b1:%lg\n", iir_left->a3) ;
	g_print("left b2:%lg\n", iir_left->a4) ;

	for(freq = 10 ; freq < 20001 ; freq += d_freq) {
	    double from_formula ;
	    double rr = BiQuad_response(freq, p->rate, iir_right, &from_formula) ;
	    g_print("freq:%5.0lf response(dB):%10.2lg formula(dB):%10.2lg\n", freq, rr, from_formula) ;
	    if(freq > 90) d_freq = 100 ;
	    if(freq > 900) d_freq = 1000 ;
	    if(freq > 9000) d_freq = 2000 ;
	}
    }

    current = first ;

    push_status_text("Filtering audio") ;
    g_print("Filtering audio %ld to %ld\n", first, last) ;
    update_status_bar(0.0,STATUS_UPDATE_INTERVAL,TRUE) ;

    {

	while(current <= last) {
	    long n = MIN(last - current + 1, BUFSIZE) ;
	    long tmplast = current + n - 1 ;
	    gfloat p = (gfloat)(current-first)/(last-first+1) ;

	    n = read_wavefile_data(left, right, current, tmplast) ;

	    update_status_bar(p,STATUS_UPDATE_INTERVAL,FALSE) ;

	    for(i = 0 ; i < n ; i++) {
		long icurrent = current + i ;
		double feather_p = 1.0 ;
		double wet_left, wet_right ;

		x_left[0] = x_left[1] ;
		x_left[1] = x_left[2] ;

		x_right[0] = x_right[1] ;
		x_right[1] = x_right[2] ;

		y_left[0] = y_left[1] ;
		y_left[1] = y_left[2] ;

		y_right[0] = y_right[1] ;
		y_right[1] = y_right[2] ;
		x_right[2] = right[i] ;

		if(icurrent - first < feather_width)
			feather_p = (double)(icurrent-first)/(feather_width) ;

		if(last - icurrent < feather_width)
			feather_p = (double)(last - icurrent)/(feather_width) ;

		if(channel_mask & 0x01) {
		    long dry_left = left[i];

#ifdef BIQUAD
		    wet_left = 32768.*BiQuad(dry_left/32768., iir_left) ;
#else
		    wet_left = iir_filter(dry_left, &iir_left) ;
#endif

		    left[i] = lrint(dry_left*(1.0-feather_p) + wet_left*feather_p) ;
		}

		if(channel_mask & 0x02) {
		    long dry_right = right[i] ;

#ifdef BIQUAD
		    wet_right = 32768.0*BiQuad(dry_right/32768., iir_right) ;
#else
		    wet_right = iir_filter(dry_right, &iir_right) ;
#endif

		    right[i] = lrint(dry_right*(1.0-feather_p) + wet_right*feather_p) ;

		}
	    }

	    write_wavefile_data(left, right, current, tmplast) ;

	    current += n ;

	    if(last - current < 10) loops++ ;

	    if(loops > 5) {
		warning("inifinite loop in filter_audio, programming error\n") ;
	    }
	}

	resample_audio_data(p, first, last) ;
	save_sample_block_data(p) ;
    }

    update_status_bar(0.0,STATUS_UPDATE_INTERVAL,TRUE) ;
    pop_status_text() ;

#ifdef BIQUAD
	free(iir_left) ;
	free(iir_right) ;
#else
    filter_free(&iir_left) ;
    filter_free(&iir_right) ;
#endif

    main_redraw(FALSE, TRUE) ;
}

void type_window_select(GtkWidget * clist, gint row, gint column,
		       GdkEventButton * event, gpointer data)
{
    filter_type = row2filter(row) ;
}

static GtkWidget *dbGain_entry ;
static GtkWidget *freq_entry ;
static GtkWidget *bandwidth_entry ;
static struct sound_prefs local_sound_prefs ;
static long first_sample, last_sample ;

void show_response(GtkWidget *w, gpointer gdata)
{
    biquad *iir_left ;
    double freq, d_freq=10 ;

    Fc = atof(gtk_entry_get_text((GtkEntry *)freq_entry)) ;
    dbGain = atof(gtk_entry_get_text((GtkEntry *)dbGain_entry)) ;
    bandwidth = atof(gtk_entry_get_text((GtkEntry *)bandwidth_entry)) ;
    iir_left  = BiQuad_new(filter_type, dbGain, Fc, 44100, bandwidth) ;

    for(freq = 10 ; freq < 20001 ; freq += d_freq) {
	double from_formula ;
	double rl = BiQuad_response(freq, 44100, iir_left, &from_formula) ;
	g_print("freq:%5.0lf response(dB):%10.4lg formula(dB):%10.4lg\n", freq, rl, from_formula) ;
	if(freq > 90) d_freq = 100 ;
	if(freq > 900) d_freq = 1000 ;
	if(freq > 9000) d_freq = 2000 ;
    }
    free(iir_left) ;

    g_print("first_sample:%ld last_sample:%ld\n", first_sample, last_sample) ;


    {
	struct denoise_prefs p ;
	p.n_noise_samples = 10 ;
	p.FFT_SIZE = 8192 ;
	print_noise_sample(&local_sound_prefs, &p, first_sample, last_sample) ;
    }
}


int filter_dialog(struct sound_prefs current, struct view *v)
{
    GtkWidget *dlg, *dialog_table ;
    GtkWidget *feather_entry ;
    int dclose = 0 ;
    int row = 0 ;
    int dres ;

    GtkWidget *type_window_list;
    GtkWidget *show_response_button ;

    gchar *type_window_titles[] = { "Filter Type" };
    gchar *type_window_parms[7][1] = { {"Low Pass"},
    {"High Pass"},
    {"Notch"},
    {"Band Pass"},
    {"Peaking EQ"},
    {"Low Shelf Filter"},
    {"High Shelf Filter"},
    };

    local_sound_prefs = current ;
    first_sample = v->selected_first_sample ;
    last_sample = v->selected_last_sample ;

    load_filter_preferences();
    filter_type = filter_prefs.filter_type ;

    type_window_list = gtk_clist_new_with_titles(1, type_window_titles);
    gtk_clist_set_selection_mode(GTK_CLIST(type_window_list),
				 GTK_SELECTION_SINGLE);
    gtk_clist_append(GTK_CLIST(type_window_list), type_window_parms[0]);
    gtk_clist_append(GTK_CLIST(type_window_list), type_window_parms[1]);
    gtk_clist_append(GTK_CLIST(type_window_list), type_window_parms[2]);
    gtk_clist_append(GTK_CLIST(type_window_list), type_window_parms[3]);
    gtk_clist_append(GTK_CLIST(type_window_list), type_window_parms[4]);
    gtk_clist_append(GTK_CLIST(type_window_list), type_window_parms[5]);
    gtk_clist_append(GTK_CLIST(type_window_list), type_window_parms[6]);

    gtk_clist_select_row(GTK_CLIST(type_window_list),
			 filter2row(filter_prefs.filter_type), 0);
    gtk_signal_connect(GTK_OBJECT(type_window_list), "select_row",
		       GTK_SIGNAL_FUNC(type_window_select), NULL);

    gtk_widget_show(type_window_list);

    dialog_table = gtk_table_new(5,2,0) ;

    gtk_table_set_row_spacings(GTK_TABLE(dialog_table), 4) ;
    gtk_table_set_col_spacings(GTK_TABLE(dialog_table), 6) ;
    gtk_widget_show (dialog_table);

    dlg = gtk_dialog_new_with_buttons("Biquad filter",
			GTK_WINDOW(main_window), GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			 GTK_STOCK_OK, GTK_RESPONSE_OK, NULL, NULL);
    gtk_dialog_set_default_response (GTK_DIALOG(dlg), GTK_RESPONSE_OK);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox), type_window_list,
		       TRUE, TRUE, 0);
    feather_entry = add_number_entry_with_label_int(filter_prefs.feather_width, "Feather Width", dialog_table, row++) ;
    dbGain_entry = add_number_entry_with_label_double(filter_prefs.dbGain, "Gain (db)", dialog_table, row++) ;
    freq_entry = add_number_entry_with_label_double(filter_prefs.Fc, "Center frequency freq (hertz)", dialog_table, row++) ;
    bandwidth_entry = add_number_entry_with_label_double(filter_prefs.bandwidth, "bandwidth (octaves)", dialog_table, row++) ;

    gtk_box_pack_start (GTK_BOX (GTK_DIALOG(dlg)->vbox), dialog_table, TRUE, TRUE, 0);

    show_response_button = gtk_button_new_with_label("Show Response") ;
    gtk_signal_connect(GTK_OBJECT(show_response_button), "clicked", GTK_SIGNAL_FUNC(show_response), NULL) ;
    gtk_widget_show(show_response_button) ;

    gtk_box_pack_start (GTK_BOX (GTK_DIALOG(dlg)->vbox), show_response_button, TRUE, TRUE, 0);

    dres = gwc_dialog_run(GTK_DIALOG(dlg)) ;

    if(dres == 0) {
	feather_width = atoi(gtk_entry_get_text((GtkEntry *)feather_entry)) ;
	Fc = atof(gtk_entry_get_text((GtkEntry *)freq_entry)) ;
	dbGain = atof(gtk_entry_get_text((GtkEntry *)dbGain_entry)) ;
	bandwidth = atof(gtk_entry_get_text((GtkEntry *)bandwidth_entry)) ;
	filter_prefs.feather_width = feather_width ;
	filter_prefs.dbGain = dbGain ;
	filter_prefs.Fc = Fc ;
	filter_prefs.bandwidth = bandwidth ;
	filter_prefs.filter_type = filter_type ;
	dclose = 1 ;
    }

    gtk_widget_destroy(dlg) ;

    save_filter_preferences() ;

    if(dres == 0)
	return 1 ;

    return 0 ;
}


/* Simple implementation of Biquad filters -- Tom St Denis
 *
 * Based on the work

Cookbook formulae for audio EQ biquad filter coefficients
---------------------------------------------------------
by Robert Bristow-Johnson, pbjrbj@viconet.com  a.k.a. robert@audioheads.com

 * Available on the web at

http://www.smartelectronix.com/musicdsp/text/filters005.txt

 * Enjoy.
 *
 * This work is hereby placed in the public domain for all purposes, whether
 * commercial, free [as in speech] or educational, etc.  Use the code and please
 * give me credit if you wish.
 *
 * Tom St Denis -- http://tomstdenis.home.dhs.org
*/

/* Computes a BiQuad filter on a sample */
smp_type BiQuad(smp_type sample, biquad * b)
{
    smp_type result;

    /* compute result */
    result = b->a0 * sample + b->a1 * b->x1 + b->a2 * b->x2 -
        b->a3 * b->y1 - b->a4 * b->y2;

    /* shift x1 to x2, sample to x1 */
    b->x2 = b->x1;
    b->x1 = sample;

    /* shift y1 to y2, result to y1 */
    b->y2 = b->y1;
    b->y1 = result;

    return result;
}

/* sets up a BiQuad Filter */
biquad *BiQuad_new(int type, smp_type dbGain, smp_type freq,
smp_type srate, smp_type bandwidth)
{
    biquad *b;
    smp_type A, omega, sn, cs, alpha, beta;
    smp_type a0, a1, a2, b0, b1, b2;

    b = malloc(sizeof(biquad));
    if (b == NULL)
        return NULL;

    /* setup variables */
    A = pow(10.0, dbGain /40.0);
    omega = 2.0 * M_PI * freq /srate;
    sn = sin(omega);
    cs = cos(omega);
    alpha = sn * sinh(M_LN2 /2.0 * bandwidth * omega /sn);
    beta = sqrt(A + A);

    switch (type) {
    case LPF:
        b0 = (1.0 - cs) /2.0;
        b1 = 1.0 - cs;
        b2 = (1.0 - cs) /2.0;
        a0 = 1.0 + alpha;
        a1 = -2.0 * cs;
        a2 = 1.0 - alpha;
        break;
    case HPF:
        b0 = (1.0 + cs) /2.0;
        b1 = -(1.0 + cs);
        b2 = (1.0 + cs) /2.0;
        a0 = 1.0 + alpha;
        a1 = -2.0 * cs;
        a2 = 1.0 - alpha;
        break;
    case BPF:
        b0 = alpha;
        b1 = 0.0;
        b2 = -alpha;
        a0 = 1.0 + alpha;
        a1 = -2.0 * cs;
        a2 = 1.0 - alpha;
        break;
    case NOTCH:
        b0 = 1.0;
        b1 = -2.0 * cs;
        b2 = 1.0;
        a0 = 1.0 + alpha;
        a1 = -2.0 * cs;
        a2 = 1.0 - alpha;
        break;
    case PEQ:
        b0 = 1.0 + (alpha * A);
        b1 = -2.0 * cs;
        b2 = 1.0 - (alpha * A);
        a0 = 1.0 + (alpha /A);
        a1 = -2.0 * cs;
        a2 = 1.0 - (alpha /A);
        break;
    case LSH:
        b0 = A * ((A + 1.0) - (A - 1.0) * cs + beta * sn);
        b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cs);
        b2 = A * ((A + 1.0) - (A - 1.0) * cs - beta * sn);
        a0 = (A + 1.0) + (A - 1.0) * cs + beta * sn;
        a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cs);
        a2 = (A + 1.0) + (A - 1.0) * cs - beta * sn;
        break;
    case HSH:
        b0 = A * ((A + 1.0) + (A - 1.0) * cs + beta * sn);
        b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cs);
        b2 = A * ((A + 1.0) + (A - 1.0) * cs - beta * sn);
        a0 = (A + 1.0) - (A - 1.0) * cs + beta * sn;
        a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cs);
        a2 = (A + 1.0) - (A - 1.0) * cs - beta * sn;
        break;
    default:
        free(b);
        return NULL;
    }


    /* the cannonical coefficients */
    b->can_a0 = a0 ;
    b->can_a1 = a1 ;
    b->can_a2 = a2 ;
    b->can_b0 = b0 ;
    b->can_b1 = b1 ;
    b->can_b2 = b2 ;

    /* precompute the coefficients */
    b->a0 = b0 /a0;
    b->a1 = b1 /a0;
    b->a2 = b2 /a0;
    b->a3 = a1 /a0;
    b->a4 = a2 /a0;

#ifdef HARDWIRE
    /* hardwire BPF at 10 kHz, srate = 44.1 kHz */
    b->a0 =  1.0 ;
    b->a1 =  0.0 ;
    b->a2 = -1.0 ;
    b->a3 = 0.1 ;
    b->a4 = 0.9 ;

    b->can_a0 = b->a0 ;
    b->can_a1 = b->a1 ;
    b->can_a2 = b->a2 ;
    b->can_b0 = 1.0 ;
    b->can_b1 = b->a3 ;
    b->can_b2 = b->a4 ;
#endif

    /* zero initial samples */
    b->x1 = b->x2 = 0;
    b->y1 = b->y2 = 0;

    return b;
}

#define M_SQR(x) ((x)*(x))

double BiQuad_response(double freq, double srate, biquad *p, double *from_formula)
{
    double omega = 2.0 * M_PI * freq /(double)srate;
    double phi = sin(omega/2) ;
    double a0 = p->can_a0 ;
    double a1 = p->can_a1 ;
    double a2 = p->can_a2 ;
    double b0 = p->can_b0 ;
    double b1 = p->can_b1 ;
    double b2 = p->can_b2 ;

    double phi2 = phi*phi ;

    *from_formula = 
     10*log10( M_SQR(b0+b1+b2) - 4.0*(b0*b1 + 4.0*b0*b2 + b1*b2)*phi + 16.0*b0*b2*phi2 )
     -10*log10( M_SQR(a0+a1+a2) - 4.0*(a0*a1 + 4.0*a0*a2 + a1*a2)*phi + 16.0*a0*a2*phi2 )  ;

    int i ;
    double sum_dry2 = 0, sum_wet2  = 0;

    p->x1 = p->x2 = 0;
    p->y1 = p->y2 = 0;

    for(i = 0 ; i < srate ; i++) {
	double x = sin(2.0*M_PI*freq/srate*(double)i) ;
	double y ;
	sum_dry2 += x*x ;
	y = BiQuad(x, p) ;
	sum_wet2 += y*y ;
    }

    return 20.0*log10(sum_wet2/sum_dry2) ;
}


/* from robert bristow-johnson's response, march 1, 2005
http://groups.google.com/group/comp.dsp/browse_frm/thread/8c0fa8d396aeb444/a1bc5b63ac56b686

20*log10[|H(e^jw)|] =

 10*log10[ (b0+b1+b2)^2 - 4*(b0*b1 + 4*b0*b2 + b1*b2)*phi + 16*b0*b2*phi^2 ]
 -10*log10[ (a0+a1+a2)^2 - 4*(a0*a1 + 4*a0*a2 + a1*a2)*phi + 16*a0*a2*phi^2 ] 
*/
