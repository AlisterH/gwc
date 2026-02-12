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

/* response plot data */
#define RESP_POINTS 256
static double resp_db[RESP_POINTS];
static int resp_n = 0;
static GtkWidget *response_area = NULL;
static unsigned int channel_mask = 0x03; /* default: both; but we actually get this from the view */
static double biquad_max_safe_gain_db; /* we will draw a line showing the maximum gain without
										* clipping; get it from amplify.c */

/* noise spectrum overlay */
#define NOISE_POINTS 4096
static double noise_freq[NOISE_POINTS];
static double noise_left_db[NOISE_POINTS];
static double noise_right_db[NOISE_POINTS];
static int noise_n = 0;
static gboolean noise_valid = FALSE;
/* predicted (filtered) noise overlay */
static double predicted_noise_left_db[NOISE_POINTS];
static double predicted_noise_right_db[NOISE_POINTS];
static gboolean predicted_noise_valid = FALSE;

static GdkWindow *response_window = NULL;
static GdkGC *green_gc = NULL;
static GdkGC *dash_gc  = NULL;
static GdkGC *blue_gc  = NULL;
static GdkGC *bg_gc = NULL;
static double max_plot_freq = 20000.0;

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

void show_response(GtkWidget *w, gpointer gdata);

static void filter_type_changed(GtkWidget *clist,
                                gint row,
                                gint column,
                                GdkEventButton *event,
                                gpointer data);

void filter_audio(struct sound_prefs *p, long first, long last, int channel_mask)
{
    long left[BUFSIZE], right[BUFSIZE] ;
    long x_left[3], x_right[3] ;
    long y_left[3], y_right[3] ;
    long current, i ;
    int loops = 0 ;

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
		warning("infinite loop in filter_audio, programming error\n") ;
	    }
	}

	resample_audio_data(p, first, last) ;
	save_sample_block_data(p) ;
    }

    update_progress_bar(0.0,PROGRESS_UPDATE_INTERVAL,TRUE) ;
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


static GtkWidget *dbGain_entry ;
static GtkWidget *freq_entry ;
static GtkWidget *bandwidth_entry ;
static struct sound_prefs local_sound_prefs ;

/* ------------------------------------------------------------ */
/* Enable/disable Gain control depending on filter type         */
/* ------------------------------------------------------------ */
static void
update_filter_ui(int filter_type)
{
    gboolean gain_ok =
        (filter_type == PEQ ||
         filter_type == LSH ||
         filter_type == HSH);

    gtk_widget_set_sensitive(dbGain_entry, gain_ok);

    gboolean bw_ok =
        (filter_type == PEQ ||
         filter_type == BPF ||
         filter_type == NOTCH);

    gtk_widget_set_sensitive(bandwidth_entry, bw_ok);

}

 static void
 filter_type_changed(GtkWidget *clist,
                     gint row,
                     gint column,
                     GdkEventButton *event,
                     gpointer data)
 {
     filter_type = row2filter(row);

     update_filter_ui(filter_type);

     /* Redraw response and predicted noise */
     show_response(NULL, NULL);
 }

/* ------------------------------------------------------------ */
/* Capture noise spectrum for plotting                           */
/* ------------------------------------------------------------ */
static void
capture_noise_spectrum(struct view *v,
                       struct sound_prefs *pPrefs,
                       struct denoise_prefs *pDnprefs)
{
    int k;
	long first = v->selected_first_sample;
	long last  = v->selected_last_sample;

    long nsamples;

    /* ------------------------------------------------------------ */
    /* Defensive bounds checks:
     * - selection must be within [0, nsamples-1]
     * - selection should be at least FFT_SIZE long if possible
     *   (get_noise_sample typically reads FFT windows)
     * ------------------------------------------------------------ */
    nsamples = pPrefs->n_samples; /* maintained elsewhere from soundfile_count_samples() */
    if (nsamples <= 0)
        return;

    /* Validate FFT size against our local buffer allocation assumptions */
    if (pDnprefs->FFT_SIZE <= 0 || pDnprefs->FFT_SIZE > DENOISE_MAX_FFT)
        return;

