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

/* dethunk.c */

#include <stdlib.h>
#include <gnome.h>
#include "gtkledbar.h"
#include "gwc.h"
#include "stat.h"

#define FFT_MAX 32768

void print_spectral(char *str, fftw_real tmp_l[], long FFT_SIZE)
{
    fftw_real tmp[FFT_MAX], amp[FFT_MAX], phase[FFT_MAX] ;
    long k ;

    double hdfs = FFT_SIZE / 2 ;

    return ;

    /* bias */
    k = 0 ;
    amp[0] = tmp_l[0]/hdfs ;
    phase[0] = 0 ;
    printf("%s: %ld %10lg %10lg\n", str, k, amp[k], phase[k]) ;

    for(k = 1 ; k < FFT_SIZE ; k++) {
	tmp[k] = tmp_l[k] / hdfs ;
    }

    /* convert noise sample to power spectrum */
    for(k = 1 ; k <= FFT_SIZE/2 ; k++) {
	if(k < FFT_SIZE/2) {
	    amp[k] = tmp[k] * tmp[k] + tmp[FFT_SIZE-k]*tmp[FFT_SIZE-k] ;
	    phase[k] = atan2(tmp[FFT_SIZE-k],tmp[k]);
	} else {
	    /* Nyquist Frequency */
	    amp[k] = tmp[k] * tmp[k] ;
	    phase[k] = 0.0 ;
	}
	amp[k] = sqrt(amp[k]) ;
	printf("%s: %ld %10lg %10lg\n", str, k, amp[k], phase[k]) ;
    }
}

