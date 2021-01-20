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

/* drawing.c */

#include <stdlib.h>
#include <gdk/gdk.h>
/*  #include <gdk-pixbuf/gnome-canvas-pixbuf.h>  */
#include "gwc.h"

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define THICKNESS 3

#define point_color "red"
#define handle_color "green"
#define point_handle_width 6
#define HANDLE_THICKNESS 1

extern int sonogram_log ;
extern GdkPixmap *audio_pixmap ;
extern GdkPixmap *highlight_pixmap ;
extern GdkPixmap *cursor_pixmap ;
int last_cursor_x = -1;
extern GtkWidget *audio_drawing_area ;
extern long markers[] ;
extern long n_markers ;
extern long num_song_markers, song_markers[] ;


GdkColor red_gdk_color = {0, 65535, 0, 0} ;
GdkColor green_gdk_color = {0, 0, 65535, 0} ;
GdkColor blue_gdk_color = {1, 0, 0, 65535} ;
GdkColor yellow_gdk_color = {2, 65535, 65535, 0} ;
GdkColor grey_gdk_color = {3, 43000, 43000, 43000} ;
GdkColor white_gdk_color = {4, 65535, 65535, 65535} ;
GdkColor black_gdk_color = {5, 0, 0, 0} ;
GdkColor orange_gdk_color = {6, 65535, 42240, 0} ;
GdkColor sonogram_color[256] ;

GdkColor *red_color = &red_gdk_color ;
GdkColor *green_color = &green_gdk_color ;
GdkColor *blue_color = &blue_gdk_color ;
GdkColor *yellow_color = &yellow_gdk_color ;
GdkColor *orange_color = &orange_gdk_color ;
GdkColor *white_color = &white_gdk_color ;
GdkColor *grey_color = &grey_gdk_color ;
GdkColor *black_color = &black_gdk_color ;

/*
#define highlight_color white_color
*/
GdkColor *highlight_color = &white_gdk_color;
#define cut_highlight_color red_color

/* a utility function to draw a rect */
static void
draw_a_rect(GdkGC *gc,
	    gint x1, gint y1, gint x2, gint y2, GdkColor *color)
{
    gdk_gc_set_foreground(gc, color) ;
    gdk_draw_rectangle(audio_pixmap, gc, TRUE, x1,y1,x2-x1+1,y2-y1+1) ;
}

/* a utility function to draw a rect */
static void
draw_a_highlight_rect(GdkGC *gc,
	    gint x1, gint y1, gint x2, gint y2, GdkColor *color)
{
    /* gdk doesn't seems to clip before passing to X which only uses
       shorts for coordinates so at high zoom wrong regions would be
       highlighted */
    if (x1 > SHRT_MAX)
       x1 = SHRT_MAX;
    if (y1 > SHRT_MAX)
       y1 = SHRT_MAX;
    if (x1 < SHRT_MIN)
       x1 = SHRT_MIN;
    if (y1 < SHRT_MIN)
       y1 = SHRT_MIN;
    if (x2 > SHRT_MAX)
       x2 = SHRT_MAX;
    if (y2 > SHRT_MAX)
       y2 = SHRT_MAX;
    if (x2 < SHRT_MIN)
       x2 = SHRT_MIN;
    if (y2 < SHRT_MIN)
       y2 = SHRT_MIN;
    gdk_gc_set_foreground(gc, color) ;
    gdk_gc_set_function(gc, GDK_XOR) ;
    gdk_draw_rectangle(highlight_pixmap, gc, TRUE, x1,y1,x2-x1+1,y2-y1+1) ;
    gdk_gc_set_function(gc, GDK_COPY) ;
}

/* a utility function to draw a line */
static void
draw_a_line(GdkGC *gc,
	    gint x1, gint y1, gint x2, gint y2, GdkColor *color, int end_shapes)
{
	gdk_gc_set_foreground(gc, color) ;

	gdk_draw_line(audio_pixmap, gc, x1,y1,x2,y2) ;

        if (x1 > SHRT_MIN && x1 < SHRT_MAX) {
            gdk_draw_line(audio_pixmap, gc, x1,y1,x2,y2) ;
	    if(end_shapes & GWC_POINT_HANDLE) {
	        draw_a_rect(gc,
			  (x1-point_handle_width/2),
			  (y1-point_handle_width/2),
			  (x1+point_handle_width/2),
			  (y1+point_handle_width/2), white_color) ;
	    }
       }
}

/* a utility function to draw a line */
static void
draw_a_cursor_line(GdkGC *gc,
	    gint x1, gint y1, gint x2, gint y2, GdkColor *color, int end_shapes)
{
	gdk_gc_set_foreground(gc, color) ;
        if (x1 > SHRT_MIN && x1 < SHRT_MAX)
 	   gdk_draw_line(highlight_pixmap, gc, x1,y1,x2,y2) ;

}

int first_pick_x, last_pick_x ;
int selecting_region = FALSE ;
int region_select_min_x = -1 ;
int region_select_max_x = -1 ;

long pixel_to_sample(struct view *v, int pixel)
{
    double p = (double)pixel / v->canvas_width ;
    double w = (v->last_sample - v->first_sample) ;
    long s = p*w + v->first_sample ;
    return s ;
}

int sample_to_pixel(struct view *v, long sample)
{
    double p = (double)(sample-v->first_sample) / (double)(v->last_sample - v->first_sample) ;
    double w = (v->canvas_width) ;
    int pix = p*w ;
    return pix ;
}

#define MAX_BUF 10000

/* in macro AtoS, a is audio level, max_a is maximum possible audio level, channel is 0 for left, 1 for right
   ch is canvas_height */

extern double view_scale ;
extern struct click_data click_data ;
struct click_data *clicks = &click_data ;

#if 0
#define AtoS(a,max_a,channel,ch) (long)(a*1000*view_scale)*(long)((ch-16)/4)/((max_a)*1000) + (long)((ch-16)/4) + (long)(channel)*ch/2+8
#endif
/* Put 8 spaces in middle for separator bar and prevent one channel from
   running over into the other */
#define AtoS(a,max_a,channel,ch) (MAX(0,MIN(ch/2-5,lrint(((a)*view_scale/(max_a)+1.0)*(ch-9)/4.0))) + channel*(ch/2+4))


GdkPixmap *wave_image = NULL ;

