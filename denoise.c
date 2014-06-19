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

/* denoise.c */

#include <stdlib.h>
#include <gnome.h>
#include "gtkledbar.h"
#include "gwc.h"
#include <math.h>
#include <float.h>

#include <unistd.h>
#include <sys/time.h>

//struct timeb start_time, middle_time, end_time ;
struct timeval start_time, middle_time, end_time ;
struct timezone tzp;

void start_timer(void)
{
        gettimeofday(&start_time,&tzp);
}

void stop_timer(char *message)
{
    gettimeofday(&end_time,&tzp) ;
    {
	double fstart = start_time.tv_sec + (double)start_time.tv_usec/1000000.0 ;
	double fend = end_time.tv_sec + (double)end_time.tv_usec/1000000.0 ;
	fprintf(stderr, "%s in %7.3lf real seconds\n", message, fend-fstart) ;
    }
}

#ifdef OLD_TIMER
struct timeb start_time, middle_time, end_time ;

void start_timer(void)
{
        ftime(&start_time) ;
}

void stop_timer(char *message)
{
    ftime(&end_time) ;

    {
	double fstart = start_time.time + (double)start_time.millitm/1000.0 ;
	double fend = end_time.time + (double)end_time.millitm/1000.0 ;
	fprintf(stderr, "%s in %7.3lf real seconds\n", message, fend-fstart) ;
    }
}
#endif



double bark_z[DENOISE_MAX_FFT] ;
double window_coef[DENOISE_MAX_FFT] ;
double sum_window_wgts[DENOISE_MAX_FFT] ;
double jg_upper[DENOISE_MAX_FFT][11] ;
double jg_lower[DENOISE_MAX_FFT][11] ;
/*  double *two_way_probs[DENOISE_MAX_FFT] ;  */

void compute_bark_z(int FFT_SIZE, int rate)
{
    int k ;

    /* compute the bark z value for this frequency bin */
    for(k = 1 ; k <= FFT_SIZE/2 ; k++) {
	double freq = (double)rate / 2.0 /(double)(FFT_SIZE/2)*(double)k ;
	bark_z[k] = 7.0*log(freq/650.0 + sqrt(1 + (freq/650.0)*(freq/650.0))) ;
    }
}

void compute_johnston_gain(int FFT_SIZE, double tonality_factor)
{
    int k ;

    for (k = 1; k <= FFT_SIZE/2 ; ++k) {
	int j ;

	for(j = k-1 ; j > 0 ; j--) {
	    double bark_diff = bark_z[k] - bark_z[j] ;

	    double johnston = 15.81 + 7.5*(bark_diff+.474) - 17.5*sqrt(1.0+(bark_diff+0.474)*(bark_diff+0.474)) ;
	    double johnston_masked = johnston - (tonality_factor*(14.5+bark_z[j])+5.5*(1.0 - tonality_factor)) ;
	    double gain = pow(10.0, johnston_masked/10.0) ;

	    jg_lower[k][k-j] = gain ;

	    if(k - j > 10) break ;
	}

	for(j = k ; j <= FFT_SIZE/2 ; j++) {
	    double bark_diff = bark_z[j] - bark_z[k] ;

	    double johnston = 15.81 + 7.5*(bark_diff+.474) - 17.5*sqrt(1.0+(bark_diff+0.474)*(bark_diff+0.474)) ;
	    double johnston_masked = johnston - (tonality_factor*(14.5+bark_z[j])+5.5*(1.0 - tonality_factor)) ;
	    double gain = pow(10.0, johnston_masked/10.0) ;

	    jg_upper[k][j-k] = gain ;

	    if(j - k > 10) break ;

	}
    }
}

int get_window_delta(struct denoise_prefs *pDnprefs)
{

    if(pDnprefs->window_type == DENOISE_WINDOW_HANNING_OVERLAP_ADD) {
	return pDnprefs->FFT_SIZE/2 ;
#ifdef DENOISE_TRY_ONE_SAMPLE
    } else if(pDnprefs->window_type == DENOISE_WINDOW_ONE_SAMPLE) {
	return 1 ;
#endif
    } else {
	if(pDnprefs->window_type == DENOISE_WINDOW_BLACKMAN)
	    return pDnprefs->FFT_SIZE/pDnprefs->smoothness ;
	else
	    return 3*pDnprefs->FFT_SIZE/4 ;
    }
}

void compute_sum_window_wgts(struct denoise_prefs *pDnprefs)
{
    int delta = get_window_delta(pDnprefs) ;
    int i, k ;

    for(i = 0 ; i < pDnprefs->FFT_SIZE ; i++) {
	sum_window_wgts[i] = 0.0 ;
	for(k = i ; k < pDnprefs->FFT_SIZE+i ; k += delta) {
	    sum_window_wgts[i] += window_coef[k%pDnprefs->FFT_SIZE] ;
	}
    }

/*      for(i = 0 ; i < pDnprefs->FFT_SIZE ; i++) {  */
/*  	g_print("i:%d sww:%lf\n", i, sum_window_wgts[i]) ;  */
/*      }  */
}

double gain_weiner(double Yk2, double Dk2)
{
    double gain ;
    double Xk2 = Yk2 - Dk2 ;

    if(Yk2 > Dk2)
	gain = (Xk2) / (Xk2+Dk2) ;
    else
	gain = 0.0 ;

    return gain ;
}

double gain_power_subtraction(double Yk2, double Dk2)
{
    double level = MAX(Yk2-Dk2, 0.0) ;

    if(Yk2 > DBL_MIN)
	return level/Yk2 ;
    else
	return 0.0 ;
}

double alpha_lorber(double snr)
{
    double snr_db = 10.*log10(snr) ;
    double alpha ;

    if(snr_db > 20) return 1.0 ;

    alpha = MIN((3.0 - 0.10*snr_db), 3.5) ;

    return alpha ;
}

#define SLOW_EM
#ifdef SLOW_EM
double hypergeom(double theta)
{
    double i0(double), i1(double) ;
    if(theta < 7.389056)
	return exp(-theta/2.0)*(1.0+theta*i0(theta/2.0)+theta*i1(theta/2.0)) ;
    else
	return exp(0.09379 + 0.50447*log(theta)) ;
}

double gain_em(double Rprio, double Rpost, double alpha)
{
    /* Ephraim-Malah classic noise suppression, from 1984 paper */

    double gain = 0.886226925*sqrt(1.0/(1.0+Rpost)*(Rprio/(1.0+Rprio))) ;

    gain *= hypergeom((1.0+Rpost)*(Rprio/(1.0+Rprio))) ;

    return gain ;
}
#else