int dethunk_new(struct sound_prefs *pPrefs,
            long first_sample, long last_sample, int channel_mask) 
{
    long i ;
    long n_samples = last_sample - first_sample + 1 ;
    int cancel, k ;
    fftw_real left[FFT_MAX], right[FFT_MAX] ;
    fftw_real pre_left[FFT_MAX] ;
    fftw_real pre_right[FFT_MAX] ;
    fftw_real post_left[FFT_MAX] ;
    fftw_real post_right[FFT_MAX] ;
    fftw_real window_coef[FFT_MAX] ;
    fftw_real windowed[FFT_MAX] ;
#ifdef HAVE_FFTW3
    fftw_real tmp_l[FFT_MAX] ;
    fftw_real tmp_r[FFT_MAX] ;
    FFTW(plan) pPreLeft, pPreRight, pPostLeft, pPostRight, pBakLeft, pBakRight;
#else /* HAVE_FFTW3 */
    rfftw_plan pFor,pBak ;
#endif /* HAVE_FFTW3 */
    double dfs, hdfs ;
    extern struct view audio_view ;
    int FFT_SIZE ;
    int repair_size ;
    int window, n_windows ;
    int n_want = 4 ;

    for(FFT_SIZE = 8 ; FFT_SIZE < n_samples/n_want && FFT_SIZE < 8192 ; FFT_SIZE *= 2) ;

    repair_size = FFT_SIZE * n_want ;
    n_windows = 2*n_want - 1 ;

    dfs = FFT_SIZE ;
    hdfs = FFT_SIZE / 2 ;

    {
	long extra_samples = repair_size - n_samples ;
	first_sample -= extra_samples/2 ;
	if(first_sample < 0) first_sample = 0 ;
	last_sample = first_sample+repair_size-1 ;
	if(last_sample > pPrefs->n_samples-1) {
	    last_sample = pPrefs->n_samples-1 ;
	    first_sample = last_sample-repair_size-1 ;
	}
    }

    push_status_text("Saving undo information") ;
    start_save_undo("Undo dethunk", &audio_view) ;
    cancel = save_undo_data( first_sample, last_sample, pPrefs, TRUE) ;
    close_undo() ;
    pop_status_text() ;

    n_samples = last_sample - first_sample + 1 ;

    push_status_text("Dethunking audio") ;
    update_status_bar(0.0,STATUS_UPDATE_INTERVAL,TRUE) ;

    g_print("FFTSIZE:%d repair_size:%d\n", FFT_SIZE, repair_size) ;

#ifdef HAVE_FFTW3
    pPreLeft   = FFTW(plan_r2r_1d)(FFT_SIZE, left, pre_left, FFTW_R2HC, FFTW_ESTIMATE);
    pPreRight  = FFTW(plan_r2r_1d)(FFT_SIZE, right, pre_right, FFTW_R2HC, FFTW_ESTIMATE);
    pPostLeft  = FFTW(plan_r2r_1d)(FFT_SIZE, left, post_left, FFTW_R2HC, FFTW_ESTIMATE);
    pPostRight = FFTW(plan_r2r_1d)(FFT_SIZE, right, post_right, FFTW_R2HC, FFTW_ESTIMATE);
    pBakLeft   = FFTW(plan_r2r_1d)(FFT_SIZE, tmp_l, windowed, FFTW_HC2R, FFTW_ESTIMATE);
    pBakRight  = FFTW(plan_r2r_1d)(FFT_SIZE, tmp_r, windowed, FFTW_HC2R, FFTW_ESTIMATE);
#else /* HAVE_FFTW3 */
    pFor = rfftw_create_plan(FFT_SIZE, FFTW_REAL_TO_COMPLEX, FFTW_ESTIMATE);
    pBak = rfftw_create_plan(FFT_SIZE, FFTW_COMPLEX_TO_REAL, FFTW_ESTIMATE);
#endif /* HAVE_FFTW3 */
    audio_normalize(0) ;

    for(k = 0 ; k < FFT_SIZE ; k++) {
	double p = ((double)(k))/(double)(FFT_SIZE) ;
	window_coef[k] = 0.5 - 0.5 * cos(2.0*M_PI*p) ;
/*  	printf("window_coef: %ld %10lg\n", k, window_coef[k]) ;  */
/*  	double hanning(int, int) ;  */
/*  	window_coef[k] = hanning(k,FFT_SIZE) ;  */
    }

    /* get fft of samples ahead of thunk */
    {
	long first = first_sample-FFT_SIZE ;
	if(first < 0) first = 0 ;
	read_fft_real_wavefile_data(left,  right, first, first+FFT_SIZE-1) ;

/*  	for(k = 0 ; k < FFT_SIZE ; k++) {  */
/*  	    left[k] *= window_coef[k] ;  */
/*  	    right[k] *= window_coef[k] ;  */
/*  	}  */

#ifdef HAVE_FFTW3
	FFTW(execute)(pPreLeft);
	FFTW(execute)(pPreRight);
#else /* HAVE_FFTW3 */
	rfftw_one(pFor, left, pre_left);
	rfftw_one(pFor, right, pre_right);
#endif /* HAVE_FFTW3 */
    }

    /* get fft of samples behind thunk */
    {
	long first = last_sample+1 ;
	if(first+FFT_SIZE-1 > pPrefs->n_samples-1) first = pPrefs->n_samples-FFT_SIZE ;
	read_fft_real_wavefile_data(left,  right, first, first+FFT_SIZE-1) ;

/*  	for(k = 0 ; k < FFT_SIZE ; k++) {  */
/*  	    left[k] *= window_coef[k] ;  */
/*  	    right[k] *= window_coef[k] ;  */
/*  	}  */

#ifdef HAVE_FFTW3
	FFTW(execute)(pPostLeft);
	FFTW(execute)(pPostRight);
#else /* HAVE_FFTW3 */
	rfftw_one(pFor, left, post_left);
	rfftw_one(pFor, right, post_right);
#endif /* HAVE_FFTW3 */
    }

    read_fft_real_wavefile_data(left,  right, first_sample, first_sample+repair_size-1) ;

    for(i = FFT_SIZE/2 ; i < repair_size-1-FFT_SIZE/2 ; i++) {
	if(channel_mask & 0x01)
	    left[i] = 0 ;

	if(channel_mask & 0x02)
	    right[i] = 0 ;
    }

    for(i = 0 ; i < FFT_SIZE/2 ; i++) {
	if(channel_mask & 0x01) {
	    left[i] *= window_coef[i+FFT_SIZE/2] ;
	}

	if(channel_mask & 0x02) {
	    right[i] *= window_coef[i+FFT_SIZE/2] ;
	}
    }

    for(i = FFT_SIZE/2 ; i < FFT_SIZE ; i++) {
	int j = i - FFT_SIZE/2 ;
	int sample = repair_size-FFT_SIZE/2-1 + j;
	if(channel_mask & 0x01) {
	    left[sample] *= window_coef[j] ;
	}

	if(channel_mask & 0x02) {
	    right[sample] *= window_coef[j] ;
	}
    }

    for(window = 0 ; window < n_windows ; window++) {
/*  	double wgt_post = (double)window/(double)(n_windows-1) ;  */
	double wgt_post = 0.0 ;
	double wgt_pre = 1.0 - wgt_post ;
	long i = window * FFT_SIZE/2 ;
#ifndef HAVE_FFTW3
	fftw_real tmp_l[FFT_MAX] ;
	fftw_real tmp_r[FFT_MAX] ;
#endif /* not HAVE_FFTW3 */

	for(k = 0 ; k < FFT_SIZE ; k++) {
	    tmp_l[k] = wgt_pre*pre_left[k] + wgt_post*post_left[k] ;
	    tmp_r[k] = wgt_pre*pre_right[k] + wgt_post*post_right[k] ;
	}

	if(channel_mask & 0x01) {
	    double offset ;
	    double hs1 ;

	    /* the inverse fft */
#ifdef HAVE_FFTW3
	    FFTW(execute)(pBakLeft);
#else /* HAVE_FFTW3 */
	    rfftw_one(pBak, tmp_l, windowed);
#endif /* HAVE_FFTW3 */

	    if(0) {
		/* make sure the tails of the sample approach zero magnitude */
		offset = windowed[0] ;
		hs1 = FFT_SIZE/2 - 1 ;

		for(k = 0 ; k < FFT_SIZE/2 ; k++) {
		    double p = (hs1-(double)k)/hs1 ;
		    windowed[k] -= offset*p ;
		}

		offset = windowed[FFT_SIZE-1] ;
		for(k = FFT_SIZE/2 ; k < FFT_SIZE ; k++) {
		    double p = ((double)k-hs1)/hs1 ;
		    windowed[k] -= offset*p ;
		}
	    } else {
		for(k = 0 ; k < FFT_SIZE ; k++) {
		    windowed[k] *= window_coef[k] ;
/*  		    windowed[k] = window_coef[k] * 32000*FFT_SIZE ;  */
		}
	    }

	    for(k = 0 ; k < FFT_SIZE ; k++)
		left[i+k] += windowed[k] / (double)(FFT_SIZE) ;
	}

	if(channel_mask & 0x02) {
	    double offset ;
	    double hs1 ;

	    /* the inverse fft */
#ifdef HAVE_FFTW3
	    FFTW(execute)(pBakRight);
#else /* HAVE_FFTW3 */
	    rfftw_one(pBak, tmp_r, windowed);
#endif /* HAVE_FFTW3 */

	    if(0) {
		/* make sure the tails of the sample approach zero magnitude */
		offset = windowed[0] ;
		hs1 = FFT_SIZE/2 - 1 ;

		for(k = 0 ; k < FFT_SIZE/2 ; k++) {
		    double p = (hs1-(double)k)/hs1 ;
		    windowed[k] -= offset*p ;
		}

		offset = windowed[FFT_SIZE-1] ;
		for(k = FFT_SIZE/2 ; k < FFT_SIZE ; k++) {
		    double p = ((double)k-hs1)/hs1 ;
		    windowed[k] -= offset*p ;
		}
	    } else {
		for(k = 0 ; k < FFT_SIZE ; k++) {
		    windowed[k] *= window_coef[k] ;
/*  		    windowed[k] = -window_coef[k] * 32000*FFT_SIZE ;  */
		}
	    }

	    for(k = 0 ; k < FFT_SIZE ; k++)
		right[i+k] += windowed[k] / (double)(FFT_SIZE) ;
	}

    }
	    
    write_fft_real_wavefile_data(left, right, first_sample, last_sample) ;

#ifdef HAVE_FFTW3
    FFTW(destroy_plan)(pPreLeft);
    FFTW(destroy_plan)(pPreRight);
    FFTW(destroy_plan)(pPostLeft);
    FFTW(destroy_plan)(pPostRight);
    FFTW(destroy_plan)(pBakLeft);
    FFTW(destroy_plan)(pBakRight);
#else /* HAVE_FFTW3 */
    rfftw_destroy_plan(pFor);
    rfftw_destroy_plan(pBak);
#endif /* HAVE_FFTW3 */

    update_status_bar(0.0,STATUS_UPDATE_INTERVAL,TRUE) ;

    pop_status_text() ;

    audio_normalize(1) ;

    main_redraw(FALSE, TRUE) ;

    return 0 ;
}