    /* Clamp selection to file bounds */
    if (first < 0) first = 0;
    if (last  < 0) last  = 0;
    if (first > last) {
        long tmp = first;
        first = last;
        last = tmp;
    }
    if (last >= nsamples)
        last = nsamples - 1;
    if (first >= nsamples)
        first = nsamples - 1;

    /* Ensure we have at least FFT_SIZE samples if the file is long enough.
     * If selection is too short, shift/expand it without exceeding EOF. */
    if ((last - first + 1) < pDnprefs->FFT_SIZE) {
        if (nsamples >= pDnprefs->FFT_SIZE) {
            /* Prefer to keep the selection ending at 'last' (useful near EOF) */
            first = last - pDnprefs->FFT_SIZE + 1;
            if (first < 0) {
                first = 0;
                last = pDnprefs->FFT_SIZE - 1;
            }
        } else {
            /* File shorter than FFT; fall back to whole file.
             * (Ideally get_noise_sample should pad; at least this won't read past EOF.) */
            first = 0;
            last = nsamples - 1;
        }
    }

	fftw_real *mem_block;
	fftw_real *left_noise_min, *left_noise_max, *left_noise_avg;
	fftw_real *right_noise_min, *right_noise_max, *right_noise_avg;

	mem_block = malloc(sizeof(fftw_real) * DENOISE_MAX_FFT * 6);
	if (!mem_block)
	return;

	left_noise_max  = mem_block;
	right_noise_max = mem_block + DENOISE_MAX_FFT;
	left_noise_avg  = mem_block + 2 * DENOISE_MAX_FFT;
	left_noise_min  = mem_block + 3 * DENOISE_MAX_FFT;
	right_noise_min = mem_block + 4 * DENOISE_MAX_FFT;
	right_noise_avg = mem_block + 5 * DENOISE_MAX_FFT;

	get_noise_sample(pPrefs, pDnprefs,
					first, last,
					left_noise_min, left_noise_max, left_noise_avg,
					right_noise_min, right_noise_max, right_noise_avg);

    noise_n = pDnprefs->FFT_SIZE / 2;
    if (noise_n > NOISE_POINTS)
        noise_n = NOISE_POINTS;

    for (k = 1; k <= noise_n; k++) {
        double freq = (double)pPrefs->rate / 2.0 /
                      (double)(pDnprefs->FFT_SIZE / 2) * k;

        noise_freq[k-1] = freq;

		/* ------------------------------------------------------------ */
		/* Convert FFT magnitude to dBFS                                 */
		/* Reference: full-scale sine wave                               */
		/* ------------------------------------------------------------ */

        /* left_noise_avg[] contains accumulated FFT power (amplitude^2)
           Match scaling used in print_noise_sample() */

        double power_l =
            left_noise_avg[k] /
            (double)pDnprefs->n_noise_samples;

        double power_r =
            right_noise_avg[k] /
            (double)pDnprefs->n_noise_samples;

        /* Normalise FFT scaling (magnitude scales with N) */
        double N = (double)pDnprefs->FFT_SIZE;
        power_l /= (N * N);
        power_r /= (N * N);

        /* Convert power (sample^2) to dBFS */
        noise_left_db[k-1] =
            10.0 * log10(power_l + 1e-30)
            - 20.0 * log10(32768.0);

        noise_right_db[k-1] =
            10.0 * log10(power_r + 1e-30)
            - 20.0 * log10(32768.0);

    }

    free(mem_block);

   noise_valid = TRUE;
}

/* ------------------------------------------------------------ */
/* helpers for plotting dB curves                     */
/* ------------------------------------------------------------ */
static int
db_to_y(double db, int h, double db_min, double db_max)
{
    double t;

    if (db > db_max)
        t = 0.0;
    else if (db < db_min)
        t = 1.0;
    else
        t = (db_max - db) / (db_max - db_min);

    return 10 + t * (h - 40);
}