void draw_compressed_audio_image(struct view *v, struct sound_prefs *p, GtkWidget *da)
{
    double samples_per_pixel = (double)(v->last_sample - v->first_sample + 1) / (double)v->canvas_width ;
    double blocks_per_pixel = samples_per_pixel / (double)SBW ;
    int draw_flag = 0 ;

    long pixel ;
    int x1=0, x2=0, ly1=0, ly2=0, ry1=0, ry2=0 ;

    if(p->sample_buffer_exists == FALSE)
	fill_sample_buffer(p) ;

/*      g_print("samples per pixel = %lg\n", samples_per_pixel) ;  */

    if(v->selection_region == FALSE) {
	region_select_min_x = -1 ;
	region_select_max_x = -1 ;
    }

    p->max_value = 0.0 ;


    for(pixel = 0 ; pixel < v->canvas_width ; pixel++) {
	long first = v->first_sample + lrint(pixel*samples_per_pixel) ;
	long last = first + lrint(samples_per_pixel) ;
	double avg_left = 0, avg_right = 0 ;
	struct sample_display_block sb ;

	if(first < 0) first = 0 ;
	if(first > v->last_sample) first = v->last_sample ;
	if(last < 0) last = 0 ;
	if(last > v->last_sample) last = v->last_sample ;

/*  	d_print("data for pixel:%ld, first=%ld, last=%ld ", pixel, first, last) ;  */

	get_sample_stats(&sb, first, last, blocks_per_pixel) ;

	if(sb.max_value[0] > p->max_value) p->max_value = sb.max_value[0] ;
	if(sb.max_value[1] > p->max_value) p->max_value = sb.max_value[1] ;

	if(1 || samples_per_pixel > 50) {
#if 0
	    avg_left = sb.rms[0] ;
	    avg_right = sb.rms[1] ;
	    avg_left = avg_left/2 + sb.max_value[0]/2 ;
	    avg_right = avg_right/2 + sb.max_value[1]/2 ;
#endif
	    avg_left = sb.max_value[0] ;
	    avg_right = sb.max_value[1] ;
	} else {
	    avg_left = sb.rms[0] ;
	    avg_right = sb.rms[1] ;
	}

	ly1 = AtoS(avg_left,1.0,0,v->canvas_height) ;
	ry1 = AtoS(avg_right,1.0,1,v->canvas_height) ;

	if(1 || samples_per_pixel > 50) {
	    ly2 = AtoS(-avg_left,1.0,0,v->canvas_height) ;
	    ry2 = AtoS(-avg_right,1.0,1,v->canvas_height) ;
	}


	if(1 || samples_per_pixel > 50) {
	    x1 = pixel ;
	    x2 = pixel  ;

	    draw_a_line(da->style->black_gc, x1, ly1, x2, ly2, black_color, draw_flag) ;
	    draw_a_line(da->style->black_gc, x1, ry1, x2, ry2, black_color, draw_flag) ;
	} else if(pixel > 0) {
	    x1 = pixel ;
	    x2 = pixel - 1 ;

	    draw_a_line(da->style->black_gc, x1, ly1, x2, ly2, black_color, draw_flag) ;
	    draw_a_line(da->style->black_gc, x1, ry1, x2, ry2, black_color, draw_flag) ;
	}

	ly2 = ly1 ;
	ry2 = ry1 ;

    }
}


extern struct view audio_view ;

int _audio_area_button_event(GtkWidget *c, GdkEventButton *event)
{
    /* we ignore all events except for BUTTON_PRESS */

    if(event->type == GDK_BUTTON_PRESS) {
	first_pick_x = last_pick_x = (int)event->x ;
	selecting_region = TRUE ;
	d_print("press mx:%d my:%d\n", (int)event->x, (int)event->y) ;
	if((int)event->y < audio_view.canvas_height/4)
	    audio_view.channel_selection_mask = 0x01 ;
	else if((int)event->y > 3*audio_view.canvas_height/4)
	    audio_view.channel_selection_mask = 0x02 ;
	else
	    audio_view.channel_selection_mask = 0x03 ;
	audio_view.selection_region = FALSE ;
	main_redraw(FALSE, FALSE) ;
	display_times() ;
	return TRUE;
    }

    if(event->type == GDK_BUTTON_RELEASE) {
	region_select_min_x = MIN(first_pick_x, last_pick_x) ;
	region_select_max_x = MAX(first_pick_x, last_pick_x) ;
	region_select_min_x = MAX(region_select_min_x, 0) ;
	region_select_max_x = MIN(region_select_max_x, audio_view.canvas_width-1) ;
	if(region_select_max_x - region_select_min_x > 0) {
	    audio_view.selected_first_sample = pixel_to_sample(&audio_view, region_select_min_x) ;
	    audio_view.selected_last_sample = pixel_to_sample(&audio_view, region_select_max_x) ;
	    audio_view.selection_region = TRUE ;
	} else {
	    audio_view.selection_region = FALSE ;
	}
	selecting_region = FALSE ;
	display_times() ;
	main_redraw(FALSE, FALSE) ;
	return TRUE;
    }

    if(event->type == GDK_2BUTTON_PRESS) {
	long click_sample = pixel_to_sample(&audio_view, (int)event->x) ;
	long select_first = audio_view.first_sample ;
	long select_last = audio_view.last_sample ;

	for(int i = 0 ; i < num_song_markers ; i++) {
	    if(song_markers[i] < 0) continue ;

	    if(song_markers[i] > click_sample && song_markers[i] < select_last)
		select_last = song_markers[i] ;

	    if(song_markers[i] < click_sample && song_markers[i] > select_first)
		select_first = song_markers[i] ;
	}


	audio_view.selected_first_sample = select_first ;
	audio_view.selected_last_sample = select_last ;
	audio_view.selection_region = TRUE ;
	selecting_region = FALSE ;
	display_times() ;
	main_redraw(FALSE, FALSE) ;
	return TRUE;
    }

    if(event->type == GDK_3BUTTON_PRESS) {
	audio_view.selected_first_sample = audio_view.first_sample ;
	audio_view.selected_last_sample = audio_view.last_sample ;
	audio_view.selection_region = TRUE ;
	selecting_region = FALSE ;
	display_times() ;
	main_redraw(FALSE, FALSE) ;
	return TRUE;
    }

    return FALSE ;
}