int dethunk_current(struct sound_prefs *pPrefs,
            long first_sample, long last_sample, int channel_mask)
{
    long i ;
    long n_samples = last_sample - first_sample + 1 ;
    int cancel, k ;
    fftw_real left[FFT_MAX], right[FFT_MAX] ;
    fftw_real pre_left_amp[FFT_MAX], pre_left_phase[FFT_MAX] ;
    fftw_real pre_right_amp[FFT_MAX], pre_right_phase[FFT_MAX] ;
    fftw_real post_left_amp[FFT_MAX], post_left_phase[FFT_MAX] ;
    fftw_real post_right_amp[FFT_MAX], post_right_phase[FFT_MAX] ;
    fftw_real tmp_l[FFT_MAX] ;
    fftw_real tmp_r[FFT_MAX] ;
#ifdef HAVE_FFTW3
    FFTW(plan) pPreLeft, pPreRight;
#else /* HAVE_FFTW3 */
    rfftw_plan pFor ;
#endif /* HAVE_FFTW3 */
    double dfs, hdfs ;
    extern struct view audio_view ;
    int FFT_SIZE ;

/*      return dethunk_new(pPrefs,first_sample,last_sample,channel_mask) ;  */

    for(FFT_SIZE = 8 ; FFT_SIZE < n_samples && FFT_SIZE < 8192 ; FFT_SIZE *= 2) ;

    dfs = FFT_SIZE ;
    hdfs = FFT_SIZE / 2 ;

    {
	long extra_samples = FFT_SIZE - n_samples ;
	first_sample -= extra_samples/2 ;
	if(first_sample < 0) first_sample = 0 ;
	last_sample = first_sample+FFT_SIZE-1 ;
	if(last_sample > pPrefs->n_samples-1) {
	    last_sample = pPrefs->n_samples-1 ;
	    first_sample = last_sample-FFT_SIZE-1 ;
	}
    }

    push_status_text("Saving undo information") ;
    start_save_undo("Undo dethunk", &audio_view) ;
    cancel = save_undo_data( first_sample, last_sample, pPrefs, TRUE) ;
    close_undo() ;
    pop_status_text() ;

    n_samples = last_sample - first_sample + 1 ;

    push_status_text("Dethunking audio") ;
    update_status_bar(0.0,STATUS_UPDATE_INTERVAL,TRUE) ;

    g_print("FFTSIZE:%d\n", FFT_SIZE) ;
    g_print("first_sample:%ld\n", first_sample) ;
    g_print("last_sample:%ld\n", last_sample) ;

#ifdef HAVE_FFTW3
    pPreLeft   = FFTW(plan_r2r_1d)(FFT_SIZE, left, tmp_l, FFTW_R2HC, FFTW_R2HC);
    pPreRight  = FFTW(plan_r2r_1d)(FFT_SIZE, right, tmp_r, FFTW_R2HC, FFTW_R2HC);
#else /* HAVE_FFTW3 */
    pFor = rfftw_create_plan(FFT_SIZE, FFTW_REAL_TO_COMPLEX, FFTW_ESTIMATE);
#endif /* HAVE_FFTW3 */

    /* get fft of samples ahead of thunk */
    {
	long first = first_sample-FFT_SIZE ;
	if(first < 0) first = 0 ;
	read_fft_real_wavefile_data(left,  right, first, first+FFT_SIZE-1) ;

#ifdef HAVE_FFTW3
	FFTW(execute)(pPreLeft);
	FFTW(execute)(pPreRight);
#else /* HAVE_FFTW3 */
	rfftw_one(pFor, left, tmp_l);
	rfftw_one(pFor, right, tmp_r);
#endif /* HAVE_FFTW3 */

	/* bias */
	pre_left_amp[0] = tmp_l[0]/hdfs ;
	pre_right_amp[0] = tmp_r[0]/hdfs ;

	for(k = 1 ; k < FFT_SIZE ; k++) {
	    tmp_l[k] /= hdfs ;
	    tmp_r[k] /= hdfs ;
	}

	/* convert noise sample to power spectrum */
	for(k = 1 ; k <= FFT_SIZE/2 ; k++) {
	    if(k < FFT_SIZE/2) {
		pre_left_amp[k] = tmp_l[k] * tmp_l[k] + tmp_l[FFT_SIZE-k]*tmp_l[FFT_SIZE-k] ;
		pre_left_phase[k] =  atan2(tmp_l[FFT_SIZE-k],tmp_l[k]);
		pre_right_amp[k] = tmp_r[k] * tmp_r[k] + tmp_r[FFT_SIZE-k]*tmp_r[FFT_SIZE-k] ;
		pre_right_phase[k] =  atan2(tmp_r[FFT_SIZE-k],tmp_r[k]);
	    } else {
		/* Nyquist Frequency */
		pre_left_amp[k] = tmp_l[k] * tmp_l[k] ;
		pre_right_amp[k] = tmp_r[k] * tmp_r[k] ;
		pre_left_phase[k] = 0.0 ;
		pre_right_phase[k] = 0.0 ;
	    }
	    pre_left_amp[k] = sqrt(pre_left_amp[k]) ;
	    pre_right_amp[k] = sqrt(pre_right_amp[k]) ;
	}

    }

    /* get fft of samples behind of thunk */
    {
	long first = last_sample+1 ;
	if(first+FFT_SIZE-1 > pPrefs->n_samples-1) first = pPrefs->n_samples-FFT_SIZE ;
	read_fft_real_wavefile_data(left,  right, first, first+FFT_SIZE-1) ;

#ifdef HAVE_FFTW3
	FFTW(execute)(pPreLeft);
	FFTW(execute)(pPreRight);
#else /* HAVE_FFTW3 */
	rfftw_one(pFor, left, tmp_l);
	rfftw_one(pFor, right, tmp_r);
#endif /* HAVE_FFTW3 */

	/* bias */
	post_left_amp[0] = tmp_l[0]/hdfs ;
	post_right_amp[0] = tmp_r[0]/hdfs ;

	for(k = 1 ; k < FFT_SIZE ; k++) {
	    tmp_l[k] /= hdfs ;
	    tmp_r[k] /= hdfs ;
	}

	/* convert noise sample to power spectrum */
	for(k = 1 ; k <= FFT_SIZE/2 ; k++) {
	    if(k < FFT_SIZE/2) {
		post_left_amp[k] = tmp_l[k] * tmp_l[k] + tmp_l[FFT_SIZE-k]*tmp_l[FFT_SIZE-k] ;
		post_left_phase[k] = atan2(tmp_l[FFT_SIZE-k],tmp_l[k]);
		post_right_amp[k] = tmp_r[k] * tmp_r[k] + tmp_r[FFT_SIZE-k]*tmp_r[FFT_SIZE-k] ;
		post_right_phase[k] = atan2(tmp_r[FFT_SIZE-k],tmp_r[k]);
	    } else {
		/* Nyquist Frequency */
		post_left_amp[k] = tmp_l[k] * tmp_l[k] ;
		post_right_amp[k] = tmp_r[k] * tmp_r[k] ;
		post_left_phase[k] = 0.0 ;
		post_right_phase[k] = 0.0 ;
	    }
	    post_left_amp[k] = sqrt(post_left_amp[k]) ;
	    post_right_amp[k] = sqrt(post_right_amp[k]) ;
	}

    }

/*      for(k = 1 ; k <= FFT_SIZE/2 ; k++) {  */
/*  	printf("pre  k:%d a:%lg p:%lg\n", k, pre_left_amp[k], pre_left_phase[k]) ;  */
/*  	printf("post k:%d a:%lg p:%lg\n", k, post_left_amp[k], post_left_phase[k]) ;  */
/*      }  */

    read_fft_real_wavefile_data(left,  right, first_sample, first_sample+FFT_SIZE-1) ;

    for(i = 0 ; i < FFT_SIZE ; i++) {
	double wgt_post = (double)i/(double)(FFT_SIZE-1) ;
	double wgt_pre = 1.0 - wgt_post ;
	double theta = ((double)i/(double)(FFT_SIZE-1))*2.0*M_PI ;

	update_status_bar((double)i/(double)FFT_SIZE,STATUS_UPDATE_INTERVAL,FALSE) ;

	if(channel_mask & 0x01)
	    left[i] = wgt_pre*pre_left_amp[0] + wgt_post*post_left_amp[0] ;

	if(channel_mask & 0x02)
	    right[i] = wgt_pre*pre_right_amp[0] + wgt_post*post_right_amp[0] ;

	for(k = 1 ; k <= FFT_SIZE/2 ; k++) {
	    double f = (double)k ;
	    if(channel_mask & 0x01) {
		double phase = wgt_pre*pre_left_phase[k] + wgt_post*post_left_phase[k] ;
		double amp = wgt_pre*pre_left_amp[k] + wgt_post*post_left_amp[k] ;
		left[i] += amp*cos(f*theta + phase) ;
	    }
	    if(channel_mask & 0x02) {
		double phase = wgt_pre*pre_right_phase[k] + wgt_post*post_right_phase[k] ;
		double amp = wgt_pre*pre_right_amp[k] + wgt_post*post_right_amp[k] ;
		right[i] += amp*cos(f*theta + phase) ;
	    }
	}
    }

    g_print("write first_sample:%ld\n", first_sample) ;
    g_print("write last_sample:%ld\n", last_sample) ;
	    
    write_fft_real_wavefile_data(left, right, first_sample, last_sample) ;

#ifdef HAVE_FFTW3
    FFTW(destroy_plan)(pPreLeft);
    FFTW(destroy_plan)(pPreRight);
#else /* HAVE_FFTW3 */
    rfftw_destroy_plan(pFor);
#endif /* HAVE_FFTW3 */

    update_status_bar(0.0,STATUS_UPDATE_INTERVAL,TRUE) ;

    pop_status_text() ;

    main_redraw(FALSE, TRUE) ;

    return 0 ;
}