static void
draw_db_curve(GtkWidget *widget,
              GdkGC     *gc,
              const double *db,
              int        n,
              int        w,
              int        h,
              double     db_min,
              double     db_max)
{
    int i;

    for (i = 1; i < n; i++) {
        int x1 = 40 + (i - 1) * (w - 50) / (n - 1);
        int x2 = 40 +  i      * (w - 50) / (n - 1);

        int y1 = db_to_y(db[i - 1], h, db_min, db_max);
        int y2 = db_to_y(db[i],     h, db_min, db_max);

        gdk_draw_line(widget->window, gc, x1, y1, x2, y2);
    }
}

/* ------------------------------------------------------------ */
/* Draw dB curve using frequency array (log-frequency X axis)   */
/* ------------------------------------------------------------ */
static void
draw_db_curve_freq(GtkWidget *widget,
                   GdkGC     *gc,
                   const double *freq,
                   const double *db,
                   int        n,
                   int        w,
                   int        h,
                   double     db_min,
                   double     db_max,
                   double     fmin,
                   double     fmax)
{
    int i;

    for (i = 1; i < n; i++) {
        /* Don't plot to the left of the y axis */
        if (freq[i-1] <= fmin || freq[i] <= fmin)
            continue;

        double t1 = log(freq[i-1] / fmin) / log(fmax / fmin);
        double t2 = log(freq[i]   / fmin) / log(fmax / fmin);

        int x1 = 40 + t1 * (w - 50);
        int x2 = 40 + t2 * (w - 50);

        int y1 = db_to_y(db[i-1], h, db_min, db_max);
        int y2 = db_to_y(db[i],   h, db_min, db_max);

        gdk_draw_line(widget->window, gc, x1, y1, x2, y2);
    }
}