double gain_em(double Rprio, double Rpost, double alpha)
{
    /* Ephraim-Malah noise suppression, from Godsill and Wolfe 2001 paper */
    double r = MAX(Rprio/(1.0+Rprio),DBL_MIN) ;
    double V = Rprio/(1.+Rprio)*Rpost ;

    return sqrt( r * (1.0+V)/Rpost ) ;
}
#endif

double blackman(int k, int N)
{
    double p = ((double)(k))/(double)(N-1) ;
    return 0.42-0.5*cos(2.0*M_PI*p) + 0.08*cos(4.0*M_PI*p) ;
}

double hanning(int k, int N)
{
    double p = ((double)(k))/(double)(N-1) ;
    return 0.5 - 0.5 * cos(2.0*M_PI*p) ;
}

double blackman_hybrid(int k, int n_flat, int N)
{
    if(k >= (N-n_flat)/2 && k <= n_flat+(N-n_flat)/2-1) {
	return 1.0 ;
    } else {
	double p ;
	if(k >= n_flat+(N-n_flat)/2-1) k -= n_flat ;
	p = (double)(k)/(double)(N-n_flat-1) ;
	return 0.42-0.5*cos(2.0*M_PI*p) + 0.08*cos(4.0*M_PI*p) ;
    }
}

double welty_alpha(double w, double x)
{
    double alpha = ( log(acos(-2.0*w+1)) - log(M_PI) ) / log(1.0 - x) ;
/*      d_print("Welty alpha=%g\n", alpha) ;  */
    return alpha ;
}

/*  double welty(int k, int N, double alpha)  */
/*  {  */
/*      double n2 = (double)N/2.0 ;  */
/*      double x = fabs(((double)k - n2) / (n2)) ;  */
/*      double tx = pow(1.0-x, alpha)*M_PI ;  */
/*      double w =  -( cos(tx)-1.0 )/2.0 ;  */
/*      d_print("Welty x=%g, w=%g, k=%d N=%d\n", x, w, k, N) ;  */
/*  }  */

double fft_window(int k, int N, int window_type)
{
    if(window_type == DENOISE_WINDOW_BLACKMAN) {
	return blackman(k, N) ;
    } else if(window_type == DENOISE_WINDOW_BLACKMAN_HYBRID) {
	return blackman_hybrid(k, N-N/4, N) ;
#ifdef DENOISE_TRY_ONE_SAMPLE
    } else if(window_type == DENOISE_WINDOW_ONE_SAMPLE) {
	return hanning(k, N) ;
#endif
    } else if(window_type == DENOISE_WINDOW_HANNING_OVERLAP_ADD) {
	return hanning(k, N) ;
    }

    return 0.0 ;
}

double db2w(double db)
{
    return pow(10,db/10) ;
}

double ceramic_mic_response(double f)
{
    double db = 0.0 ;
    if(f < 4000.0) {
	db =  0.0 ;
    } else if(f < 7000.0) {
	db =  (f-4000.0)/7000.0*2.0 ;
    } else if(f < 20000.0) {
	db =  2.0 - ((f-7000.0)/13000.0)*12.0 ;
    } else {
	db =  -10.0 ;
    }

    return db2w(db) ;
}

void cdivide(double *a, double *b, double c, double d)
{
    double denom =  (c*c + d*d) ;
    double anew, bnew ;

    if(denom < 1.e-60) {
	*a = 0.0 ;
	*b = 0.0 ;
	return ;
    }

    anew = (*a*c + *b*d) / denom ;
    bnew = (*b*c - *a*d) / denom ;
}

#define bin2freq(r,s,k) ((double)r / 2.0 /(double)(s/2)*(double)k)

int prev_sample[2] ;

static fftw_real windowed[DENOISE_MAX_FFT] ;
static fftw_real out[DENOISE_MAX_FFT] ;