int _audio_area_motion_event(GtkWidget *c, GdkEventMotion *event)
{
    int x, y ;
    GdkModifierType state ;

    if(event->is_hint ) {
	gdk_window_get_pointer(event->window, &x, &y, &state) ;
    } else {
	x = event->x ;
	y = event->y ;
	state = event->state ;
    }

/*      d_print("motion mx:%d my:%d\n", x, y) ;  */

    if(state & GDK_BUTTON1_MASK) {
	if(selecting_region == TRUE) {
	    long marker_pix ;
	    int i ;
	    int min_marker_dist_to_first = 10 ;
	    int min_marker_dist_to_last = 10 ;

	    last_pick_x = x ;
	    region_select_min_x = MIN(first_pick_x, last_pick_x) ;
	    region_select_max_x = MAX(first_pick_x, last_pick_x) ;
	    region_select_min_x = MAX(region_select_min_x, 0) ;
	    region_select_max_x = MIN(region_select_max_x, audio_view.canvas_width-1) ;

	    audio_view.selected_first_sample = pixel_to_sample(&audio_view, region_select_min_x) ;
	    audio_view.selected_last_sample = pixel_to_sample(&audio_view, region_select_max_x+1) - 1 ;

	    if(audio_view.selected_last_sample > audio_view.n_samples-1) audio_view.selected_last_sample = audio_view.n_samples-1 ;

	    for(i = 0 ; i < n_markers ; i++) {
		if(markers[i] < 0) continue ;

		marker_pix = sample_to_pixel(&audio_view, markers[i]) ;

		if( ABS(region_select_min_x-marker_pix) < min_marker_dist_to_first) {
		    min_marker_dist_to_first = ABS(region_select_min_x-marker_pix) ;
		    //region_select_min_x = marker_pix ;
		    audio_view.selected_first_sample = markers[i] ;
		}

		if( ABS(region_select_max_x-marker_pix) < min_marker_dist_to_last) {
		    min_marker_dist_to_last = ABS(region_select_max_x-marker_pix) ;
		    //region_select_max_x = marker_pix ;
		    audio_view.selected_last_sample = markers[i] ;
		}
	    }

	    for(i = 0 ; i < num_song_markers ; i++) {
		if(song_markers[i] < 0) continue ;

		marker_pix = sample_to_pixel(&audio_view, song_markers[i]) ;

		if( ABS(region_select_min_x-marker_pix) < min_marker_dist_to_first) {
		    min_marker_dist_to_first = ABS(region_select_min_x-marker_pix) ;
		    //region_select_min_x = marker_pix ;
		    audio_view.selected_first_sample = song_markers[i] ;
		}

		if( ABS(region_select_max_x-marker_pix) < min_marker_dist_to_last) {
		    min_marker_dist_to_last = ABS(region_select_max_x-marker_pix) ;
		    //region_select_max_x = marker_pix ;
		    audio_view.selected_last_sample = song_markers[i] ;
		}
	    }

	    audio_view.selection_region = TRUE ;

	    display_times() ;
	    main_redraw(FALSE, FALSE) ;
	    return TRUE ;
	}
    }


    /* return true as we have handled it */
    return FALSE;
}

int psk_to_color(double pwr, double max_amp, int max_color)
{
    double p = 10.0*log10(pwr/(max_amp*max_amp)) ;


    /* convert decibels  in range 0 to -120 to range 1 to 0 */
    p = (p - -80.0) / (80.0) ;

    if(p < 0.0) p = 0.0 ;
    if(p > 1.0) p = 1.0 ;
    
    return p * max_color  ;
}

int jw_psk_to_color(double pwr, double max_amp, int max_color)
{
    double p = 10.0*log10(pwr/(max_amp*max_amp)) ;


    /* convert decibels  in range 0 to -120 to range 1 to 0 */

    /* jjw version */
    p = (p - -100.0) / (100.0) ;

    if(p < 0.0) p = 0.0 ;
    if(p > 1.0) p = 1.0 ;
    
    return p * max_color  ;
}

#define sqr(x) (x)*(x)

#define FFT_MAX 8192

double spectral_amp = 1.0 ;

/* If COMBINE_MAX is set then we combine multiple frequency bins into each
   pixel with a max function else average them */
#define COMBINE_MAX