/* GTK2 expose handler for response plot */
static gboolean response_expose(GtkWidget *widget,
                                GdkEventExpose *event,
                                gpointer data)
{
    int w = widget->allocation.width;
    int h = widget->allocation.height;

	/* AI advises guarding against unrealized widgets */
	if (!GTK_WIDGET_REALIZED(widget))
		return TRUE;

	/* Don't set up all this more often than we need to */
	if (widget->window != response_window) {

		/* Window changed or first realization */
		response_window = widget->window;

		/* Drop old GCs */
		if (green_gc) g_object_unref(green_gc);
		if (dash_gc)  g_object_unref(dash_gc);
		if (blue_gc)  g_object_unref(blue_gc);
		if (bg_gc)  g_object_unref(bg_gc);

		green_gc = dash_gc = blue_gc = NULL;

		/* Recreate GCs for this window */
		{
			GdkColor dark_green = { 0, 0, 32768, 0 };

			green_gc = gdk_gc_new(response_window);
			gdk_gc_set_rgb_fg_color(green_gc, &dark_green);

			dash_gc = gdk_gc_new(response_window);
			gdk_gc_set_rgb_fg_color(dash_gc, &dark_green);

			gint8 dashes[] = { 4, 4 };
			gdk_gc_set_line_attributes(dash_gc,
									   1,
									   GDK_LINE_ON_OFF_DASH,
									   GDK_CAP_BUTT,
									   GDK_JOIN_MITER);
			gdk_gc_set_dashes(dash_gc, 0, dashes, 2);

			GdkColor blue = { 0, 0, 0, 65535 };
			blue_gc = gdk_gc_new(response_window);
			gdk_gc_set_rgb_fg_color(blue_gc, &blue);
			
			bg_gc = gdk_gc_new(widget->window);
			/* For now just set a gray background - later we can figure out how to set suitable line colours to use with this: GdkGC *bg = widget->style->bg_gc[GTK_STATE_NORMAL]; */
			GdkColor gray = { 0, 59624, 59624, 59367 };
			gdk_gc_set_rgb_fg_color(bg_gc, &gray);
		}
	}

    double min_db = -120.0; /* noise floor */
    double max_db =   20.0; /* headroom above 0 dBFS so we can plot max amplification*/

    gdk_draw_rectangle(widget->window, bg_gc, TRUE, 0, 0, w, h);

    gdk_draw_line(widget->window, green_gc, 40, h-30, w-10, h-30);
    gdk_draw_line(widget->window, green_gc, 40, 10,   40,  h-30);
	/* ----- Horizontal line at 0 dB ----- */
	int y0 = db_to_y(0.0, h, min_db, max_db);
	gdk_draw_line(widget->window, green_gc, 40, y0, w-10, y0);

    /* ----- Axis labels ----- */
    PangoLayout *layout;
    PangoFontDescription *font;
    int lw, lh;

    layout = gtk_widget_create_pango_layout(widget, NULL);
    font = pango_font_description_from_string("Sans 9");
    pango_layout_set_font_description(layout, font);

    /* Y axis label */
    pango_layout_set_text(layout, "dB", -1);
    pango_layout_get_pixel_size(layout, &lw, &lh);
    gdk_draw_layout(widget->window, green_gc,
                    42,           /* just right of Y axis */
                    12,           /* just below top */
                    layout);

    /* X axis label */
    pango_layout_set_text(layout, "Frequency (Hz)", -1);
    pango_layout_get_pixel_size(layout, &lw, &lh);
    gdk_draw_layout(widget->window, green_gc,
                    w - lw - 12,   /* inside right edge */
                    h - lh - 32,   /* just above X axis */
                    layout);

    int db;
    for (db = (int)min_db; db <= (int)max_db; db += 20) {
        int y = h-30 - (db - min_db) * (h-40) / (max_db - min_db);
        char buf[16];

        gdk_draw_line(widget->window, green_gc, 35, y, 40, y);
        snprintf(buf, sizeof(buf), "%d", db);
        pango_layout_set_text(layout, buf, -1);
        gdk_draw_layout(widget->window, green_gc, 8, y - 6, layout);
    }

    const int freqs[] = {10, 100, 1000, 10000};
    int j;

    for (j = 0; j < 4; j++) {
        double t = log((double)freqs[j] / 10.0) /
                   log(max_plot_freq / 10.0);
        int x = 40 + t * (w - 50);
        char buf[16];

        gdk_draw_line(widget->window, green_gc, x, h-30, x, h-25);
        snprintf(buf, sizeof(buf), "%d", freqs[j]);
        pango_layout_set_text(layout, buf, -1);
        gdk_draw_layout(widget->window, green_gc, x - 10, h - 22, layout);
    }

    if (resp_n < 2)
        return TRUE;

    /* ------------------------------------------------------------ */
    /* Dashed vertical line at centre frequency (Fc)                */
    /* ------------------------------------------------------------ */
    if (Fc > 0.0) {
        double fmin = 10.0;
        double fmax = max_plot_freq;
        double t;
        int x_fc;

        if (Fc < fmin)
            t = 0.0;
        else if (Fc > fmax)
            t = 1.0;
        else
            t = log(Fc / fmin) / log(fmax / fmin);

        x_fc = 40 + t * (w - 50);

        gdk_draw_line(widget->window,
                      dash_gc,
                      x_fc, 10,
                      x_fc, h - 30);

    }

    /* ---- Filter response ---- */
    draw_db_curve(widget, green_gc,
                  resp_db, resp_n,
                  w, h, min_db, max_db);

    /* ---- Noise spectrum overlay ---- */
	if (channel_mask & 0x01) {
		draw_db_curve_freq(widget, green_gc,
						   noise_freq, noise_left_db, noise_n,
						   w, h, min_db, max_db,
						   10.0, max_plot_freq);
	}

	if (channel_mask & 0x02) {
		draw_db_curve_freq(widget, green_gc,
						   noise_freq, noise_right_db, noise_n,
						   w, h, min_db, max_db,
						   10.0, max_plot_freq);
	}

/* ---- Predicted noise (filtered) overlay ---- */
 if (predicted_noise_valid && noise_n > 1) {
     GdkGC *clip_gc = gdk_gc_new(widget->window);
     GdkColor red = {0, 65535, 0, 0};   /* clipping */
     GdkColor blue = {0, 0, 0, 65535};  /* safe */
 
     int start = 0;
     gboolean clipping = FALSE;
 
     /* This safe gain check isn't actually useful except for a Peaking EQ with a wide bandwidth */
	 /* We are plotting in the frequency domain */
	 /* To know what gain will really clip we need to apply the biquad to the noise samples in time domain then measure the actual peak (TODO)*/
     double safe_gain_db = biquad_max_safe_gain_db;

    /* draw left channel */
     if (channel_mask & 0x01) {
         clipping = (predicted_noise_left_db[0] > safe_gain_db);
         gdk_gc_set_rgb_fg_color(clip_gc, clipping ? &red : &blue);

        for (int i = 1; i < noise_n; i++) {
             gboolean clip_now = (predicted_noise_left_db[i] > safe_gain_db);

            if (clip_now != clipping) {
                /* draw segment up to here */
                draw_db_curve_freq(widget, clip_gc,
                                   &noise_freq[start], &predicted_noise_left_db[start],
                                   i - start + 1,
                                   w, h, min_db, max_db,
                                   10.0, max_plot_freq);
                start = i - 1; /* start new segment at previous point */
                clipping = clip_now;
                gdk_gc_set_rgb_fg_color(clip_gc, clipping ? &red : &blue);
            }
        }

        /* draw final segment */
        draw_db_curve_freq(widget, clip_gc,
                           &noise_freq[start], &predicted_noise_left_db[start],
                           noise_n - start,
                           w, h, min_db, max_db,
                           10.0, max_plot_freq);
    }

    /* draw right channel */
    if (channel_mask & 0x02) {
        start = 0;
		clipping = (predicted_noise_right_db[0] > safe_gain_db);
        gdk_gc_set_rgb_fg_color(clip_gc, clipping ? &red : &blue);

        for (int i = 1; i < noise_n; i++) {
                gboolean clip_now = (predicted_noise_right_db[i] > safe_gain_db);

            if (clip_now != clipping) {
                draw_db_curve_freq(widget, clip_gc,
                                   &noise_freq[start], &predicted_noise_right_db[start],
                                   i - start + 1,
                                   w, h, min_db, max_db,
                                   10.0, max_plot_freq);
                start = i - 1;
                clipping = clip_now;
                gdk_gc_set_rgb_fg_color(clip_gc, clipping ? &red : &blue);
            }
        }

        draw_db_curve_freq(widget, clip_gc,
                           &noise_freq[start], &predicted_noise_right_db[start],
                           noise_n - start,
                           w, h, min_db, max_db,
                           10.0, max_plot_freq);
    }

    g_object_unref(clip_gc);


    }
    pango_font_description_free(font);
    g_object_unref(layout);
    return TRUE;
}