#include "ar.h"
#include "ar.c"

#define ORDER 2048
int forward_extrapolate(fftw_real data[], int firstbad, int lastbad, int siglen)
{
    int n_bad = lastbad - firstbad + 1 ;
    int autolen = 60 ;
    int i, j ;
    double auto_coefs[ORDER+1] ;


/*
int AutoRegression(
   double   *inputseries,
   int      length,
   int      degree,
   double   *coefficients,
   int      method)
*/

    autolen = (siglen-n_bad)/4 ;
    //autolen *= 2 ;

    if(autolen < 0) {
	d_print("Autolen < 0!\n") ;
	return REPAIR_FAILURE;
    }

    if(autolen > ORDER) autolen = ORDER ;
    g_print("siglen:%d n_bad:%d Autolen:%d\n",siglen,n_bad,autolen) ;

    AutoRegression(data,firstbad,autolen,auto_coefs,0) ;

    for(i = firstbad ; i < lastbad ; i++) {
	data[i] = 0 ;
	for(j = 0 ; j < autolen ; j++)
	    data[i] += data[i - j - 1]*auto_coefs[j] ;
    }

    return 0 ;
}

int reverse_extrapolate(fftw_real data[], int firstbad, int lastbad, int siglen)
{
    int i ;
    fftw_real *x = calloc(siglen, sizeof(fftw_real)) ;

    for(i = 0 ; i < siglen ; i++)
	x[siglen-i-1] = data[i] ;

    forward_extrapolate(x, siglen-lastbad-1, siglen-firstbad-1, siglen) ;

    for(i = 0 ; i < siglen ; i++)
	data[i] = x[siglen-i-1] ;

    free(x) ;

    return 0 ;
}