void draw_sonogram(struct view *v, struct sound_prefs *pPrefs, GtkWidget *da, double samples_per_pixel, int cursor_flag)
{
    int FFT_SIZE ;
    fftw_real left[FFT_MAX], right[FFT_MAX], out[FFT_MAX], power_spectrum[FFT_MAX/2+1], window[FFT_MAX] ;
    double merged_power_spectrum[FFT_MAX];
    double merged_power_count[FFT_MAX];
#ifdef HAVE_FFTW3
    FFTW(plan) pLeft, pRight ;
#else /* HAVE_FFTW3 */
    rfftw_plan p ;
#endif /* HAVE_FFTW3 */
    int ly1, ly2, color, k ;
    int ly1_tbl[FFT_MAX], ly2_tbl[FFT_MAX] ;
    int ly_offset[2];
    long min_sample, max_sample ;
    long sample ;
    int x ;
    int m ;
    double view_scale_save = view_scale ;
    GdkImage *image ;
    int y ;
    int spec_start, spec_stop;
    int channel;
    double spectrum_scale;
    int index_20Hz;

#define MAXSW 2000
#define MAXSH 400
    unsigned char level[2][MAXSW][MAXSH] ;

    push_status_text("Building sonogram") ;
    update_progress_bar(0.0,PROGRESS_UPDATE_INTERVAL,TRUE) ;


    /* only draw the sonogram at a scale of 1.0 */
    view_scale = 1.0 ;

      /* Is gdk_visual_get_system the correct visual? */
    image = gdk_image_new(GDK_IMAGE_FASTEST, gdk_visual_get_system(),
        v->canvas_width, v->canvas_height);
    if (image == NULL) {
       printf("Unable to create image\n");
       exit(1);
    }
    for(y = 0 ; y < v->canvas_height ; y++) {
        for(x = 0 ; x < v->canvas_width ; x++) {
            gdk_image_put_pixel(image, x, y, black_color->pixel);
        }
    }

    for(FFT_SIZE = 64 ; FFT_SIZE < FFT_MAX ; FFT_SIZE *= 2)
	if(FFT_SIZE > samples_per_pixel*2)
	    break ;
/*      g_print("Sonogram FFT_SIZE:%d\n", FFT_SIZE) ;  */

    m = FFT_SIZE/4 ;

    index_20Hz = MAX(1,(20 * FFT_SIZE + pPrefs->rate / 2) / pPrefs->rate);

       /* Normalize energy based on number of bins relative to maximum
          length FFT */
    spectrum_scale = spectral_amp * sqrt((double) FFT_SIZE / FFT_MAX) * 4.0;
#ifdef HAVE_FFTW3
    pLeft = FFTW(plan_r2r_1d)(FFT_SIZE, left, out, FFTW_R2HC, FFTW_ESTIMATE);
    pRight = FFTW(plan_r2r_1d)(FFT_SIZE, right, out, FFTW_R2HC, FFTW_ESTIMATE);
#else /* HAVE_FFTW3 */
    p = rfftw_create_plan(FFT_SIZE, FFTW_REAL_TO_COMPLEX, FFTW_ESTIMATE);
#endif /* HAVE_FFTW3 */

    for(k = 0 ; k < FFT_SIZE ; k++) {
	 window[k] = blackman(k,FFT_SIZE) ;
/*  	 window[k] = pow(window[k],1.0) ;  */
    }

    {
        double shift = (log(FFT_SIZE/2-1) + log(index_20Hz))/2;
        double scale = shift - log(index_20Hz);
        

        for(k = 1 ; k <= FFT_SIZE/2 ; k++) {
            if (sonogram_log) {
                ly1_tbl[k] = AtoS(MIN(shift - log(k),scale),scale,0,v->canvas_height) ;
                ly2_tbl[k] = AtoS(MIN(shift - log(k+1),scale),scale,0,v->canvas_height) ;
            } else {
                ly1_tbl[k] = AtoS(-(k-m-1),FFT_SIZE/4.0,0,v->canvas_height) ;
                ly2_tbl[k] = AtoS(-(k-m),FFT_SIZE/4.0,0,v->canvas_height) ;
            }
        }
    }
ly1_tbl[0] = 99999999;
ly2_tbl[0] = 99999999;

    spec_start = ly2_tbl[FFT_SIZE/2];
    spec_stop = ly1_tbl[1]-1;

    for(k = spec_start ; k <= spec_stop ; k++) {
        merged_power_count[k] = 0;
    }
    for(k = 1 ; k <= FFT_SIZE/2 ; k++) {
        merged_power_count[ly2_tbl[k]]++;
    }

    ly_offset[0] = 0;
    ly_offset[1] = AtoS(-(1-m-1),FFT_SIZE/4.0,1,v->canvas_height) - ly1_tbl[1];

    for(x = 0 ; x < v->canvas_width ; x++) {
	long mid_sample ;

        update_progress_bar((double) x / v->canvas_width,PROGRESS_UPDATE_INTERVAL,FALSE) ;

	sample = pixel_to_sample(&audio_view, x) ;

	min_sample = sample-FFT_SIZE/2 ;
	min_sample = MAX(0, min_sample) ;
	max_sample = min_sample+FFT_SIZE-1 ;

	if(max_sample > audio_view.n_samples-1)
	    max_sample = audio_view.n_samples-1 ;

	mid_sample = (max_sample-min_sample+1)/2 ;
	read_fft_real_wavefile_data(left,  right, min_sample, max_sample) ;

	for(k = 0 ; k < FFT_SIZE ; k++) {
	    left[k] *= window[k] ;
	    right[k] *= window[k] ;
	}

        for (channel = 0; channel < 2; channel++) {
#ifdef HAVE_FFTW3
            if (channel == 0)
	       FFTW(execute)(pLeft);
            else
	       FFTW(execute)(pRight);
#else /* HAVE_FFTW3 */
            if (channel == 0)
	       rfftw_one(p, left, out);
            else
	       rfftw_one(p, right, out);
#endif /* HAVE_FFTW3 */
	    power_spectrum[0] = out[0]*out[0];  /* DC component */
	    for (k = 1; k < (FFT_SIZE+1)/2; ++k)  /* (k < FFT_SIZE/2 rounded up) */
    /*  		power_spectrum[k] = out[k]*out[k] ;  */
		power_spectrum[k] = out[k]*out[k] + out[FFT_SIZE-k]*out[FFT_SIZE-k];
	    if (FFT_SIZE % 2 == 0) /* N is even */
		power_spectrum[FFT_SIZE/2] = (out[FFT_SIZE/2]*out[FFT_SIZE/2]);  /* Nyquist freq. */

	    if (FFT_SIZE/2 >= spec_stop - spec_start + 1 || sonogram_log) {
	       for(k = spec_start ; k <= spec_stop ; k++) {
#ifdef COMBINE_MAX
		   merged_power_spectrum[k] = -DBL_MAX;
#else
		   merged_power_spectrum[k] = 0.0;
#endif
	       }
	       for(k = sonogram_log ? index_20Hz : 1 ; k <= FFT_SIZE/2 ; k++) {
#ifdef COMBINE_MAX
	           if (power_spectrum[k] > merged_power_spectrum[ly1_tbl[k]])
	              merged_power_spectrum[ly1_tbl[k]] = power_spectrum[k];
#else
	           merged_power_spectrum[ly1_tbl[k]] += power_spectrum[k];
#endif
	       }
#ifndef COMBINE_MAX
	       for(k = spec_start ; k <= spec_stop ; k++) {
		   merged_power_spectrum[k] /= merged_power_count[k];
	       }
#endif
		for(k = spec_start ; k <= spec_stop ; k++) {
		    ly1 = ly1_tbl[k];
		    ly2 = ly2_tbl[k];
		    for (y = ly2 + 1; y < ly1; y++) {
			merged_power_spectrum[y] = merged_power_spectrum[ly1];
		    }
		}
	    } else {
	       for(k = 1 ; k <= FFT_SIZE/2 ; k++) {
		  ly1 = ly1_tbl[k];
		  ly2 = ly2_tbl[k];
		  for (y = ly2; y <= ly1; y++) {
		      merged_power_spectrum[y] = power_spectrum[k];
		  }
	       }
	    }

#define BOTH_DJG_JJW_NOT

#ifdef BOTH_DJG_JJW
	    for(k = spec_start ; k <= spec_stop ; k++) {
		color = psk_to_color(merged_power_spectrum[k], spectrum_scale, 255) ;
		if(color > 255) color=255 ;
		if(color < 0) color=0 ; 
		gdk_image_put_pixel(image, x, k + ly_offset[0], sonogram_color[color].pixel);
	    }

	    for(k = 1 ; k <= FFT_SIZE/2 ; k++) {
		{
		    int y ;
		    color = jw_psk_to_color(power_spectrum[k]*spectral_amp, 1.0, 255) ;
		    if(color > 255) color=255 ;
		    if(color < 0) color=0 ; 

		    for(y = ly2_tbl[k] ; y < ly1_tbl[k] ; y++) {
			gdk_image_put_pixel(image, x, y + ly_offset[1], sonogram_color[color].pixel);
		    }
		}
	    }
#else
#ifdef DJG
	    for(k = spec_start ; k <= spec_stop ; k++) {
		color = psk_to_color(merged_power_spectrum[k], spectrum_scale, 255) ;
		if(color > 255) color=255 ;
		if(color < 0) color=0 ; 
		gdk_image_put_pixel(image, x, k + ly_offset[channel], sonogram_color[color].pixel);
	    }
#else

	    for(k = 1 ; k <= FFT_SIZE/2 ; k++) {
		int y ;
		color = psk_to_color(power_spectrum[k]*spectral_amp, 1.0, 255) ;
		if(color < 0) color=0 ; 
		level[channel][x][k-1] = color ;
		if(color > 255) color=255 ;
		{ 
		    for(y = ly2_tbl[k] ; y < ly1_tbl[k] ; y++) {
			gdk_image_put_pixel(image, x, y + ly_offset[channel], sonogram_color[color].pixel);
		    }
		}
	    }
#endif

#endif
        }
    }

    gdk_draw_image(audio_pixmap, da->style->white_gc, image, 0, 0, 0, 0,
       v->canvas_width, v->canvas_height); 
    gdk_image_destroy(image);

#define DRAW_CLICKS_NOT
#ifdef DRAW_CLICKS

#define CLICK_WINDOW_SIZE 1000
#define CLICK_WINDOW_STEP 800

    int iwindow ;

    /* this goes BACKWARDS through the data */
    for(iwindow = v->last_sample; iwindow >= v->first_sample  ; iwindow -= CLICK_WINDOW_STEP) {
	int ifirst = iwindow-CLICK_WINDOW_SIZE ;
	if(ifirst < 0) ifirst=0 ;
	int window_size = iwindow-ifirst ;
	fftw_real pdata[2][CLICK_WINDOW_SIZE] ;
	fprintf(stderr, "ifirst, iwindow, %d %d\n", ifirst, iwindow) ;
	read_fft_real_wavefile_data(pdata[0],  pdata[1], ifirst, iwindow) ;
	int j ;

	for(j = 0 ; j < window_size+1 ; j++) {
	    int i = iwindow-j ;
	    for(channel = 0 ; channel < 2 ; channel++) {
		double hpf, hpf_ave, hpf_dev;
		double hpf2, hpf2_ave, hpf2_dev;

		get_hpf(i,pdata[channel],&hpf,&hpf_ave,&hpf_dev,&hpf2,&hpf2_ave,&hpf2_dev); 
		ly1 = AtoS(hpf,200.0,channel,v->canvas_height) ;
		ly2 = AtoS(hpf2,200.0,channel,v->canvas_height) ;
		int x = sample_to_pixel(v, i) ;
		draw_a_line(da->style->white_gc, x, ly1, x, ly2, green_color, 0) ;
	    }
	}
    }
#endif /* DRAW_FFT_CLICKS */

#ifdef DRAW_FFT_CLICKS
    for(channel = 0 ; channel < 2 ; channel++) {
	double mean_level[MAXSH] ;
	double offset[MAXSW] ;
	double hgt_sum ;
	double mean, std_err, var, cv, stddev ;
	long click_start, click_end ;
	int in_click ;

	for(k = 0 ; k < FFT_SIZE/2 ; k++) {
	    mean_level[k] = 0.0 ;
	    for(x = 0 ; x < v->canvas_width ; x++)
		mean_level[k] += level[channel][x][k] ;
	    mean_level[k] /= v->canvas_width ;
	}

	for(x = 0 ; x < v->canvas_width ; x++) {
	    offset[x] = 0.0 ;
	    for(k = 0 ; k < FFT_SIZE/2 ; k++) {
		offset[x] += level[channel][x][k] - mean_level[k] ;
	    }

	    offset[x] /= (double)FFT_SIZE/2.0 ;
	    if(offset[x] < 0.0) offset[x] = 0.0 ;

	}

	stats(offset, v->canvas_width, &mean, &std_err, &var, &cv, &stddev) ;

	in_click = 0 ;

	for(x = 0 ; x < v->canvas_width-1 ; x++) {
	    double z = (offset[x]-mean)/stddev ;
	    double z1 = (offset[x+1]-mean)/stddev ;

	    if(z < 0.0) z = 0.0 ;
	    if(z1 < 0.0) z1 = 0.0 ;

	    ly1 = AtoS(-40.0*z,200.0,channel,v->canvas_height) ;
	    ly2 = AtoS(-40.0*z1,200.0,channel,v->canvas_height) ;

	    if(z > 1.e-30) {
		if(!in_click) {
		    in_click = 1 ;
		    click_start = x ;
		    hgt_sum = z ;
		} else {
		    hgt_sum += z ;
		}
	    } else {
		if(in_click) {
		    double click_width = ((x-1) - click_start+1) ;
		    int click_mid = (click_start + click_width/2+0.5) ;
		    int y1,y2 ;

		    hgt_sum /= click_width ;
		    //printf("%d %lg\n", click_mid, hgt_sum) ;

		    y1 = AtoS(0.0,200.0,channel,v->canvas_height) ;
		    y2 = AtoS(-40.0*hgt_sum,200.0,channel,v->canvas_height) ;
		    draw_a_line(da->style->white_gc, click_mid-1, y1, click_mid-1, y2, green_color, 0) ;
		    draw_a_line(da->style->white_gc, click_mid, y1, click_mid, y2, green_color, 0) ;
		    draw_a_line(da->style->white_gc, click_mid+1, y1, click_mid+1, y2, green_color, 0) ;
		}
		in_click = 0 ;
	    }

	    draw_a_line(da->style->white_gc, x, ly1, x+1, ly2, green_color, 0) ;
	}
    }
#endif /* DRAW_FFT_CLICKS */

#ifdef HAVE_FFTW3
    FFTW(destroy_plan)(pLeft);
    FFTW(destroy_plan)(pRight);
#else /* HAVE_FFTW3 */
    rfftw_destroy_plan(p);
#endif /* HAVE_FFTW3 */

    update_progress_bar(0.0,PROGRESS_UPDATE_INTERVAL,TRUE) ;
    pop_status_text();

#ifdef TEST_DRAW_CLICK_FINDER
    in_click = 0 ;

#define N 50
    {
	double mean_right, stderr_right, var_right, cv_right, stddev_right ;

	stats(&right_hpf[0], v->canvas_width, &mean_right, &stderr_right, &var_right, &cv_right, &stddev_right) ;

	d_print("mean_right:%g\n", mean_right) ;

	/* draw 2 reference lines, for mean height and z values on first derivative */
	ly1 = AtoS(100-ABS(2.0*mean_right),200.0,1,v->canvas_height) ;
	draw_a_line(da->style->white_gc, 0, ly1, v->canvas_width-1, ly1, green_color, 0) ;

	ly1 = AtoS(2.5-ABS(1.0),5.0,1,v->canvas_height) ;
	draw_a_line(da->style->white_gc, 0, ly1, v->canvas_width-1, ly1, red_color, 0) ;
    }
#endif

    gdk_gc_set_foreground(da->style->white_gc, white_color) ;
    gdk_gc_set_foreground(da->style->black_gc, black_color) ;
	
    view_scale =  view_scale_save ;
}