void show_response(GtkWidget *w, gpointer gdata)
{
    biquad *iir;
    int i;
    double fmin = 10.0;
    double fmax;
    double srate;

    srate = local_sound_prefs.rate;
    max_plot_freq = srate * 0.5;
    /* in real life this should never occur */
    if (max_plot_freq < fmin)
        max_plot_freq = fmin;

    fmax = max_plot_freq;

    Fc = atof(gtk_entry_get_text((GtkEntry *)freq_entry)) ;
    dbGain = atof(gtk_entry_get_text((GtkEntry *)dbGain_entry)) ;
    bandwidth = atof(gtk_entry_get_text((GtkEntry *)bandwidth_entry)) ;
    iir = BiQuad_new(filter_type, dbGain, Fc, srate, bandwidth) ;
    if (!iir)
        return;

    resp_n = RESP_POINTS;

    for (i = 0; i < RESP_POINTS; i++) {
        double t = (double)i / (RESP_POINTS - 1);
        double freq = fmin * pow(fmax / fmin, t);
        double dummy;
        resp_db[i] = BiQuad_response(freq, srate, iir, &dummy);
    }
    /* ------------------------------------------------------------ */
    /* Predict filtered noise (display only)                        */
    /* ------------------------------------------------------------ */
    predicted_noise_valid = FALSE;
    if (noise_valid && noise_n > 1) {
        for (i = 0; i < noise_n; i++) {
            double dummy;
            double f = noise_freq[i];
            double gain_db;

            if (f < 1.0)
                continue;

            // Convert linear magnitude to dB before adding
			gain_db = BiQuad_response(f, srate, iir, &dummy);

			/* predicted = measured + filter gain (all in dBFS) */
			predicted_noise_left_db[i] =
				noise_left_db[i] + gain_db;

			predicted_noise_right_db[i] =
				noise_right_db[i] + gain_db;

        }
        predicted_noise_valid = TRUE;
    }
    free(iir);

    gtk_widget_queue_draw(response_area);
}