void estimate_region(fftw_real data[], int firstbad, int lastbad, int siglen)
{
    int i ;
    fftw_real *data_r = calloc(siglen, sizeof(fftw_real)) ;
    int n_samples = lastbad - firstbad + 1 ;

    if(n_samples < 1) return ;

    if(n_samples == 1) {
	data[firstbad] = (data[firstbad-1]+data[firstbad+1])/2.0 ;
	return ;
    }


    for(i = 0 ; i < siglen ; i++)
	data_r[siglen-i-1] = data[i] ;

    forward_extrapolate(data, firstbad, lastbad, siglen) ;

    forward_extrapolate(data_r, siglen-lastbad-1, siglen-firstbad-1, siglen) ;

    for(i = firstbad ; i <= lastbad ; i++) {
	double d = i - firstbad ;
	double w_r = d / (double)(n_samples-1) ;
	double w_f = 1.0 - w_r ;
	data[i] = w_f*data[i] + w_r*data_r[siglen - i - 1] ;
    }

    free(data_r) ;
}


int dethunk(struct sound_prefs *pPrefs,
            long first_sample, long last_sample, int channel_mask)
{
    long n_samples = last_sample - first_sample + 1 ;
    int cancel ;
    fftw_real *left, *right ;
    int FFT_SIZE = MIN(ORDER*2,(last_sample-first_sample+1)*4) ;
    int siglen = last_sample-first_sample+1+2*FFT_SIZE ;
    extern struct view audio_view ;

    left = calloc(siglen, sizeof(fftw_real)) ;
    right = calloc(siglen, sizeof(fftw_real)) ;

    push_status_text("Saving undo information") ;
    start_save_undo("Undo dethunk", &audio_view) ;
    cancel = save_undo_data( first_sample, last_sample, pPrefs, TRUE) ;
    close_undo() ;
    pop_status_text() ;

    n_samples = last_sample - first_sample + 1 ;

    push_status_text("Dethunking audio") ;
    update_status_bar(0.0,STATUS_UPDATE_INTERVAL,TRUE) ;

    g_print("first_sample:%ld\n", first_sample) ;
    g_print("last_sample:%ld\n", last_sample) ;

    read_fft_real_wavefile_data(left,  right, first_sample-FFT_SIZE, last_sample+FFT_SIZE) ;

    if(channel_mask & 0x01) {
	estimate_region(left, FFT_SIZE, FFT_SIZE+n_samples-1, last_sample-first_sample+1+2*FFT_SIZE) ;
    }

    if(channel_mask & 0x02) {
	estimate_region(right, FFT_SIZE, FFT_SIZE+n_samples-1, last_sample-first_sample+1+2*FFT_SIZE) ;
    }

    write_fft_real_wavefile_data(left,  right, first_sample-FFT_SIZE, last_sample+FFT_SIZE) ;


    update_status_bar(0.0,STATUS_UPDATE_INTERVAL,TRUE) ;

    pop_status_text() ;
    free(left) ;
    free(right) ;

    main_redraw(FALSE, TRUE) ;

    return 0 ;
}