void paint_screen_with_highlight(struct view *v, GtkWidget *da, int y1, int y2, int cursor_flag)
{
    int x = sample_to_pixel(v, v->cursor_position) ;
    GdkGC *MyGC = gdk_gc_new(da->window) ;

    gdk_draw_pixmap(highlight_pixmap,
	   da->style->fg_gc[GTK_WIDGET_STATE (da)],
	   audio_pixmap,
	   0,0,
	   0,0,
	   v->canvas_width, v->canvas_height);

    if(v->selection_region == TRUE) {
	gint minx = region_select_min_x ;
	gint maxx = region_select_max_x ;
	if(audio_view.channel_selection_mask == 0x01)
	    draw_a_highlight_rect(MyGC, minx, 0, maxx, v->canvas_height/2, highlight_color) ;
	else if(audio_view.channel_selection_mask == 0x02)
	    draw_a_highlight_rect(MyGC, minx, v->canvas_height/2, maxx, v->canvas_height-1, highlight_color) ;
	else
	    draw_a_highlight_rect(MyGC, minx, 0, maxx, v->canvas_height-1, highlight_color) ;
    }
#ifdef TRUNCATE_OLD
    if(v->truncate_head > 0) {
	gint maxx = sample_to_pixel(v, v->truncate_head) ;
	if(maxx > 0) {
	    gint minx = sample_to_pixel(v, 0) ;
	    if(minx < 0) minx = 0 ;
	    draw_a_highlight_rect(MyGC, minx, 0, maxx, v->canvas_height-1, cut_highlight_color) ;
	}
    }

    if(v->truncate_tail < v->n_samples-1) {
	gint minx = sample_to_pixel(v, v->truncate_tail) ;
	if(minx < v->canvas_width) {
	    gint maxx = sample_to_pixel(v, v->n_samples-1) ;
	    if(maxx > v->canvas_width-1) maxx = v->canvas_width-1 ;
	    draw_a_highlight_rect(MyGC, minx, 0, maxx, v->canvas_height-1, cut_highlight_color) ;
	}
    }
#endif /* TRUNCATE_OLD */
    if(cursor_flag == TRUE) {
/*  	draw_a_cursor_line(MyGC, x-4, y1+4, x-4, y2-4, yellow_color, 0) ;  */
/*  	draw_a_cursor_line(MyGC, x-3, y1+3, x-3, y2-3, yellow_color, 0) ;  */
/*  	draw_a_cursor_line(MyGC, x-2, y1+2, x-2, y2-2, yellow_color, 0) ;  */
/*  	draw_a_cursor_line(MyGC, x-1, y1+1, x-1, y2-1, yellow_color, 0) ;  */
/*  	draw_a_cursor_line(MyGC, x+0, y1+0, x+0, y2+0, yellow_color, 0) ;  */
        last_cursor_x = x;
        /**************** This is also in redraw *******************/
        gdk_draw_pixmap(cursor_pixmap,
	   da->style->fg_gc[GTK_WIDGET_STATE (da)],
	   highlight_pixmap,
	   x,0,
	   0,0,
	   1, v->canvas_height);
	draw_a_cursor_line(MyGC, x+0, 0, x+0, v->canvas_height-1, yellow_color, 0) ;
/*  	draw_a_cursor_line(MyGC, x+1, y1+1, x+1, y2-1, yellow_color, 0) ;  */
/*  	draw_a_cursor_line(MyGC, x+2, y1+2, x+2, y2-2, yellow_color, 0) ;  */
/*  	draw_a_cursor_line(MyGC, x+3, y1+3, x+3, y2-3, yellow_color, 0) ;  */
/*  	draw_a_cursor_line(MyGC, x+4, y1+4, x+4, y2-4, yellow_color, 0) ;  */
    } else {
       last_cursor_x = -1;   
    }

    gdk_draw_pixmap(da->window,
	   da->style->fg_gc[GTK_WIDGET_STATE (da)],
	   highlight_pixmap,
	   0,0,
	   0,0,
	   v->canvas_width, v->canvas_height);
    gdk_gc_unref(MyGC) ;
}
    