int filter_dialog(struct sound_prefs current, struct view *v)
{
    GtkWidget *dlg, *dialog_table ;
    GtkWidget *feather_entry ;
    int row = 0 ;
    int dres ;

    GtkWidget *type_window_list;

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
    channel_mask = v->channel_selection_mask;

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

    gtk_widget_show(type_window_list);

    dialog_table = gtk_table_new(5,2,0) ;

    gtk_table_set_row_spacings(GTK_TABLE(dialog_table), 4) ;
    gtk_table_set_col_spacings(GTK_TABLE(dialog_table), 6) ;
    gtk_widget_show (dialog_table);

    dlg = gtk_dialog_new_with_buttons("Biquad filter",
			GTK_WINDOW(main_window), GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			 GTK_STOCK_OK, GTK_RESPONSE_OK, NULL, NULL);
    gtk_dialog_set_default_response (GTK_DIALOG(dlg), GTK_RESPONSE_OK);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox), type_window_list,
		       TRUE, TRUE, 0);
    feather_entry = add_number_entry_with_label_int(filter_prefs.feather_width, "Feather Width", dialog_table, row++) ;
    dbGain_entry = add_number_entry_with_label_double(filter_prefs.dbGain, "Gain (db)", dialog_table, row++) ;
    freq_entry = add_number_entry_with_label_double(filter_prefs.Fc, "Center frequency (hertz)", dialog_table, row++) ;
    bandwidth_entry = add_number_entry_with_label_double(filter_prefs.bandwidth, "Bandwidth (octaves)", dialog_table, row++) ;

    gtk_box_pack_start (GTK_BOX (GTK_DIALOG(dlg)->vbox), dialog_table, TRUE, TRUE, 0);

    response_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(response_area, 420, 220);
    gtk_signal_connect(GTK_OBJECT(response_area), "expose-event",
                       GTK_SIGNAL_FUNC(response_expose), NULL);
    gtk_widget_show(response_area);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox),
                       response_area, TRUE, TRUE, 0);
    /* ------------------------------------------------------------ */
    /* Live filter response updates                                 */
    /* ------------------------------------------------------------ */

    /* Update when numeric entries change */
    gtk_signal_connect(GTK_OBJECT(freq_entry), "changed",
                       GTK_SIGNAL_FUNC(show_response), NULL);

    gtk_signal_connect(GTK_OBJECT(dbGain_entry), "changed",
                       GTK_SIGNAL_FUNC(show_response), NULL);

    gtk_signal_connect(GTK_OBJECT(bandwidth_entry), "changed",
                       GTK_SIGNAL_FUNC(show_response), NULL);

    /* Update when filter type changes */
	gtk_signal_connect(GTK_OBJECT(type_window_list), "select_row",
					   GTK_SIGNAL_FUNC(filter_type_changed), NULL);

	update_filter_ui(filter_type);

    /* ------------------------------------------------------------ */

	/* get this so we can check if the signal will be clipped */
	double maxamp = max_gain_for_view_or_selection(&current, &current, v);
	biquad_max_safe_gain_db = (maxamp > 0.0) ? 20.0 * log10(maxamp) : -INFINITY;
	printf("biquad_max_safe_gain_db: %lg\n", biquad_max_safe_gain_db) ;

    /* Capture noise spectrum once on dialog open */
    {
        struct denoise_prefs p;
        memset(&p, 0, sizeof(p));
        p.n_noise_samples = 10;
        p.FFT_SIZE = 8192;
		noise_valid = FALSE; /* capture_noise_spectrum() sets TRUE only on success */
        capture_noise_spectrum(v, &local_sound_prefs, &p);
		/* Force redraw now that noise exists */
		show_response(NULL, NULL);
    }

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
    smp_type Q;

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
		/* 2nd-order Butterworth */
        Q = 1.0 / sqrt(2.0);
        alpha = sn / (2.0 * Q);
        b0 = (1.0 - cs) / 2.0;
        b1 = 1.0 - cs;
        b2 = (1.0 - cs) / 2.0;
        a0 = 1.0 + alpha;
        a1 = -2.0 * cs;
        a2 = 1.0 - alpha;
        break;
    case HPF:
		/* 2nd-order Butterworth */
        Q = 1.0 / sqrt(2.0);
        alpha = sn / (2.0 * Q);
        b0 = (1.0 + cs) / 2.0;
        b1 = -(1.0 + cs);
        b2 = (1.0 + cs) / 2.0;
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


    /* the canonical coefficients
	 * we don't actually use them */
    /*b->can_a0 = a0 ;
    b->can_a1 = a1 ;
    b->can_a2 = a2 ;
    b->can_b0 = b0 ;
    b->can_b1 = b1 ;
    b->can_b2 = b2 ;*/

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

    /*b->can_a0 = b->a0 ;
    b->can_a1 = b->a1 ;
    b->can_a2 = b->a2 ;
    b->can_b0 = 1.0 ;
    b->can_b1 = b->a3 ;
    b->can_b2 = b->a4 ;*/
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
    double z_re = cos(omega);
    double z_im = -sin(omega);

    /* Normalized coefficients (actual filter) */
    double b0 = p->a0;
    double b1 = p->a1;
    double b2 = p->a2;
    double a1 = p->a3;
    double a2 = p->a4;
	/* H(e^jw) numerator */
    double num_re =
        b0 + b1 * z_re + b2 * (2*z_re*z_re - 1);
    double num_im =
        b1 * z_im + b2 * (2*z_re*z_im);

    /* ------------------------------------------------------------ */
    /* Absolute magnitude response |H(e^jw)| in dB                  */
    /* No DC or peak normalization                                  */
    /* ------------------------------------------------------------ */

    /* H(e^jw) denominator */
    double den_re =
        1.0 + a1 * z_re + a2 * (2*z_re*z_re - 1);
    double den_im =
        a1 * z_im + a2 * (2*z_re*z_im);

    double num_mag = sqrt(num_re*num_re + num_im*num_im);
    double den_mag = sqrt(den_re*den_re + den_im*den_im);

    *from_formula = 20.0 * log10(num_mag / den_mag);
   return *from_formula;
}