#ifdef HAVE_FFTW3
static void fft_remove_noise(fftw_real sample[], fftw_real noise_min2[], fftw_real noise_max2[], fftw_real noise_avg2[],
                      FFTW(plan) *pFor, FFTW(plan) *pBak,
#else /* HAVE_FFTW3 */
void fft_remove_noise(fftw_real sample[], fftw_real noise_min2[], fftw_real noise_max2[], fftw_real noise_avg2[],
                      rfftw_plan *pFor, rfftw_plan *pBak,
#endif /* HAVE_FFTW3 */
		      struct denoise_prefs *pPrefs, int ch)
{
    int k ;
    fftw_real noise2[DENOISE_MAX_FFT/2+1] ;
    fftw_real noise[DENOISE_MAX_FFT/2+1] ;
    fftw_real Y2[DENOISE_MAX_FFT/2+1] ;
    fftw_real Y[DENOISE_MAX_FFT/2+1] ;
    fftw_real masked[DENOISE_MAX_FFT/2+1] ;
    fftw_real gain_k[DENOISE_MAX_FFT] ;
    static fftw_real bsig_prev[2][DENOISE_MAX_FFT],bY2_prev[2][DENOISE_MAX_FFT/2+1],bgain_prev[2][DENOISE_MAX_FFT/2+1] ;
    fftw_real *sig_prev,*Y2_prev,*gain_prev ;
    static int debug_frame = 1 ;
    double SFM, tonality_factor ;

    sig_prev = bsig_prev[ch] ;
    Y2_prev = bY2_prev[ch] ;
    gain_prev = bgain_prev[ch] ;

    if(0 || pPrefs->window_type == DENOISE_WINDOW_HANNING_OVERLAP_ADD) {
	/* with overlap-add pre-window the sample */
	for(k = 0 ; k < pPrefs->FFT_SIZE ; k++) {
	    windowed[k] = sample[k]*window_coef[k] ;
	    //if(debug_frame == 3) g_print("k:%d window_coef:%lf windowed:%lf\n", k, window_coef[k], windowed[k]) ;
	}

    } else {
	/* with other methods don't window the sample for FFT,
 	   but window the result back into the original
	   sample the FFT noise-removal process */

	for(k = 0 ; k < pPrefs->FFT_SIZE ; k++) {
	    windowed[k] = sample[k] ;
	}
    }

#ifdef HAVE_FFTW3
    FFTW(execute)(*pFor);
#else /* HAVE_FFTW3 */
    rfftw_one(*pFor, windowed, out);
#endif /* HAVE_FFTW3 */

    {
	double sum_log_p = 0.0 ;
	double sum_p = 0.0 ;
	double kinv = 1./(double)(pPrefs->FFT_SIZE/2.0) ;

	for (k = 1; k <= pPrefs->FFT_SIZE/2 ; ++k) {
	    noise2[k] = noise_max2[k] ;
	    noise2[k] = noise_min2[k] + 0.5*(noise_max2[k] - noise_min2[k]) ;
	    noise2[k] = noise_avg2[k] ;
	    noise[k] = sqrt(noise2[k]) ;
	    if(k < pPrefs->FFT_SIZE/2) {
		Y2[k] = out[k]*out[k] + out[pPrefs->FFT_SIZE-k]*out[pPrefs->FFT_SIZE-k] ;
		Y[k] = sqrt(Y2[k]) ;
	    } else {
		Y2[k] = out[k]*out[k] ;
		Y[k] = out[k] ;
	    }
	    sum_log_p += log10(Y2[k]) ;
	    sum_p += Y2[k] ;
	}


	SFM = 10.0*( kinv*sum_log_p - log10(sum_p*kinv) ) ;
	tonality_factor = MIN(SFM/-60.0, 1) ;
    }


    if(pPrefs->noise_suppression_method == DENOISE_LORBER) tonality_factor = 0.0 ;

/*      g_print("SFM:%f tonality:%lf\n", SFM, tonality_factor) ;  */

    if(pPrefs->noise_suppression_method == DENOISE_LORBER) {
	for (k = 1; k <= pPrefs->FFT_SIZE/2 ; ++k) {
	    double sum = 0 ;
	    double sum_wgts = 0 ;
	    int j ;

	    int j1 = MAX(k-10,1) ;
	    int j2 = MIN(k+10,pPrefs->FFT_SIZE/2) ;

	    for(j = j1 ; j <= j2 ; j++) {
		double d = ABS(j-k)+1.0 ;
		double wgt = 1./sqrt(d) ;
		sum += Y2[j]*wgt ;
		sum_wgts += wgt ;
	    }

	    masked[k] = sum / (sum_wgts+1.e-300) ;
	}
    }

/*      if(pPrefs->noise_suppression_method == DENOISE_LORBER || pPrefs->noise_suppression_method == DENOISE_WOLFE_GODSILL) {  */
    if(pPrefs->noise_suppression_method == DENOISE_WOLFE_GODSILL) {
	for (k = 1; k <= pPrefs->FFT_SIZE/2 ; ++k) {
	    int j ;

	    masked[k] = 0.0 ;

	    for(j = k-1 ; j > 0 ; j--) {
#ifdef OLD_N_SLOW
		double bark_diff = bark_z[k] - bark_z[j] ;

		double johnston = 15.81 + 7.5*(bark_diff+.474) - 17.5*sqrt(1.0+(bark_diff+0.474)*(bark_diff+0.474)) ;
		double johnston_masked = johnston - (tonality_factor*(14.5+bark_z[j])+5.5*(1.0 - tonality_factor)) ;
		double gain = pow(10.0, johnston_masked/10.0) ;

		if(gain < 1.e-2) break ;
#else
		double gain = jg_lower[k][k-j] ;
#endif
		if(k - j > 10) break ;

		masked[k] += MAX((Y2[j]-noise2[j]),0.0)*gain ;
	    }

	    for(j = k ; j <= pPrefs->FFT_SIZE/2 ; j++) {
#ifdef OLD_N_SLOW
		double bark_diff = bark_z[j] - bark_z[k] ;

		double johnston = 15.81 + 7.5*(bark_diff+.474) - 17.5*sqrt(1.0+(bark_diff+0.474)*(bark_diff+0.474)) ;
		double johnston_masked = johnston - (tonality_factor*(14.5+bark_z[j])+5.5*(1.0 - tonality_factor)) ;
		double gain = pow(10.0, johnston_masked/10.0) ;
#else
		double gain = jg_upper[k][j-k] ;
#endif

		if(gain < 1.e-2) break ;
		if(j - k > 10) break ;

		masked[k] += MAX((Y2[j]-noise2[j]),0.0)*gain ;
	    }

/*  	    if(debug_frame == 3) g_print("k:%d Y:%lf masked:%lf bark:%lf\n", k, Y[k], masked[k], bark_z[k]) ;  */
	}
    }

	

#ifdef TEST
    if(pPrefs->noise_suppression_method == DENOISE_AUDACITY) {

	   for(i=0; i<=len/2; i++)
	      plog[i] = log(power[i]);
	    
	   int half = len/2;
	   for(i=0; i<=half; i++) {
	      float smooth;
	      
	      if (plog[i] < noiseGate[i] + (level/2.0))
		 smooth = 0.0;
	      else
		 smooth = 1.0;
	      
	      smoothing[i] = smooth * 0.5 + smoothing[i] * 0.5;
	   }

	   /* try to eliminate tinkle bells */
	   for(i=2; i<=half-2; i++) {
	      if (smoothing[i]>=0.5 &&
		  smoothing[i]<=0.55 &&
		  smoothing[i-1]<0.1 &&
		  smoothing[i-2]<0.1 &&
		  smoothing[i+1]<0.1 &&
		  smoothing[i+2]<0.1)
		 smoothing[i] = 0.0;
	   }

	   outr[0] *= smoothing[0];
	   outi[0] *= smoothing[0];
	   outr[half] *= smoothing[half];
	   outi[half] *= smoothing[half];
	   for(i=1; i<half; i++) {
	      int j = len - i;
	      float smooth = smoothing[i];

	      outr[i] *= smooth;
	      outi[i] *= smooth;
	      outr[j] *= smooth;
	      outi[j] *= smooth;
	   }
#else
    if(0) {
#endif
    } else {
	if(pPrefs->noise_suppression_method == DENOISE_EXPERIMENTAL) {
	    /* this whole section is starting to play around with audio *restoration*, i.e. trying
	     * to guess what was missing, and adding it back in.  Think of very old recordings
	     * with that have a damped high frequency response */
	    if(0) {
		static int first=1 ;
		/* try simple deconvolution */
		for (k = 0 ; k <= pPrefs->FFT_SIZE/2 ; ++k) {
		    /* Gk needs to look like the original response filter */
		    double Gk = hanning(k,pPrefs->FFT_SIZE/2) ;
		    double freq = bin2freq(pPrefs->rate,pPrefs->FFT_SIZE,k) ;
		    Gk =  ceramic_mic_response(bin2freq(pPrefs->rate,pPrefs->FFT_SIZE,k)) ;
		    Gk = Gk * Gk ;

		    if( !(k % 100) && first ) printf("k:%d freq:%0.1f Gk:%f\n", k, freq, Gk) ;

		    if(k < pPrefs->FFT_SIZE/2) {
/*  			cdivide(&out[k],&out[pPrefs->FFT_SIZE-k],Gk,0) ;  */
			out[pPrefs->FFT_SIZE-k] /= Gk ;
			out[k] /= Gk ;
		    } else {
			out[k] /= Gk ;
		    }


		    out[k] *= Gk ;
		}
		first=0 ;
	    } else {
		// Step 1.  Compute the linear regression coefs(a,b) for the function ln(power2) = a*k + b ;
		double sumx = 0.0 ;
		double sumy = 0.0 ;
		double sumx2 = 0.0 ;
		double sumxy = 0.0 ;
		double n = 0 ;
		double a, b ;
		int k_cut_off = 2*pPrefs->FFT_SIZE/8 ;
		fftw_real *r = gain_k ;

		for (k = 1 ; k <= pPrefs->FFT_SIZE/2 ; ++k) {
		    r[k] = 1.0 ;
		}

		for (k = 1 ; k < k_cut_off ; ++k) {
		    double y = log(Y2[k]) ;
		    sumx += (double)k ;
		    sumy += y ;
		    sumxy += (double)k*y ;
		    sumx2 += (double)k*(double)k ;
		    n++ ;
		}

		a = (n*(sumxy)-sumx*sumy) / (n*sumx2 - sumx*sumx) ;
		b = (sumy - a*sumx) / n ;

		printf("a,b %f %f\n", a, b) ;

		// Step 2. Compute r[k], amount of energy present for each frequency bin relative to predicted

		for (k = 1 ; k < k_cut_off ; ++k) {
		    r[k] = log(Y2[k]) / (a*(double)k+b) ;
		}

    #define MAX_HARMONIC 3
		// Step 3.  Compute gain in each bin >= k_cutoff ;
		double harmonic_factor[6] = {1.0,1.0,1.0,1.0,1.0,1.0/8.0} ;
		int harmonic ;

		for(harmonic = 1 ; harmonic < MAX_HARMONIC+1 ; harmonic++) {
		    double harmonic_fraction = 1.0 - ((double)harmonic / (double)(harmonic+1)) ;
		    for (k = k_cut_off ; k <= pPrefs->FFT_SIZE/2 ; ++k) {
			/* compute j, the index of the fundamental freq of which k is the harmonic */
			int j = (int)((double)k * harmonic_fraction + 0.5) ;
			double ln_p2_hat = r[j]*(a * (double)k + b) ;
			double p2_hat = harmonic_factor[harmonic]*exp(ln_p2_hat) ;
			double Gk = sqrt((p2_hat+Y2[k]) / Y2[k]) ;

			if (k == pPrefs->FFT_SIZE/2) printf("harmonic, hf, j, p2_hat, Gk[maxfft], ln_p2_hat %d %f %d %f %f %f\n",
								harmonic, harmonic_fraction, j, p2_hat, Gk, ln_p2_hat) ;

			out[k] *= Gk ;
			if(k < pPrefs->FFT_SIZE/2) out[pPrefs->FFT_SIZE-k] *= Gk ;
		    }
		}
	    }
	} else {
	    for (k = 1; k <= pPrefs->FFT_SIZE/2 ; ++k) {
		if(noise2[k] > DBL_MIN) {
		    double gain, Fk, Gk ;

		    if(pPrefs->noise_suppression_method == DENOISE_EM) {
			double Rpost = MAX(Y2[k]/noise2[k]-1.0, 0.0) ;
			double alpha = pPrefs->dn_gamma ;
			double Rprio ;

			if(prev_sample[ch] == 1)
			    Rprio = (1.0-alpha)*Rpost+alpha*gain_prev[k]*gain_prev[k]*Y2_prev[k]/noise2[k] ;
			else
			    Rprio = Rpost ;

			gain = gain_em(Rprio, Rpost, alpha) ;
	/*  		g_print("Rpost:%lg Rprio:%lg gain:%lg gain_prev:%lg y2_prev:%lg\n", Rpost, Rprio, gain, gain_prev[k], Y2_prev[k]) ;  */
			gain_prev[k] = gain ;
			Y2_prev[k] = Y2[k] ;
		    } else if(pPrefs->noise_suppression_method == DENOISE_LORBER) {
			double SNRlocal = MAX(masked[k]/noise2[k]-1.0, 0.0) ;
			double SNRfilt, SNRprio ;
			double alpha ;

			if(prev_sample[ch] == 1) {
			    double eta = (1.0 + pPrefs->dn_gamma) / 2.0 ;  /* eta seems to be better closer to 1.0 than gamma */
			    SNRfilt = (1.-pPrefs->dn_gamma)*SNRlocal + pPrefs->dn_gamma*(sig_prev[k]/noise2[k]) ;
			    SNRprio = SNRfilt ; /* note, could use another parameter, like pPrefs->dn_gamma, here to compute SNRprio */
			    SNRprio = (1.-eta)*SNRlocal + eta*(sig_prev[k]/noise2[k]) ;
			}else {
			    SNRfilt = SNRlocal ;
			    SNRprio = SNRfilt ;
			}
			alpha = alpha_lorber(SNRprio) ;
			gain = 1.0 - alpha/(SNRfilt+1) ;
			sig_prev[k] = MAX(Y2[k]*gain,0.0) ;
		    } else if(pPrefs->noise_suppression_method == DENOISE_WOLFE_GODSILL) {
			double Rpost = MAX(Y2[k]/noise2[k]-1.0, 0.0) ;
			double alpha = pPrefs->dn_gamma ;
			double Rprio ;

			if(prev_sample[ch] == 1)
			    Rprio = (1.0-alpha)*Rpost+alpha*gain_prev[k]*gain_prev[k]*Y2_prev[k]/noise2[k] ;
			else
			    Rprio = Rpost ;

			Y2_prev[k] = Y2[k] ;

			if(Y2[k] > masked[k]) {
			    gain = MAX(masked[k]/Y2[k], Rprio/(Rprio+1.0)) ;
			} else {
			    gain = 1.0 ;
			}

			gain_prev[k] = gain ;

		    } else if(pPrefs->noise_suppression_method == DENOISE_WEINER)
			gain = gain_weiner(Y2[k], noise2[k]) ;
		    else
			gain = gain_power_subtraction(Y2[k], noise2[k]) ;

		    Fk = pPrefs->amount*(1.0-gain) ;

		    if(Fk < 0.0) Fk = 0.0 ;
		    if(Fk > 1.0) Fk = 1.0 ;

		    Gk =  1.0 - Fk ;

		    out[k] *= Gk ;
		    if(k < pPrefs->FFT_SIZE/2) out[pPrefs->FFT_SIZE-k] *= Gk ;

		    gain_k[k] = Gk ;
		}
	    }
	}
    }

    /* the inverse fft */
#ifdef HAVE_FFTW3
    FFTW(execute)(*pBak);
#else /* HAVE_FFTW3 */
    rfftw_one(*pBak, out, windowed);
#endif /* !HAVE_FFTW3 */

    for(k = 0 ; k < pPrefs->FFT_SIZE ; k++)
	windowed[k] /= (double)(pPrefs->FFT_SIZE) ;

    if(0|| pPrefs->window_type == DENOISE_WINDOW_HANNING_OVERLAP_ADD) {
	/** HANNING - OVERLAP - ADD */
	/* make sure the tails of the sample approach zero magnitude */
	double offset = windowed[0] ;
	double hs1 = pPrefs->FFT_SIZE/2 - 1 ;

	for(k = 0 ; k < pPrefs->FFT_SIZE/2 ; k++) {
	    double p = (hs1-(double)k)/hs1 ;
	    sample[k] = windowed[k] - offset*p ;
	}

	offset = windowed[pPrefs->FFT_SIZE-1] ;
	for(k = pPrefs->FFT_SIZE/2 ; k < pPrefs->FFT_SIZE ; k++) {
	    double p = ((double)k-hs1)/hs1 ;
	    sample[k] = windowed[k] - offset*p ;
	}
    } else {
	/* merge results back into sample data based on window function */
	for(k = 0 ; k < pPrefs->FFT_SIZE ; k++) {
	    double w = window_coef[k] ;
	    sample[k] = (1.0-w)*sample[k] + w*windowed[k] ;
	}
    }

    prev_sample[ch] = 1 ;
    debug_frame++ ;
}

int denoise_normalize = 0 ;

#define NO_DEBUG
#ifdef DEBUG
void print_denoise(char *header, struct denoise_prefs *pDnprefs)
{
    int k ;

    g_print("******** %s ************\n", header) ;
    g_print("FFT_SIZE:%d\n", pDnprefs->FFT_SIZE) ;
    g_print("n_noise_samples:%d\n", pDnprefs->n_noise_samples) ;
    g_print("amount:%lf\n", pDnprefs->amount) ;
    g_print("smoothness:%d\n", pDnprefs->smoothness) ;
    g_print("window_type:") ;
    switch(pDnprefs->window_type) {
	case DENOISE_WINDOW_BLACKMAN : g_print("Blackman\n") ; break ;
	case DENOISE_WINDOW_BLACKMAN_HYBRID : g_print("Blackman-hybrid\n") ; break ;
	case DENOISE_WINDOW_HANNING_OVERLAP_ADD : g_print("Hanning-overlap-add\n") ; break ;
#ifdef DENOISE_TRY_ONE_SAMPLE
	case DENOISE_WINDOW_ONE_SAMPLE : g_print("One Sample\n") ; break ;
#endif
	default : g_print("!!!!!!!!! UNKNOWN !!!!!!!!!!!!\n") ; break ;
    }
    g_print("Suppression method:") ;
    switch(pDnprefs->noise_suppression_method) {
	case DENOISE_LORBER : g_print("Lorber-Hoeldrich\n") ; break ;
	case DENOISE_WEINER : g_print("Weiner\n") ; break ;
	case DENOISE_EM : g_print("Ephram\n") ; break ;
	case DENOISE_WOLFE_GODSILL : g_print("Wolfe-Godsill\n") ; break ;
	default : g_print("Spectral Subtraction\n") ; break ;
    }
    for(k = 0 ; k < pDnprefs->FFT_SIZE ; k++) {
	g_print("k:%d wc:%lg\n", k, window_coef[k]) ;
    }
}
#else
void print_denoise(char *header, struct denoise_prefs *pDnprefs) {}
#endif

void get_noise_sample(struct sound_prefs *pPrefs, struct denoise_prefs *pDnprefs,
		    long noise_start, long noise_end,
		    fftw_real *left_noise_min, fftw_real *left_noise_max, fftw_real *left_noise_avg,
		    fftw_real *right_noise_min, fftw_real *right_noise_max, fftw_real *right_noise_avg) ;

int denoise(struct sound_prefs *pPrefs, struct denoise_prefs *pDnprefs, long noise_start, long noise_end,
            long first_sample, long last_sample, int channel_mask) {
    long current ;
    int k ;
    fftw_real left[DENOISE_MAX_FFT], right[DENOISE_MAX_FFT] ;
    fftw_real left_noise_max[DENOISE_MAX_FFT], right_noise_max[DENOISE_MAX_FFT], left_noise_avg[DENOISE_MAX_FFT] ;
    fftw_real left_noise_min[DENOISE_MAX_FFT], right_noise_min[DENOISE_MAX_FFT], right_noise_avg[DENOISE_MAX_FFT] ;
    fftw_real tmp[DENOISE_MAX_FFT] ;
    fftw_real left_prev_frame[DENOISE_MAX_FFT] ;
    fftw_real right_prev_frame[DENOISE_MAX_FFT] ;
#ifdef HAVE_FFTW3
    FFTW(plan) pForLeft, pForRight ;
    FFTW(plan) pFor, pBak ;
#else /* HAVE_FFTW3 */
    rfftw_plan pFor, pBak ;
#endif /* HAVE_FFTW3 */
    int framenum = 0 ;
    double alpha ;
    double s_amount ; /* amount, reduced to account for oversampling
                         due to smoothness */
                         

    start_timer() ;
    current = first_sample ;

    push_status_text("Denoising audio") ;
    update_status_bar(0.0,STATUS_UPDATE_INTERVAL,TRUE) ;

#ifdef HAVE_FFTW3
    pFor =
        FFTW(plan_r2r_1d)(pDnprefs->FFT_SIZE, windowed, out, FFTW_R2HC, FFTW_ESTIMATE);
    pBak =
        FFTW(plan_r2r_1d)(pDnprefs->FFT_SIZE, out, windowed, FFTW_HC2R, FFTW_ESTIMATE);
#endif /* HAVE_FFTW3 */


#ifdef HAVE_FFTW3
    pForLeft =
        FFTW(plan_r2r_1d)(pDnprefs->FFT_SIZE, left, tmp, FFTW_R2HC, FFTW_ESTIMATE);
    pForRight =
        FFTW(plan_r2r_1d)(pDnprefs->FFT_SIZE, right, tmp, FFTW_R2HC, FFTW_ESTIMATE);
#else /* HAVE_FFTW3 */
    pFor = rfftw_create_plan(pDnprefs->FFT_SIZE, FFTW_REAL_TO_COMPLEX, FFTW_ESTIMATE);
    pBak = rfftw_create_plan(pDnprefs->FFT_SIZE, FFTW_COMPLEX_TO_REAL, FFTW_ESTIMATE);
#endif /* HAVE_FFTW3 */

    alpha = welty_alpha(0.5, 1.0/(double)pDnprefs->smoothness) ;
    alpha = 1.0 ;

    for(k = 0 ; k < pDnprefs->FFT_SIZE ; k++) {
	window_coef[k] = fft_window(k,pDnprefs->FFT_SIZE, pDnprefs->window_type) ;
	d_print("k:%d wc:%lg\n", k, window_coef[k]) ;
	left_prev_frame[k] = 0.0 ;
	left[k] = 0.0 ;
	right_prev_frame[k] = 0.0 ;
	right[k] = 0.0 ;
/*  	two_way_probs[k] = (double *)calloc(pDnprefs->FFT_SIZE,sizeof(double)) ;  */
/*  	if(two_way_probs[k] == NULL) {  */
/*  	    fprintf(stderr, "Failed to malloc two_way_probs, %d of %d\n", k+1, pDnprefs->FFT_SIZE) ;  */
/*  	    exit(1) ;  */
/*  	}  */
    }


    audio_normalize(denoise_normalize) ;

    get_noise_sample(pPrefs, pDnprefs, noise_start, noise_end,
		    left_noise_min, left_noise_max, left_noise_avg,
		    right_noise_min, right_noise_max, right_noise_avg) ;
    pDnprefs->rate = pPrefs->rate ;

    audio_normalize(denoise_normalize) ;


/*      if(smoothness <= 4 || window_type == DENOISE_WINDOW_BLACKMAN_HYBRID)  */
	s_amount = pDnprefs->amount ;
/*      else  */
/*  	s_amount = amount/(double)(smoothness-3) ;  */

    prev_sample[0] = 0 ;
    prev_sample[1] = 0 ;

    if(pDnprefs->amount > DBL_MIN) {

	while(current <= last_sample-pDnprefs->FFT_SIZE) {
	    long n = pDnprefs->FFT_SIZE ;
	    long tmplast = current + n - 1 ;
	    gfloat p = (gfloat)(current-first_sample)/(last_sample-first_sample) ;

	    n = read_fft_real_wavefile_data(left,  right, current, current+pDnprefs->FFT_SIZE-1) ;
	    if(n < pDnprefs->FFT_SIZE) break ; /*  hit the end of all data? */

#ifdef MAC_OS_X
	    // This usleep fixes a segfault on OS X.  Rob Frohne
	    usleep(2);
#endif
	    update_status_bar(p,STATUS_UPDATE_INTERVAL,FALSE) ;

	    /*
	    */
	    if(framenum == 0) {
		for(k = 0 ; k < pDnprefs->FFT_SIZE ; k++) {
		    if(k < pDnprefs->FFT_SIZE/2) {
			d_print("OA sum %d:%lg\n", k, window_coef[k]+window_coef[k+pDnprefs->FFT_SIZE/2]) ;
			window_coef[k] = 1.0 ;
		    }
		}
		compute_sum_window_wgts(pDnprefs) ;
		framenum = 1 ;
	    } else if(framenum == 1) {
		for(k = 0 ; k < pDnprefs->FFT_SIZE ; k++) {
		    window_coef[k] = fft_window(k,pDnprefs->FFT_SIZE, pDnprefs->window_type) ;
		}
		compute_sum_window_wgts(pDnprefs) ;
		framenum = 2 ;
	    }

	    if(channel_mask & 0x01)
		fft_remove_noise(left, left_noise_min, left_noise_max, left_noise_avg,
		                  &pFor, &pBak, pDnprefs,0) ;
	    if(channel_mask & 0x02)
		fft_remove_noise(right, right_noise_min, right_noise_max, right_noise_avg,
		                  &pFor, &pBak, pDnprefs,1) ;

	    if(pDnprefs->window_type == DENOISE_WINDOW_HANNING_OVERLAP_ADD) {
		for(k = 0 ; k < pDnprefs->FFT_SIZE/2 ; k++) {
		    if(channel_mask & 0x01) {
			/* add in the last half of the previous output frame */
			left[k] += left_prev_frame[k+pDnprefs->FFT_SIZE/2] ;

			/* save the last half of this output frame */
			left_prev_frame[k+pDnprefs->FFT_SIZE/2] = left[k+pDnprefs->FFT_SIZE/2] ;
		    }

		    if(channel_mask & 0x02) {
			/* add in the last half of the previous output frame */
			right[k] += right_prev_frame[k+pDnprefs->FFT_SIZE/2] ;

			/* save the last half of this output frame */
			right_prev_frame[k+pDnprefs->FFT_SIZE/2] = right[k+pDnprefs->FFT_SIZE/2] ;
		    }
		}

		write_fft_real_wavefile_data(left, right, current, current+pDnprefs->FFT_SIZE/2-1) ;
#ifdef DENOISE_TRY_ONE_SAMPLE
	    } else if(pDnprefs->window_type == DENOISE_WINDOW_ONE_SAMPLE) {
		if(current == first_sample) {
		    write_fft_real_wavefile_data(left, right, current, current+pDnprefs->FFT_SIZE/2-1) ;
		} else {
		    write_fft_real_wavefile_data(left, right, current, current) ;
		}
#endif
	    } else {
		write_fft_real_wavefile_data(left, right, current, MIN(tmplast, last_sample)) ;
	    }

	    current += get_window_delta(pDnprefs) ;

	}
    }

#ifdef HAVE_FFTW3
    FFTW(destroy_plan)(pForLeft);
    FFTW(destroy_plan)(pForRight);
    FFTW(destroy_plan)(pBak);
    FFTW(destroy_plan)(pFor);
#else /* HAVE_FFTW3 */
    rfftw_destroy_plan(pFor);
    rfftw_destroy_plan(pBak);
#endif /* HAVE_FFTW3 */

    for(k = 0 ; k < pDnprefs->FFT_SIZE ; k++) {
/*  	free(two_way_probs[k]) ;  */
    }

    audio_normalize(1) ;

    update_status_bar(0.0,STATUS_UPDATE_INTERVAL,TRUE) ;

    pop_status_text() ;

    main_redraw(FALSE, TRUE) ;

    stop_timer("DENOISE") ;

    return 0 ;
}

int print_noise_sample(struct sound_prefs *pPrefs, struct denoise_prefs *pDnprefs, long noise_start, long noise_end)
{
    int k ;
    FILE *fp ;

    fftw_real left_noise_max[DENOISE_MAX_FFT], right_noise_max[DENOISE_MAX_FFT], left_noise_avg[DENOISE_MAX_FFT] ;
    fftw_real left_noise_min[DENOISE_MAX_FFT], right_noise_min[DENOISE_MAX_FFT], right_noise_avg[DENOISE_MAX_FFT] ;
    extern int MAXSAMPLEVALUE ;
    double max = MAXSAMPLEVALUE * MAXSAMPLEVALUE ;

    get_noise_sample(pPrefs, pDnprefs, noise_start, noise_end,
		    left_noise_min, left_noise_max, left_noise_avg,
		    right_noise_min, right_noise_max, right_noise_avg) ;

    fp = fopen("noise.dat", "w") ;

    fprintf(stderr, "FFT_SIZE in print_noise_sample is %d\n", pDnprefs->FFT_SIZE) ;

    for(k = 1 ; k <= pDnprefs->FFT_SIZE/2 ; k++) {
	double freq = (double)pPrefs->rate / 2.0 /(double)(pDnprefs->FFT_SIZE/2)*(double)k ;
	double db_left = 20.0*log10(left_noise_avg[k]/(max/2.0)) ;
	double db_right = 20.0*log10(right_noise_avg[k]/(max/2.0)) ;
	fprintf(fp, "%10lgHz %12.1lfdB %12.1lfdB\n", freq, db_left, db_right) ;
    }

    fclose(fp) ;

    return 0 ;
}

void get_noise_sample(struct sound_prefs *pPrefs, struct denoise_prefs *pDnprefs, long noise_start, long noise_end,
		    fftw_real *left_noise_min, fftw_real *left_noise_max, fftw_real *left_noise_avg,
		    fftw_real *right_noise_min, fftw_real *right_noise_max, fftw_real *right_noise_avg)
{
    int i, j, k ;
#ifdef HAVE_FFTW3
    FFTW(plan) pForLeft, pForRight ;
#else /* HAVE_FFTW3 */
    rfftw_plan pFor, pBak ;
#endif /* HAVE_FFTW3 */

    fftw_real left[DENOISE_MAX_FFT], right[DENOISE_MAX_FFT] ;
    fftw_real tmp[DENOISE_MAX_FFT] ;

#ifdef HAVE_FFTW3
    pForLeft =
        FFTW(plan_r2r_1d)(pDnprefs->FFT_SIZE, left, tmp, FFTW_R2HC, FFTW_ESTIMATE);
    pForRight =
        FFTW(plan_r2r_1d)(pDnprefs->FFT_SIZE, right, tmp, FFTW_R2HC, FFTW_ESTIMATE);
#else /* HAVE_FFTW3 */
    pFor = rfftw_create_plan(pDnprefs->FFT_SIZE, FFTW_REAL_TO_COMPLEX, FFTW_ESTIMATE);
    pBak = rfftw_create_plan(pDnprefs->FFT_SIZE, FFTW_COMPLEX_TO_REAL, FFTW_ESTIMATE);
#endif /* HAVE_FFTW3 */

    for(k = 0 ; k < pDnprefs->FFT_SIZE ; k++) {
	if(0 || pDnprefs->window_type == DENOISE_WINDOW_HANNING_OVERLAP_ADD) {
	    window_coef[k] = fft_window(k,pDnprefs->FFT_SIZE, pDnprefs->window_type) ;
	} else {
	    window_coef[k] = 1.0 ;
	}
	left_noise_max[k] = 0.0 ;
	right_noise_max[k] = 0.0 ;
	left_noise_avg[k] = 0.0 ;
	right_noise_avg[k] = 0.0 ;
	left_noise_min[k] = DBL_MAX ;
	right_noise_min[k] = DBL_MAX ;
    }

    audio_normalize(denoise_normalize) ;

    for(i = 0 ; i < pDnprefs->n_noise_samples ; i++) {
	long first = noise_start + i*(noise_end-noise_start-pDnprefs->FFT_SIZE)/pDnprefs->n_noise_samples ;

	read_fft_real_wavefile_data(left,  right, first, first+pDnprefs->FFT_SIZE-1) ;

	for(k = 0 ; k < pDnprefs->FFT_SIZE ; k++) {
	    left[k] *= window_coef[k] ;
	    right[k] *= window_coef[k] ;
	}

	if(0 && pDnprefs->noise_suppression_method == DENOISE_EXPERIMENTAL) {
	    for(k = 1 ; k <= pDnprefs->FFT_SIZE/2 ; k++) {
		for(j = 1 ; j <= pDnprefs->FFT_SIZE/2 ; j++) {
/*  		    two_way_probs[j][k] = 0.0 ;  */
		}
	    }
	}

#ifdef HAVE_FFTW3
	FFTW(execute)(pForLeft);
#else /* HAVE_FFTW3 */
	rfftw_one(pFor, left, tmp);
#endif /* HAVE_FFTW3 */

	/* convert noise sample to power spectrum */
	for(k = 1 ; k <= pDnprefs->FFT_SIZE/2 ; k++) {
	    double p2 ;
	    if(k < pDnprefs->FFT_SIZE/2) {
		p2 = tmp[k] * tmp[k] + tmp[pDnprefs->FFT_SIZE-k]*tmp[pDnprefs->FFT_SIZE-k] ;
	    } else {
		/* Nyquist Frequency */
		p2 = tmp[k] * tmp[k] ;
	    }
	    left_noise_min[k] = MIN(left_noise_min[k], p2) ;
	    left_noise_max[k] = MAX(left_noise_max[k], p2) ;
	    left_noise_avg[k] += p2 ;
	}

	if(0 && pDnprefs->noise_suppression_method == DENOISE_EXPERIMENTAL) {
	    for(k = 1 ; k <= pDnprefs->FFT_SIZE/2 ; k++) {
		double p2 ;
		if(k < pDnprefs->FFT_SIZE/2) {
		    p2 = tmp[k] * tmp[k] + tmp[pDnprefs->FFT_SIZE-k]*tmp[pDnprefs->FFT_SIZE-k] ;
		} else {
		    /* Nyquist Frequency */
		    p2 = tmp[k] * tmp[k] ;
		}
		for(j = k+k/2 ; j <= pDnprefs->FFT_SIZE/2 ; j++) {
		    double p2j ;
		    if(j < pDnprefs->FFT_SIZE/2) {
			p2j = tmp[j] * tmp[j] + tmp[pDnprefs->FFT_SIZE-j]*tmp[pDnprefs->FFT_SIZE-j] ;
		    } else {
			/* Nyquist Frequency */
			p2j = tmp[j] * tmp[j] ;
		    }
/*  		    two_way_probs[j][k] = MAX(two_way_probs[j][k],p2j/p2) ;  */
		}
	    }
	}

#ifdef HAVE_FFTW3
	FFTW(execute)(pForRight);
#else /* HAVE_FFTW3 */
	rfftw_one(pFor, right, tmp);
#endif /* HAVE_FFTW3 */

	/* convert noise sample to power spectrum */
	for(k = 1 ; k <= pDnprefs->FFT_SIZE/2 ; k++) {
	    double p2 ;
	    if(k < pDnprefs->FFT_SIZE/2) {
		p2 = tmp[k] * tmp[k] + tmp[pDnprefs->FFT_SIZE-k]*tmp[pDnprefs->FFT_SIZE-k] ;
	    } else {
		/* Nyquist Frequency */
		p2 = tmp[k] * tmp[k] ;
	    }
	    right_noise_min[k] = MIN(right_noise_min[k], p2) ;
	    right_noise_max[k] = MAX(right_noise_max[k], p2) ;
	    right_noise_avg[k] += p2 ;
	}

	if(0 && pDnprefs->noise_suppression_method == DENOISE_EXPERIMENTAL) {
	    for(k = 1 ; k <= pDnprefs->FFT_SIZE/2 ; k++) {
		double p2 ;
		if(k < pDnprefs->FFT_SIZE/2) {
		    p2 = tmp[k] * tmp[k] + tmp[pDnprefs->FFT_SIZE-k]*tmp[pDnprefs->FFT_SIZE-k] ;
		} else {
		    /* Nyquist Frequency */
		    p2 = tmp[k] * tmp[k] ;
		}
		for(j = k+k/2 ; j <= pDnprefs->FFT_SIZE/2 ; j++) {
		    double p2j ;
		    if(j < pDnprefs->FFT_SIZE/2) {
			p2j = tmp[j] * tmp[j] + tmp[pDnprefs->FFT_SIZE-j]*tmp[pDnprefs->FFT_SIZE-j] ;
		    } else {
			/* Nyquist Frequency */
			p2j = tmp[j] * tmp[j] ;
		    }
/*  		    two_way_probs[j][k] = MAX(two_way_probs[j][k],p2j/p2) ;  */
		}
	    }
	}

    }


    /* average out the power spectrum samples */
    for(k = 1 ; k <= pDnprefs->FFT_SIZE/2 ; k++) {
	left_noise_avg[k] /= (double)pDnprefs->n_noise_samples ;
	right_noise_avg[k] /= (double)pDnprefs->n_noise_samples ;
    }

    compute_bark_z(pDnprefs->FFT_SIZE, pPrefs->rate) ;
    compute_johnston_gain(pDnprefs->FFT_SIZE, 0.0) ;

    if(pDnprefs->freq_filter) {
	if(pDnprefs->estimate_power_floor) {
	    double half_freq_w = (pDnprefs->max_sample_freq - pDnprefs->min_sample_freq)/2.0 ;
	    double sum_left = 0.0 ;
	    double n_left = 0.0 ;
	    double sum_right = 0.0 ;
	    double n_right = 0.0 ;
	    for(k = 1 ; k <= pDnprefs->FFT_SIZE/2 ; k++) {
		double freq = (double)pPrefs->rate / 2.0 /(double)(pDnprefs->FFT_SIZE/2)*(double)k ;
		if(freq < pDnprefs->min_sample_freq && freq > pDnprefs->min_sample_freq-half_freq_w) {
		    sum_left += left_noise_avg[k] ;
		    n_left += 1.0 ;
		    sum_right += right_noise_avg[k] ;
		    n_right += 1.0 ;
		}
		if(freq > pDnprefs->max_sample_freq && freq < pDnprefs->max_sample_freq+half_freq_w) {
		    sum_left += left_noise_avg[k] ;
		    n_left += 1.0 ;
		    sum_right += right_noise_avg[k] ;
		    n_right += 1.0 ;
		}
	    }
	    if(n_left > 1.e-30) sum_left /= n_left ;
	    if(n_right > 1.e-30) sum_right /= n_left ;
	    for(k = 1 ; k <= pDnprefs->FFT_SIZE/2 ; k++) {
		double freq = (double)pPrefs->rate / 2.0 /(double)(pDnprefs->FFT_SIZE/2)*(double)k ;
		if(freq < pDnprefs->min_sample_freq || freq > pDnprefs->max_sample_freq) {
		    left_noise_avg[k] -= sum_left ;
		    right_noise_avg[k] -= sum_right ;
		}
	    }

	}
	for(k = 1 ; k <= pDnprefs->FFT_SIZE/2 ; k++) {
	    double freq = (double)pPrefs->rate / 2.0 /(double)(pDnprefs->FFT_SIZE/2)*(double)k ;
	    if(freq < pDnprefs->min_sample_freq || freq > pDnprefs->max_sample_freq) {
		left_noise_avg[k] = 0.0 ;
		right_noise_avg[k] = 0.0 ;
	    }
	}
    }

#ifdef HAVE_FFTW3
    FFTW(destroy_plan)(pForLeft);
    FFTW(destroy_plan)(pForRight);
#else /* HAVE_FFTW3 */
    rfftw_destroy_plan(pFor);
    rfftw_destroy_plan(pBak);
#endif /* HAVE_FFTW3 */

    audio_normalize(1) ;
}