void redraw(struct view *v, struct sound_prefs *p, GtkWidget *da, int cursor_flag, int redraw_data, int sonogram_flag)
{
/*  double left[MAX_BUF], right[MAX_BUF] ; */
    fftw_real left[MAX_BUF], right[MAX_BUF] ;
    long i, n ;
    int pixels_per_sample = v->canvas_width / (v->last_sample - v->first_sample + 1) ;
    double samples_per_pixel = (double)(v->last_sample - v->first_sample + 1) / (double)v->canvas_width ;
    int draw_flag = 0 ;
    int zero_left, zero_right ;
    int y1 = v->canvas_height/2.0 - 4;
    int y2 = y1 + 8 ;
    static int first_time = 1 ;
    static GdkColormap *my_map ;
    GdkGC *MyGC ;

    zero_left = AtoS(0,1.0,0,v->canvas_height) ;
    zero_right = AtoS(0,1.0,1,v->canvas_height) ;

    if(v->selection_region == FALSE) {
	region_select_min_x = -1 ;
	region_select_max_x = -1 ;
    } else {
	if(v->selection_region == TRUE) {
	    region_select_min_x = sample_to_pixel(v, v->selected_first_sample) ;
	    region_select_max_x = sample_to_pixel(v, v->selected_last_sample) ;
	}
    }

    if(cursor_flag != TRUE) {
	MyGC = gdk_gc_new(da->window) ;
    } else {
	if(v->cursor_position >= v->first_sample && v->cursor_position <= v->last_sample) {
	    int prev_x = sample_to_pixel(v, v->prev_cursor_position) ;
	    int x = sample_to_pixel(v, v->cursor_position) ;
	    if(x != prev_x) {
	       MyGC = gdk_gc_new(da->window) ;
               if (last_cursor_x == -1) {
		   paint_screen_with_highlight(v, da, y1, y2, cursor_flag) ;
               } else {
                   int x = sample_to_pixel(v, v->cursor_position) ;

                   /****** This is also in  paint_screen_with_highlight *******/

                   gdk_draw_pixmap(highlight_pixmap,
	               da->style->fg_gc[GTK_WIDGET_STATE (da)],
	               cursor_pixmap,
	               0,0,
	               last_cursor_x,0,
	               1, v->canvas_height);
                   gdk_draw_pixmap(da->window,
	               da->style->fg_gc[GTK_WIDGET_STATE (da)],
	               cursor_pixmap,
	               0,0,
	               last_cursor_x,0,
	               1, v->canvas_height);
                   gdk_draw_pixmap(cursor_pixmap,
	               da->style->fg_gc[GTK_WIDGET_STATE (da)],
	               highlight_pixmap,
	               x,0,
	               0,0,
	               1, v->canvas_height);

	           draw_a_cursor_line(MyGC, x+0, 0, x+0, v->canvas_height-1, yellow_color, 0) ;
                   gdk_draw_pixmap(da->window,
	               da->style->fg_gc[GTK_WIDGET_STATE (da)],
	               highlight_pixmap,
	               x,0,
	               x,0,
	               1, v->canvas_height);
                   last_cursor_x = x;
               }
	       gdk_gc_unref(MyGC) ;
	    } else {
	    }
	    v->prev_cursor_position = v->cursor_position ;
	}
	return ;
    }

    if(first_time == 1) {
	int i ;
	first_time = 0 ;
/*  	my_visual = gdk_visual_get_best() ;  */
/*  	my_map = gdk_colormap_new(my_visual, FALSE) ;  */
	my_map = gdk_colormap_get_system() ;
	gdk_colormap_alloc_color(my_map, black_color, FALSE, TRUE) ;
	gdk_colormap_alloc_color(my_map, white_color, FALSE, TRUE) ;
	gdk_colormap_alloc_color(my_map, grey_color, FALSE, TRUE) ;
	gdk_colormap_alloc_color(my_map, red_color, FALSE, TRUE) ;
	gdk_colormap_alloc_color(my_map, green_color, FALSE, TRUE) ;
	gdk_colormap_alloc_color(my_map, blue_color, FALSE, TRUE) ;
	gdk_colormap_alloc_color(my_map, yellow_color, FALSE, TRUE) ;
	gdk_colormap_alloc_color(my_map, orange_color, FALSE, TRUE) ;

#define JJW_CMAP_NOT
#ifdef JJW_CMAP
	for(i = 0 ; i < 256 ; i++) {
	    double d = i ;
	    double p_red = (d-105.0)/150.0 ;
	    double p_green = (d-200.0)/55.0 ;
	    double p_blue = d/255.0 ;

	    if(i > 100) p_blue = 1.0 - (d-100.0)/20.0 ;
/*  	    if(i > 128 && i < 192) p_blue = 1.0 - (d-128.0)/64.0 ;  */
/*  	    if(i > 192) p_blue = (d-192.0)/64.0 ;  */

	    if(0){
		double c[5][3] = { {0.0,0.0,0.0},
				    {0.0,0.0,0.5},
				    {0.5,0.0,1.0},
				    {1.0,0.5,1.0},
				    {1.0,1.0,1.0} } ;
		double p0, p1,d ;
		int ilow ;

		int breakpt[5] = {0,51,102,204,255} ;

		for(ilow = 0 ; ilow < 4 ; ilow++)
		    if(breakpt[ilow+1] > i)
			break ;

		d = breakpt[ilow+1] - breakpt[ilow] ;
		p1 = (double)(i-breakpt[ilow])/d ;
		p0 = 1.0 - p1 ;

		p_red = c[ilow][0]*p0 + c[ilow+1][0]*p1 ;
		p_green = c[ilow][1]*p0 + c[ilow+1][1]*p1 ;
		p_blue = c[ilow][2]*p0 + c[ilow+1][2]*p1 ;
	    }

	    p_red = MIN(1.0, p_red) ;
	    p_green = MIN(1.0, p_green) ;
	    p_blue = MIN(1.0, p_blue) ;

	    p_red = MAX(0.0, p_red) ;
	    p_green = MAX(0.0, p_green) ;
	    p_blue = MAX(0.0, p_blue) ;

	    sonogram_color[i].red = 65535*p_red/2.0 ;
	    sonogram_color[i].green = 65535*p_green/2.0 ;
	    sonogram_color[i].blue = 65535*p_blue/2.0 ;

	    gdk_colormap_alloc_color(my_map, &sonogram_color[i], FALSE, TRUE) ;
	}
#else
#define PASTEL
#define WHITE_HOT
	for(i = 0 ; i < 256 ; i++) {
	    double d = i ;
	    double p_red = 0.0;
	    double p_green = 0.0;
	    double p_blue = 0.0;
            double gamma_fact = 1/1.5;

            p_blue = d / 85.0;

            if (i > 85) {
	        p_red = (d - 85.0) / (85.0) ;
                p_blue = (170.0 - d) / 85.0;
            }

            if (i > 170) {
	        p_red = (255.0 - d) / 85.0 ;
	        p_green = (d - 170.0) / (50.0) ;
#ifdef WHITE_HOT
		p_red = MAX(p_red, p_green) ;
#endif
            }

#ifdef WHITE_HOT
            if (i > 220) {
	       p_blue = (d-220.0) / 35.0 ;
            }
#endif

	    p_red = MIN(1.0, p_red) ;
	    p_green = MIN(1.0, p_green) ;
	    p_blue = MIN(1.0, p_blue) ;

	    p_red = MAX(0.0, p_red) ;
	    p_green = MAX(0.0, p_green) ;
	    p_blue = MAX(0.0, p_blue) ;

#ifdef PASTEL
	    /* desaturate the colors a bit... */
	    {
		double max_v = MAX(p_red, MAX(p_green, p_blue)) ;
		double sum_v = p_red + p_green + p_blue ;

		if(sum_v < 1.0) {
		    double ds = (1.0-sum_v)*max_v ;
		    p_red += ds ;
		    p_green += ds ;
		    /* p_blue += ds ; */
		}
	    }
               
	    p_red = MIN(1.0, p_red) ;
	    p_green = MIN(1.0, p_green) ;
	    p_blue = MIN(1.0, p_blue) ;
#endif

	    sonogram_color[i].red = 65535*pow(p_red, gamma_fact) ;
	    sonogram_color[i].green = 65535*pow(p_green, gamma_fact) ;
	    sonogram_color[i].blue = 65535*pow(p_blue, gamma_fact) ;
#ifdef INVERT_SONOGRAM
	    sonogram_color[i].red = 65535 - sonogram_color[i].red ;
	    sonogram_color[i].green = 65535 - sonogram_color[i].green ;
	    sonogram_color[i].blue = 65535 - sonogram_color[i].blue ;
#endif

	    gdk_colormap_alloc_color(my_map, &sonogram_color[i], FALSE, TRUE) ;
	}
#endif
	   /* Some 8 bit displays use 0 for white so XOR doesn't highlight
	    * selection */
	if (highlight_color->pixel == 0)
	   highlight_color = black_color;
    }


    if(sonogram_flag == TRUE) {
	if (redraw_data)
	    draw_sonogram(v, p, da, samples_per_pixel, cursor_flag)   ;
        gdk_draw_rectangle(audio_pixmap, MyGC, TRUE,
    		0, y1, v->canvas_width-1, 8) ;
	paint_screen_with_highlight(v, da, y1, y2, cursor_flag) ;
    } else if (redraw_data) {
	/* clear the background to grey */
	draw_a_rect(MyGC, 0, 0, v->canvas_width-1, v->canvas_height-1, grey_color) ;

	draw_a_rect(MyGC, 0, y1, v->canvas_width-1, y2, black_color) ;

	for(i = 0 ; i < clicks->n_clicks ; i++) {
	    int x1 = sample_to_pixel(v, clicks->start[i]) ;
	    int x2 = sample_to_pixel(v, clicks->end[i]) ;
	    int y1 = AtoS(-1.0,1.0,clicks->channel[i],v->canvas_height) ;
	    int y2 = AtoS(1.0,1.0,clicks->channel[i],v->canvas_height) ;
    /*  	g_print("Click %s, %ld to %ld\n", clicks->channel[i] ? "R" : "L", clicks->start[i], clicks->end[i]) ;  */
	    draw_a_rect(MyGC, x1, y1, x2, y2, red_color) ;
	}

	p->max_value = 0.0 ;
	if(pixels_per_sample >= 1) {
	    double samp_width = 1./samples_per_pixel ;

	    if(pixels_per_sample > 8)
		draw_flag = GWC_POINT_HANDLE ;

	    n = read_fft_real_wavefile_data(left, right, v->first_sample, v->last_sample) ;

	    for(i = 0 ; i < n-1 ; i++) {
		int x1, x2, ly1, ly2, ry1, ry2 ;
		if(ABS(left[i]) > p->max_value) p->max_value = ABS(left[i]) ;
		if(ABS(right[i]) > p->max_value) p->max_value = ABS(right[i]) ;
		ly1 = AtoS(left[i],1.0,0,v->canvas_height) ;
		ly2 = AtoS(left[i+1],1.0,0,v->canvas_height) ;
		ry1 = AtoS(right[i],1.0,1,v->canvas_height) ;
		ry2 = AtoS(right[i+1],1.0,1,v->canvas_height) ;

		x1 = (int)((double)i*samp_width+0.5) + point_handle_width/2 ;
		x2 = x1 + pixels_per_sample ;

		draw_a_line(MyGC, x1, ly1, x2, ly2, black_color, draw_flag) ;
		draw_a_line(MyGC, x1, ry1, x2, ry2, black_color, draw_flag) ;
	    }
    /*  	d_print("MAX VALUE is %ld.\n", p->max_value) ;  */
	} else {
	    draw_compressed_audio_image(v, p, da) ;

	}
	draw_a_line(MyGC, 0, zero_left, v->canvas_width-1, zero_left, blue_color, 0) ;
	draw_a_line(MyGC, 0, zero_right, v->canvas_width-1, zero_right, blue_color, 0) ;

	int n_view_samples = v->last_sample-v->first_sample+1 ;

	if(n_view_samples < 20000) {
	    int x ;
	    /* draw the RMSE of samples in a green line */
	    double last_left_rmse = -1.0 ;
	    double last_right_rmse = -1.0 ;
	    double left_rmse = -1 ;
	    double right_rmse = -1 ;
	    double samp_width = 1./samples_per_pixel ;

	    for(x = 0 ; x < v->canvas_width ; x++) {
		int s_at_x = (double)x / (double)(v->canvas_width-1) * n_view_samples + v->first_sample ;
		int first = MAX(s_at_x-50, 0) ;
		int last = MIN(s_at_x+50, v->n_samples-1) ;
		n = read_fft_real_wavefile_data(left, right, first, last) ;

		left_rmse = 0 ; /* first use these for sums */
		right_rmse = 0 ;
		for(i = 0 ; i < n-1 ; i++) {
		    left_rmse += left[i] * left[i] ;
		    right_rmse += right[i] * right[i] ;
		}

		left_rmse = sqrt(left_rmse/((double)n+1.e-30)) ;
		right_rmse = sqrt(right_rmse/((double)n+1.e-30)) ;

		if(last_left_rmse > -1.0) {
		    int x1, x2, ly1, ly2, ry1, ry2 ;

		    ly1 = AtoS(-last_left_rmse,1.0,0,v->canvas_height) ;
		    ly2 = AtoS(-left_rmse,1.0,0,v->canvas_height) ;
		    ry1 = AtoS(-last_right_rmse,1.0,1,v->canvas_height) ;
		    ry2 = AtoS(-right_rmse,1.0,1,v->canvas_height) ;

		    x1 = x ;
		    x2 = x1 + 1 ;

		    draw_a_line(MyGC, x1, ly1, x2, ly2, green_color, draw_flag) ;
		    draw_a_line(MyGC, x1, ry1, x2, ry2, green_color, draw_flag) ;
		}
		last_left_rmse = left_rmse ;
		last_right_rmse = right_rmse ;
	    }
	}

    }

    draw_a_rect(MyGC, 0, y1, v->canvas_width-1, y1+8, black_color) ;

    for(i = 0 ; i < n_markers ; i++) {
	if(markers[i] >= v->first_sample && markers[i] <= v->last_sample) {
	    int x = sample_to_pixel(v, markers[i]) ;
	    draw_a_line(MyGC, x, 0, x, v->canvas_height-1, blue_color, 0) ;
	}
    }

    { 
	for (i = 0; i < num_song_markers; i++) {
	    if(song_markers[i] >= v->first_sample && song_markers[i] <= v->last_sample) {
		gint x = sample_to_pixel(v, song_markers[i]) ;
		draw_a_line(MyGC, x, 0, x, v->canvas_height-1, orange_color, 0) ;
	    }
	}
    }

/*      if(v->selection_region == TRUE) {  */
	paint_screen_with_highlight(v, da, y1, y2, cursor_flag) ;
/*      } else {  */
/*  	rect.x = 0 ;  */
/*  	rect.y = 0 ;  */
/*  	rect.width = v->canvas_width ;  */
/*  	rect.height = v->canvas_height ;  */
/*  	gtk_widget_draw(da, &rect) ;  */
/*      }  */

    gdk_gc_set_foreground(da->style->white_gc, white_color) ;
    gdk_gc_set_foreground(da->style->black_gc, black_color) ;
    gdk_gc_unref(MyGC) ;
}