/* Alternative formula that does the same maths using canonical coefficients */
/* Alternative formula that does the same maths using canonical coefficients */
/* double BiQuad_response(double freq, double srate, biquad *p, double *from_formula)
 {
    double omega = 2.0 * M_PI * freq / srate;

    double cos1 = cos(omega);
    double sin1 = sin(omega);
    double cos2 = cos(2.0 * omega);
    double sin2 = sin(2.0 * omega);*/

    /* Use canonical (unnormalized) coefficients */
/*    double b0 = p->can_b0;
    double b1 = p->can_b1;
    double b2 = p->can_b2;
    double a0 = p->can_a0;
    double a1 = p->can_a1;
    double a2 = p->can_a2;*/

    /* Evaluate H(e^{jw}) */
/*    double num_re = b0 + b1 * cos1 + b2 * cos2;
    double num_im = -b1 * sin1 - b2 * sin2;

    double den_re = a0 + a1 * cos1 + a2 * cos2;
    double den_im = -a1 * sin1 - a2 * sin2;

    double num_mag = sqrt(num_re*num_re + num_im*num_im);
    double den_mag = sqrt(den_re*den_re + den_im*den_im);

    *from_formula = 20.0 * log10(num_mag / den_mag);
    return *from_formula;
 }*/

/* from robert bristow-johnson's response, march 1, 2005
http://groups.google.com/group/comp.dsp/browse_frm/thread/8c0fa8d396aeb444/a1bc5b63ac56b686

20*log10[|H(e^jw)|] =

 10*log10[ (b0+b1+b2)^2 - 4*(b0*b1 + 4*b0*b2 + b1*b2)*phi + 16*b0*b2*phi^2 ]
 -10*log10[ (a0+a1+a2)^2 - 4*(a0*a1 + 4*a0*a2 + a1*a2)*phi + 16*a0*a2*phi^2 ] 
*/
