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

/* declick.c */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <float.h>
#include "gwc.h"
#include "stat.h"
#undef warning

#define MESCHACH
#ifndef MESCHACH
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_permutation.h>
#include <gsl/gsl_linalg.h>
#endif

void fit_cubic(fftw_real data[], int n, fftw_real estimated[]) ;

#define FFT_MAX 8192

double high_pass_filter(fftw_real x[], int N)
{
    int i ;
    double sum2 = 0.0 ;
    double d2x ;

    for(i = 1 ; i < N-1 ; i++) {
	d2x = x[i-1] - 2.0 * x[i] + x[i+1] ;
	sum2 += d2x*d2x ;
    }

    return sqrt(sum2/( (double)N - 2) ) ;
}

void stats(double x[], int n, double *pMean, double *pStderr, double *pVar, double *pCv, double *pStddev)
{
    double sum_wgt = 0.0 ;
    double sum = 0.0 ;
    double sum2 = 0.0 ;
    double wgt ;
    int i ;

    for(i = 0 ; i < n ; i++) {
	wgt = 1./((double)(n-i)) ;
	wgt = 1. ;
	sum += x[i]*wgt ;
	sum2 += x[i]*x[i]*wgt ;
	sum_wgt += wgt ;
    }

    if(sum_wgt > -DBL_MIN && sum_wgt < DBL_MIN) sum_wgt = 10.0*DBL_MIN ;

    *pMean = sum / sum_wgt ;

    if(n > 1) {
        *pVar = (sum2 - 2.0*(*pMean*sum) + *pMean**pMean*sum_wgt) / ((double)n - 1.0) ;
        *pStddev = sqrt(*pVar) ;
        *pCv = 100.0 * *pStddev / (*pMean+1.e-100) ;
        *pStderr = sqrt(*pVar / sum_wgt) ;
    } else {
        *pVar = 0.0 ;
        *pStddev = 0.0 ;
        *pCv = 0.0 ;
        *pStderr = 0.0 ;
    }
} ;

#ifdef UNUSED_FUNCTION_IN_DECLICK
void get_windowed_ps(fftw_real ps[], fftw_real in[], double window_coef[], int FFT_SIZE, rfftw_plan pFor)
{
    fftw_real out[FFT_MAX], windowed[FFT_MAX] ;
    int k ;

    for(k = 0 ; k < FFT_SIZE ; k++) {
	windowed[k] = window_coef[k] * in[k] ;
    }

    rfftw_one(pFor, windowed, out);

    for (k = 1; k <= FFT_SIZE/2 ; ++k)
	ps[k] = k < FFT_SIZE/2 ?  out[k]*out[k] + out[FFT_SIZE-k]*out[FFT_SIZE-k] : out[k]*out[k] ;
}
#endif

void fit_trig_basis(fftw_real data[], int n, fftw_real estimated[], int click_start, int click_end)
{
    int leftmin = 0 ;
    int leftmax = click_start ;
    int rightmax = n-1 ;
    int rightmin = click_end+1 ;
#define ORDER 3
#define NPARAMS (ORDER*2+1)
    int o ;
    double B[ORDER*2+1+1] ;
    int i ;
    double x[ORDER*2+1] ;

    init_reg(ORDER*2+1) ;

    for(i = leftmin ; i <= leftmax ; i++) {
	double v = (double)(i-leftmin)/(double)n ;
	for(o = 0 ; o < ORDER ; o++) {
	    x[o*2] = cos((o+1)*M_PI*v) ;
	    x[o*2+1] = sin((o+1)*M_PI*v) ;
	}
	x[NPARAMS-1] = v ;

	sum_reg(x, data[i]) ;
    }

    for(i = rightmin ; i <= rightmax ; i++) {
	double v = (double)(i-leftmin)/(double)n ;
	for(o = 0 ; o < ORDER ; o++) {
	    x[o*2] = cos((o+1)*M_PI*v) ;
	    x[o*2+1] = sin((o+1)*M_PI*v) ;
	}
	x[NPARAMS-1] = v ;
	sum_reg(x, data[i]) ;
    }

    estimate_reg(B) ;

    for(i = 0 ; i < ORDER  ; i++)
	printf("B[%d,%d]=%10lg %10lg\n", i*2+1, i*2+1+1, B[i*2+1], B[i*2+1+1]) ;

    printf("B[%d,%d]=%10lg %10lg\n", 0, NPARAMS, B[0], B[NPARAMS]) ;

    for(i = leftmin ; i <= rightmax ; i++) {
	double v = (double)(i-leftmin)/(double)n ;
	estimated[i] = B[0] + B[NPARAMS]*v ;
	for(o = 0 ; o < ORDER ; o++) {
	    estimated[i] += B[o*2+1] * cos((o+1)*M_PI*v) ;
	    estimated[i] += B[o*2+1+1] * sin((o+1)*M_PI*v) ;
	}
    }
}


#ifndef MESCHACH
gsl_matrix *gsl_transp(gsl_matrix *m)
{
    int i,j,rows,cols ;
    gsl_matrix *t ;

    rows = m->size1 ;
    cols = m->size2 ;

    t = gsl_matrix_alloc(cols,rows) ;

    for(i = 0 ; i < rows ; i++)
	for(j = 0 ; j < cols ; j++)
	    gsl_matrix_set(t, j, i, gsl_matrix_get(m, i, j)) ;

    return t ;
}

gsl_vector *gsl_mv_mlt(gsl_matrix *m, gsl_vector *v)
{
    gsl_vector *r ;
    int i,j,rows,cols ;

    rows = m->size1 ;
    cols = m->size2 ;

    r = gsl_vector_alloc(cols) ;

    for(j = 0 ; j < cols ; j++) {
	double x = gsl_matrix_get(m, 0, j)*gsl_vector_get(v,0) ;
	for(i = 1 ; i < rows ; i++)
	    x += gsl_matrix_get(m, i, j)*gsl_vector_get(v,i) ;
	gsl_vector_set(r, i, x) ;
    }

    return r ;
}

gsl_matrix  *gsl_m_mlt(gsl_matrix *m1, gsl_matrix *m2)
{
    gsl_matrix *r ;

    int i,j,k,rows,cols,out_cols ;

    rows = m1->size1 ;
    cols = m1->size2 ;

    out_cols = m2->size2 ;

    r = gsl_matrix_alloc(rows,out_cols) ;

    for(i = 0 ; i < rows ; i++) {
	for(k = 0 ; k < out_cols ; k++) {
	    double x = gsl_matrix_get(m1, i, 0)*gsl_matrix_get(m2,0,k) ;

	    for(j = 1 ; j < cols ; j++)
		x += gsl_matrix_get(m1, i, j)*gsl_matrix_get(m2,j,k) ;

	    gsl_matrix_set(r, i, k, x) ;
	}
    }

    return r ;
}

gsl_matrix * gsl_m_inverse(gsl_matrix *m)
{
    gsl_matrix *inverse,*ludecomp ;
    gsl_permutation *perm ;
    int s ;

    inverse = gsl_matrix_alloc(m->size1,m->size2) ;
    ludecomp = gsl_matrix_alloc(m->size1,m->size2) ;
    perm = gsl_permutation_alloc(m->size1) ;

    gsl_matrix_memcpy(ludecomp,m) ;
    gsl_linalg_LU_decomp(ludecomp,perm,&s) ;
    gsl_linalg_LU_invert(ludecomp,perm,inverse) ;

    gsl_matrix_free(ludecomp) ;
    gsl_permutation_free(perm) ;

    return inverse ;
}
#endif


int lsar_sample_restore(fftw_real data[], int firstbad, int lastbad, int siglen)
{
#ifdef MESCHACH
    int n_bad = lastbad - firstbad + 1 ;
    int autolen = 60 ;
    int i, j, rows ;
    int rcode ;
    gboolean clipped ;
    double x[100], auto_coefs[101] ;
    static MAT *A=MNULL, *Au=MNULL, *Aut=MNULL, *AutmAu=MNULL, *iAutmAu=MNULL, *final=MNULL ;
/*      static MAT *A, *Au, *Aut, *AutmAu, *iAutmAu, *final ;  */
    static VEC *rhs, *sig, *sig_final ;

    //estimate_region(data, firstbad, lastbad, siglen) ;
    //return REPAIR_SUCCESS ;

    autolen = (siglen-n_bad)/4 ;
    //autolen *= 2 ;

    if(autolen < 0) {
	d_print("Autolen < 0!\n") ;
	return REPAIR_FAILURE;
    }

    if(autolen > 3*n_bad) autolen = 3*n_bad ;
    if(autolen > 100) autolen = 100 ;
    //g_print("siglen:%d n_bad:%d Autolen:%d\n",siglen,n_bad,autolen) ;

    sig = v_get(siglen) ;
    A = m_resize(A,siglen-autolen, siglen) ;
    Au = m_resize(Au, siglen-autolen, n_bad) ;

    for(i = 0 ; i < siglen ; i++)
	sig->ve[i] = data[i] ;

    init_reg(autolen) ;

    for(i = autolen ; i < firstbad ; i++) {
	for(j = 0 ; j < autolen ; j++)
	    x[j] = data[i - autolen + j] ;
	sum_reg(x, data[i]) ;
    }

    for(i = lastbad+autolen+1 ; i < siglen ; i++) {
	for(j = 0 ; j < autolen ; j++)
	    x[j] = data[i - autolen + j] ;
	sum_reg(x, data[i]) ;
    }

    if(estimate_reg(auto_coefs) == 1) {
	rcode = SINGULAR_MATRIX ;
    } else {

	for(i = firstbad ; i <= lastbad ; i++) sig->ve[i] = 0.0 ;

	rows = siglen - autolen ;

	for(i = 0 ; i < rows ; i++) {

	    for(j = 0 ; j < autolen ; j++)
		A->me[i][i+j]= -auto_coefs[j] ;

	    A->me[i][i+autolen] = 1. ;

	    for(j = firstbad ; j <= lastbad ; j++)
		Au->me[i][j-firstbad] = A->me[i][j] ;
	}

	for(j = firstbad ; j <= lastbad ; j++)
	    sig->ve[j] = 0.0 ;

	Aut = m_transp(Au, Aut) ;

	rhs = mv_mlt(A,sig,rhs) ;

	AutmAu = m_mlt(Aut,Au, AutmAu) ;

	iAutmAu = m_inverse(AutmAu, iAutmAu) ;

	final = m_mlt(iAutmAu,Aut, final) ;

	sig_final = mv_mlt(final,rhs, sig_final) ;

	clipped = FALSE ;

	for(j = firstbad ; j <= lastbad ; j++) {
	    double tmp = -sig_final->ve[j-firstbad] ;
	    if(tmp > 1.0) clipped = TRUE ;
	    if(tmp < -1.0) clipped = TRUE ;
	}

	if(clipped == FALSE) {
	    for(j = firstbad ; j <= lastbad ; j++) {
		data[j] = -sig_final->ve[j-firstbad] ;
		if(data[j] > 1.0) data[j] = 1.0 ;
		if(data[j] < -1.0) data[j] = -1.0 ;
	    }
	}

	if(clipped == FALSE)
	    rcode =  REPAIR_SUCCESS ;
	else
	    rcode =  REPAIR_CLIPPED ;
    }

    M_FREE(A) ;
    M_FREE(Au) ;
    M_FREE(Aut) ;
    M_FREE(AutmAu) ;
    M_FREE(iAutmAu) ;
    M_FREE(final) ;
    V_FREE(sig) ;
    V_FREE(sig_final) ;
    V_FREE(rhs) ;

    return rcode ;
#else
    int n_bad = lastbad - firstbad + 1 ;
    int autolen = 60 ;
    int i, j, rows, cols ;
    int rcode ;
    gboolean clipped ;
    double x[100], auto_coefs[101] ;
    static gsl_matrix *A, *Au, *Aut, *AutmAu, *iAutmAu, *final ;
    static gsl_vector *rhs, *sig, *sig_final ;

    //estimate_region(data, firstbad, lastbad, siglen) ;
    //return REPAIR_SUCCESS ;

    autolen = (siglen-n_bad)/4 ;
    //autolen *= 2 ;

    if(autolen < 0) {
	d_print("Autolen < 0!\n") ;
	return REPAIR_FAILURE;
    }

    if(autolen > 3*n_bad) autolen = 3*n_bad ;
    if(autolen > 100) autolen = 100 ;
    //g_print("siglen:%d n_bad:%d Autolen:%d\n",siglen,n_bad,autolen) ;

    sig = gsl_vector_alloc(siglen) ;
    A = gsl_matrix_alloc(siglen-autolen, siglen) ;
    Au = gsl_matrix_alloc(siglen-autolen, n_bad) ;

    for(i = 0 ; i < siglen ; i++)
	gsl_vector_set(sig,i, data[i]) ;

    init_reg(autolen) ;

    for(i = autolen ; i < firstbad ; i++) {
	for(j = 0 ; j < autolen ; j++)
	    x[j] = data[i - autolen + j] ;
	sum_reg(x, data[i]) ;
    }

    for(i = lastbad+autolen+1 ; i < siglen ; i++) {
	for(j = 0 ; j < autolen ; j++)
	    x[j] = data[i - autolen + j] ;
	sum_reg(x, data[i]) ;
    }

    if(estimate_reg(auto_coefs) == 1) {
	rcode = SINGULAR_MATRIX ;
    } else {

	for(i = firstbad ; i <= lastbad ; i++) gsl_vector_set(sig,i,0.0) ;

	rows = siglen - autolen ;
	cols = siglen ;

	for(i = 0 ; i < rows ; i++) {

	    for(j = 0 ; j < autolen ; j++)
		gsl_matrix_set(A,i,i+j,-auto_coefs[j]) ;

	    gsl_matrix_set(A,i,i+autolen, 1.) ;

	    for(j = firstbad ; j <= lastbad ; j++)
		gsl_matrix_set(A,i,j-firstbad, gsl_matrix_get(A,i,j)) ;
	}

	for(j = firstbad ; j <= lastbad ; j++)
	    gsl_vector_set(sig,j, 0.0) ;

	Aut = gsl_transp(Au) ;

	rhs = gsl_mv_mlt(A,sig) ;

	AutmAu = gsl_m_mlt(Aut,Au) ;

	iAutmAu = gsl_m_inverse(AutmAu) ;

	final = gsl_m_mlt(iAutmAu,Aut) ;

	sig_final = gsl_mv_mlt(final,rhs) ;

	clipped = FALSE ;

	for(j = firstbad ; j <= lastbad ; j++) {
	    double tmp = -gsl_vector_get(sig_final,j-firstbad) ;
	    if(tmp > 1.0) clipped = TRUE ;
	    if(tmp < -1.0) clipped = TRUE ;
	}

	if(clipped == FALSE) {
	    for(j = firstbad ; j <= lastbad ; j++) {
		double tmp = -gsl_vector_get(sig_final,j-firstbad) ;
		if(data[j] > 1.0) data[j] = 1.0 ;
		if(data[j] < -1.0) data[j] = -1.0 ;
	    }
	}

	if(clipped == FALSE)
	    rcode =  REPAIR_SUCCESS ;
	else
	    rcode =  REPAIR_CLIPPED ;
    }

    gsl_matrix_free(A) ;
    gsl_matrix_free(Au) ;
    gsl_matrix_free(Aut) ;
    gsl_matrix_free(AutmAu) ;
    gsl_matrix_free(iAutmAu) ;
    gsl_matrix_free(final) ;
    gsl_vector_free(sig) ;
    gsl_vector_free(sig_final) ;
    gsl_vector_free(rhs) ;

    return rcode ;
#endif

}
    

#define DECLICK_CUBIC 0x01
#define DECLICK_LSAR  0x02

int declick_a_click(struct sound_prefs *p, long first_sample, long last_sample, int channel_mask)
{
    long n_samples = last_sample - first_sample + 1;
    long first ;
    int ch, k, last ;
    int click_start, click_end ;
    int repair_method ;
    int FFT_SIZE ;
    int result = REPAIR_FAILURE ;
    fftw_real estimated[FFT_MAX*3], window_coef[FFT_MAX] ;
    fftw_real data[2][FFT_MAX] ;


    /* choose a repair strategy based on the length of the click */

    if(n_samples < 1) {
	d_print("Whoa there, trying to declick %d samples!\n", n_samples) ;
	return 1 ;
    } else if(n_samples < 6) {
	/* cubic function -- interpolation */
	first = first_sample-4 ;
	if(first < 0) first = 0 ;
	last = last_sample+4 ;
	if(last > p->n_samples-1) last = p->n_samples-1 ;
	repair_method = DECLICK_CUBIC ;
    } else {
	/* LSAR */
	first = first_sample-200;
	if(first < 0) first = 0 ;
	last = last_sample+200;
	if(last > p->n_samples-1) last = p->n_samples-1 ;
	repair_method = DECLICK_LSAR ;
    }

    repair_method = DECLICK_LSAR ;

    FFT_SIZE = last-first+1 ;


    read_fft_real_wavefile_data(data[0],  data[1], first, last) ;
    save_undo_data( first, last, p, FALSE) ;

    /* compute  click starting and ending positions in the buffer data_all */
    click_start = first_sample-first ;
    click_end = last_sample-first ;

    for(k = 0 ; k < FFT_SIZE ; k++) {
	window_coef[k] = blackman(k, FFT_SIZE) ;
	window_coef[k] = 1.0 ;
    }

    for(ch = 0 ; ch < 2 ; ch++) {
	if(channel_mask & (ch+1)) {
	    if(repair_method == DECLICK_CUBIC) {
		fit_cubic(data[ch], FFT_SIZE, estimated) ;
/*  		fit_trig_basis(data_all[ch], FFT_SIZE*3, windowed, click_start, click_end) ;  */

		/* merge results back into sample data based on window function */
		for(k = 0 ; k < FFT_SIZE ; k++) {
		    double w = window_coef[k] ;
		    w = blackman(k, FFT_SIZE) ;
		    w = blackman_hybrid(k, click_end-click_start+2, FFT_SIZE) ;
		    data[ch][k] = (1.0-w) * data[ch][k] + w*estimated[k+FFT_SIZE] ;
		}
	    } else
		result = lsar_sample_restore(data[ch], click_start, click_end, FFT_SIZE) ;

	}
    }

    write_fft_real_wavefile_data(data[0],  data[1], first, last) ;

    return result ;
}

/* bj 10/2002
 * WINDOW_SIZE = number of data points to read at a time
 * WINDOW_OVERLAP = number of data points to overlap between windows
 *                  (set this to maximum click size you think reasonable)
 * HPF_AVE_WING_BASE = number of points about current point (each side)
 *                     to use as baseline rms average
 * HPF_AVE_WING_LOCAL = number of points about current point (each side)
 *                      to average to get rms value for current point
 * HPF_DATA_WING = number of points about current point (each side)
 *                 required to get rms value for current point
 *                 (only change this if you change the hpf from 2nd
 *                  derivative to something else)
 * HPF_DELTA_WIDTH = number of previous points used as base to compare
 *                   current change in hpf.  used to detect trailing
 *                   edge of a click
 */
#define WINDOW_SIZE 30000
#define HPF_AVE_WING_BASE 500
#define HPF_AVE_WING_LOCAL 8
#define HPF_DATA_WING 1
#define HPF_DELTA_WIDTH 50
#define WINDOW_OVERLAP 300

#define HPF_AVE_WIDTH_BASE (HPF_AVE_WING_BASE * 2 + 1)
#define HPF_AVE_WIDTH_LOCAL (HPF_AVE_WING_LOCAL * 2 + 1)
#define EXTRA_DATA_WING (HPF_AVE_WING_BASE + HPF_AVE_WING_LOCAL + HPF_DATA_WING)
#define MAX_WINDOW_SIZE (WINDOW_SIZE + EXTRA_DATA_WING * 2)

#define INC_POS(a,b,c) ( ((a)+(b)+(c)) % (c) )

/* maintain running sum of 2nd derivative rms so that we avoid excessive
 * calculation time.
 * 1) local rms value across small number of data points (HPF_AVE_WING_LOCAL
 *    about current point)
 * 2) compare local rms value with HPF_AVE_WING_BASE local rms values about
 *    current point
 *
 * Also used to get change in hpf near a point (pass in sample=-N as flag)
 *
 * do_declick runs backward thru data points, so this does too.
 * To get first real datapoint, have to run through (HPF_AVE_WIDTH_BASE
 * + HPF_AVE_WING_LOCAL) * 2 calculations first in order to fill up
 * the hpfl & hpfb arrays with correct rms values
 */
void get_hpf (long sample, fftw_real channel_data[], double *hpf, double *hpf_ave, double *hpf_dev)
{
	static double hpfl[HPF_AVE_WIDTH_LOCAL];
	static double suml;
	static int posl;
	static double hpfb[HPF_AVE_WIDTH_BASE];
	static double sumb;
	static int posb;


	/* if real sample, get next hpf value into array */
	if (sample >= 0)
	{
		fftw_real *data = &channel_data[sample - (HPF_AVE_WING_BASE+HPF_AVE_WING_LOCAL)];
	
		posl = INC_POS(posl,-1,HPF_AVE_WIDTH_LOCAL);
		posb = INC_POS(posb,-1,HPF_AVE_WIDTH_BASE);
	
		/* get updated sum of rms values of actual data points
		 * and get updated sum of local average of rms values */
		suml -= hpfl[posl];
		hpfl[posl] = data[-1] - 2. * data[0] + data[1];
		hpfl[posl] *= hpfl[posl];
		suml += hpfl[posl];

		/* bugfix -- thanks Paul Sanders 1/12/2007 */
		*hpf = (suml > 0.0) ? sqrt(suml/(HPF_AVE_WIDTH_LOCAL-2)) : 0.0 ;
	
		sumb -= hpfb[posb];
		hpfb[posb] = *hpf;
		sumb += *hpf;
		*hpf_ave = sumb / HPF_AVE_WIDTH_BASE;
	
		/* hpf of current point was read in a while back;
		 * retrieve that value now */
		*hpf = hpfb[INC_POS(posb,HPF_AVE_WING_BASE,HPF_AVE_WIDTH_BASE)];

	}

	/* negative sample; get change in hpf */
	else
	{
		int posF, pos0, n = -sample;
		double x = 0, sumx = 0, sumx2 = 0;

		pos0 = INC_POS(posb,HPF_AVE_WING_BASE+n,HPF_AVE_WIDTH_BASE);
		
		while (sample < 0)
		{
			posF = INC_POS(pos0,-2,HPF_AVE_WIDTH_BASE);
			x = hpfb[posF] - hpfb[pos0];
			sumx += x;
			sumx2 += x*x;

			pos0 = INC_POS(pos0,-1,HPF_AVE_WIDTH_BASE);

			sample++;
		}
		*hpf = x;		/* last value in x is value at "current" position */
		*hpf_ave = sumx / n;
		*hpf_dev = sqrt((sumx2 - sumx*sumx/n)/(n-1));
	}

}

/* bj 10/2002 end, but several changes in do_declick also */

char *do_declick(struct sound_prefs *p, long first_sample, long last_sample, int channel_mask, double sensitivity, int repair,
struct click_data *clicks, int iterate_flag, int leave_click_marks)
{
    extern int declick_detector_type ;

    if(declick_detector_type == FFT_DETECT)
	return do_declick_fft(p,first_sample,last_sample,channel_mask,sensitivity,repair,clicks,iterate_flag,leave_click_marks) ;
    else
	return do_declick_hpf(p,first_sample,last_sample,channel_mask,sensitivity,repair,clicks,iterate_flag,leave_click_marks) ;
}

#define DECLICK_MAX_FFT 128
char *do_declick_fft(struct sound_prefs *p, long first_sample, long last_sample, int channel_mask, double sensitivity, int repair,
struct click_data *clicks, int iterate_flag, int leave_click_marks)
{
    static char results_buf[200] ;
    long window_first ;
    long i,k ;
    int FFT_SIZE = 64 ;
    int n_repaired[2] , n_this_pass ;
    int n_not_repaired[2] ;
    char max_exceeded_notice = 0 ;
#define FFT_WINDOW 1000
    char level[2][2*FFT_WINDOW+1][DECLICK_MAX_FFT] ;
#ifdef HAVE_FFTW3
    FFTW(plan) pLeft, pRight ;
#else /* HAVE_FFTW3 */
    rfftw_plan fftw_p ;
#endif /* HAVE_FFTW3 */

    fftw_real data[2][2*FFT_WINDOW+1] ;
    fftw_real out[2*DECLICK_MAX_FFT] ;
    fftw_real window[2*DECLICK_MAX_FFT] ;
    fftw_real power_spectrum[2*DECLICK_MAX_FFT] ;

    int in_click ;
    int window_size  = FFT_WINDOW * MIN((p->rate/44100.0),2) ;
    int window_step ;
    int channel ;
    int done = 0 ;

    if(last_sample > p->n_samples-21) last_sample = p->n_samples-21 ;
    if(first_sample < 20) first_sample = 20 ;
    if(first_sample >= last_sample) {
	return "Region to small to declick." ;
    }

    start_timer();

    if(repair == FALSE || leave_click_marks == FALSE)
	clicks->n_clicks = 0 ;

    n_repaired[0] = n_repaired[1] = 0 ;
    n_not_repaired[0] = n_not_repaired[1] = 0 ;

    window_step = 700 ;
    window_size = 801 ;

    g_print("Declick_fft first_sample:%ld last_sample:%ld window_size:%d\n", first_sample, last_sample, window_size) ;
    update_progress_bar(0.0,PROGRESS_UPDATE_INTERVAL,TRUE) ;


#ifdef HAVE_FFTW3
    pLeft = FFTW(plan_r2r_1d)(FFT_SIZE, data[0], out, FFTW_R2HC, FFTW_ESTIMATE);
    pRight = FFTW(plan_r2r_1d)(FFT_SIZE, data[1], out, FFTW_R2HC, FFTW_ESTIMATE);
#else /* HAVE_FFTW3 */
    fftw_p = rfftw_create_plan(FFT_SIZE, FFTW_REAL_TO_COMPLEX, FFTW_ESTIMATE);
#endif /* HAVE_FFTW3 */

    for(k = 0 ; k < FFT_SIZE ; k++) {
	 window[k] = blackman(k,FFT_SIZE) ;
    }

    for(window_first = first_sample ; !done && window_first < last_sample ; window_first += window_step ) {
	int min_sample,max_sample ;

	if(window_first + window_size > last_sample) {
	    window_first = last_sample - window_size ;
	    done = 1 ;
	}

	double percentage = (double)(window_first-first_sample)/(double)(last_sample-first_sample) ;
	update_progress_bar(percentage,PROGRESS_UPDATE_INTERVAL,FALSE) ;

	for(i=0 ; i < window_size ; i += 2) {

	    min_sample = i+window_first-FFT_SIZE/2 ;
	    min_sample = MAX(0, min_sample) ;
	    max_sample = min_sample+FFT_SIZE-1 ;

	    if(max_sample > p->n_samples-1)
		max_sample = p->n_samples-1 ;

	    read_fft_real_wavefile_data(data[0],  data[1], min_sample, max_sample) ;

	    for(k = 0 ; k < FFT_SIZE ; k++) {
		data[0][k] *= window[k] ;
		data[1][k] *= window[k] ;
	    }

	    for (channel = 0; channel < 2; channel++) {
		double min_p = 1.e30, max_p = -1.e30 ;
		if(! ((channel+1) & channel_mask) ) continue ;

#ifdef HAVE_FFTW3
		if (channel == 0)
		   FFTW(execute)(pLeft);
		else
		   FFTW(execute)(pRight);
#else /* HAVE_FFTW3 */
		if (channel == 0)
		   rfftw_one(fftw_p, data[0], out);
		else
		   rfftw_one(fftw_p, data[1], out);
#endif /* HAVE_FFTW3 */

		power_spectrum[0] = out[0]*out[0];  /* DC component */

		for (k = 1; k < (FFT_SIZE+1)/2; ++k)  /* (k < FFT_SIZE/2 rounded up) */
		    power_spectrum[k] = out[k]*out[k] + out[FFT_SIZE-k]*out[FFT_SIZE-k];

		if (FFT_SIZE % 2 == 0) /* N is even */
		    power_spectrum[FFT_SIZE/2] = (out[FFT_SIZE/2]*out[FFT_SIZE/2]);  /* Nyquist freq. */

		for(k = 1 ; k <= FFT_SIZE/2 ; k++) {
		    double p = 10.0*log10(power_spectrum[k]) ;

		    if(p < -127.0) p = -127.0 ;
		    if(p > 127.0) p = 127.0 ;

		    if(p > max_p) max_p = p ;
		    if(p < min_p) min_p = p ;
		    
		    level[channel][i][k-1] = (char)p ;
		}
	    }
	}

	for(channel = 0 ; channel < 2 ; channel++) {
	    double mean_level[FFT_SIZE] ;
	    double offset[FFT_WINDOW*2+1] ;
	    double hgt_sum = 0 ;
	    double mean, std_err, var, cv, stddev ;
	    long click_start = 0 ;

	    if(! ((channel+1) & channel_mask) ) continue ;

	    for(k = 0 ; k < FFT_SIZE/2 ; k++) {
		mean_level[k] = 0.0 ;
		for(i = 0 ; i < window_size ; i += 2)
		    mean_level[k] += level[channel][i][k] ;
		mean_level[k] /= (double)window_size ;
	    }

	    for(i = 0 ; i < window_size ; i += 2) {
		offset[i] = 0.0 ;
		for(k = 0 ; k < FFT_SIZE/2 ; k++) {
		    offset[i] += level[channel][i][k] - mean_level[k] ;
		}

		offset[i] /= (double)FFT_SIZE/2.0 ;
	    }

	    for(i = 1 ; i < window_size ; i += 2) {
		offset[i] = (offset[i-1]+offset[i+1])/2.0 ;
	    }

/*  	    for(i = 0 ; i < window_size ; i++) {  */
/*  		if(offset[i] < 0.0) offset[i] = 0.0 ;  */
/*  	    }  */

	    stats(offset, window_size, &mean, &std_err, &var, &cv, &stddev) ;

	    in_click = 0 ;

	    for(i = 0 ; i < window_size ; i++) {
		//double z = (offset[i]-mean)/stddev ;
		double z = (offset[i]-mean) ;

		//printf("i:%d z:%lg\n", i, z) ;

		if(z < 0.0) z = 0.0 ;

		if(z > 1.e-30) {
		    if(!in_click) {
			in_click = 1 ;
			click_start = i+window_first ;
			hgt_sum = z ;
		    } else {
			hgt_sum += z ;
		    }
		} else {
		    if(in_click) {
			int click_end = i-1+window_first;
			double click_width = (click_end - click_start+1) ;
			int click_mid = (click_start + click_width/2+0.5) ;
			int result = DETECT_ONLY ;

			double mean_hgt = hgt_sum / click_width ;

			if(click_width > 8) {
			    click_start += click_width/4.0+0.5 ;
			    /* leave the end, more audio artifacts there that need fixed */
/*  			    click_end -= click_width/4.0+0.5 ;  */
			    click_width = (click_end - click_start+1) ;
			}


			if(mean_hgt > sensitivity && click_width < 100) {

			    printf("%d %d %lg\n", channel, click_mid, mean_hgt) ;

			    if(repair == TRUE) {
				result = declick_a_click(p, click_start, click_end, channel+1) ;

				if(result == SINGULAR_MATRIX) {
				    click_start -= 10 ;
				    click_end += 10 ;
				    click_start = MAX(0, click_start) ;
				    click_end = MIN(p->n_samples-1, click_start) ;
				    result = declick_a_click(p, click_start, click_end, channel+1) ;
				}
				    
			    }
			} else {
			    result = REPAIR_OOB ;
			}

			if(result == REPAIR_OOB) {
			} else if(result != REPAIR_SUCCESS) {
			    if(clicks->n_clicks < clicks->max_clicks) {
				clicks->start[clicks->n_clicks] = click_start ;
				clicks->end[clicks->n_clicks] = click_end ;
				clicks->channel[clicks->n_clicks] = channel ;
				clicks->n_clicks++ ;
			    } else {
				if( max_exceeded_notice == 0) {
				    warning("Exceeded 1000 clicks in selection, additional detections not marked") ;
				    max_exceeded_notice = 1 ;
				}
			    }
			    n_not_repaired[channel]++ ;
			} else {
			    n_repaired[channel]++ ;
			    n_this_pass ++ ;
			}
		    }
		    in_click = 0 ;
		}

	    }
	}
    }

    #ifdef HAVE_FFTW3
	FFTW(destroy_plan)(pLeft);
	FFTW(destroy_plan)(pRight);
    #else /* HAVE_FFTW3 */
	rfftw_destroy_plan(fftw_p);
    #endif /* HAVE_FFTW3 */


    {

	sprintf(results_buf, "%d clicks repaired, %d clicks marked, but remain unrepaired",
	    n_repaired[0] + n_repaired[1],
	    n_not_repaired[0] + n_not_repaired[1]) ;
        g_print("%s\n",results_buf);
        stop_timer("DECLICK");


/*  	warning(results_buf) ;  */
    }

    update_progress_bar(0.0,PROGRESS_UPDATE_INTERVAL,TRUE) ;

    return results_buf ;
}

char *do_declick_hpf(struct sound_prefs *p, long first_sample, long last_sample, int channel_mask, double sensitivity, int repair,
struct click_data *clicks, int iterate_flag, int leave_click_marks)
{
    static char results_buf[200] ;
    long min_sample, max_sample ;
    long sample ;
    long window_first ;
    long i ;
    long n_samples = last_sample - first_sample + 1 ;
    int n_repaired[2] , n_this_pass, n_last_pass;
    int n_not_repaired[2] ;
    int n ;
    int offset0,offsetF;
    char max_exceeded_notice = 0 ;

    fftw_real left[2*MAX_WINDOW_SIZE+1], right[2*MAX_WINDOW_SIZE+1], *pdata[2] ;
    double hpf, hpf_ave, hpf_dev;

    int in_click ;
    int window_size  = MAX_WINDOW_SIZE * MIN((p->rate/44100.0),2) ;
    int window_overlap  = WINDOW_OVERLAP * MIN((p->rate/44100.0),2) ;
    int window_step ;
    int channel ;

    if(last_sample > p->n_samples-21) last_sample = p->n_samples-21 ;
    if(first_sample < 20) first_sample = 20 ;
    if(first_sample >= last_sample) {
	return "Region to small to declick." ;
    }

    start_timer();

    if(repair == FALSE || leave_click_marks == FALSE)
	clicks->n_clicks = 0 ;

    n_repaired[0] = n_repaired[1] = 0 ;
    n_not_repaired[0] = n_not_repaired[1] = 0 ;

    if(window_size > n_samples + 2 * EXTRA_DATA_WING) window_size = n_samples + 2 * EXTRA_DATA_WING;

    if (p->n_samples < EXTRA_DATA_WING+1) return "Audio file too small.";

    window_step = window_size - 2*EXTRA_DATA_WING - window_overlap ;
    if (window_step < 1) {window_step = window_size;}

    //g_print("Declick first_sample:%ld last_sample:%ld window_size:%d\n", first_sample, last_sample, window_size) ;
    update_progress_bar(0.0,PROGRESS_UPDATE_INTERVAL,TRUE) ;

    for(window_first = first_sample ; window_first < last_sample ; window_first += window_step ) {
	int nclicks_not_repaired_start = clicks->n_clicks ;
	int clicks_repaired = 1 ;
	int loop_flag = 1 ;

	double percentage = (double)(window_first-first_sample)/(double)(last_sample-first_sample) ;
	update_progress_bar(percentage,PROGRESS_UPDATE_INTERVAL,FALSE) ;


	n_last_pass = INT_MAX;
	while(loop_flag) {
	    clicks->n_clicks = nclicks_not_repaired_start ;

	    clicks_repaired = 0 ;
	    n_this_pass = 0 ;

	    /* read some to the left & right of our window so that we can get
	     * local averages even at the ends of our window */
	    min_sample = window_first - EXTRA_DATA_WING;
	    offset0 = -MIN(min_sample,0);
	    min_sample += offset0;

	    max_sample = window_first + window_size-1 - EXTRA_DATA_WING;
	    offsetF = MAX(max_sample - p->n_samples+1,0);
	    max_sample -= offsetF;
	    offsetF = MIN(offsetF,EXTRA_DATA_WING);

	    n = read_fft_real_wavefile_data(&left[offset0], &right[offset0], min_sample, max_sample);

	    /* Now fill in any gaps; if we were too close to sample 0, mirror about zero
	       and if too close to the last sample, mirror about last sample */
	    for (i = 1; i <= offset0; i++)
	    {
		left[offset0-i] = -left[offset0+i];
		right[offset0-i] = -right[offset0+i];
	    }
	    sample = offset0 + n - 1;
	    for (i = 1; i <= offsetF; i++)
	    {
		left[sample+i] = -left[sample-i];
		right[sample+i] = -right[sample-i];
	    }

	    /* set up arrays where element 0 is window_first sample */
	    pdata[0] = &left[EXTRA_DATA_WING];
	    pdata[1] = &right[EXTRA_DATA_WING];
	    for(channel = 0 ; channel < 2 ; channel++) {
		long click_start, click_end=0 ;

		if(! ((channel+1) & channel_mask) ) continue ;

		in_click = 0 ;

		sample = offset0 + n-1 + offsetF - HPF_DATA_WING * 2;
		for(i = sample; i >= 0  ; i--) {

		    get_hpf(i,pdata[channel],&hpf,&hpf_ave,&hpf_dev); 

		    if(i <= sample - 2*(HPF_AVE_WING_BASE+HPF_AVE_WING_LOCAL)) {
			int sample_is_in_click = 0 ;
			if(hpf > 2.0*hpf_ave/sensitivity) sample_is_in_click = 1 ;
#define test_clicks_as_dB 1
			if(test_clicks_as_dB) {
			    sample_is_in_click = 0 ;
			    if(10.0*log10(hpf/hpf_ave) > sensitivity) sample_is_in_click = 1 ;
			}

			if(in_click == 0 && sample_is_in_click) {
			    get_hpf(-HPF_DELTA_WIDTH,pdata[channel],&hpf,&hpf_ave,&hpf_dev);
			    if (hpf > 2. * hpf_dev/sensitivity + hpf_ave) {
				in_click = 1 ;
				click_end = window_first + i ;
			    }
			} else if(in_click == 1 && !sample_is_in_click) {
			    int result = DETECT_ONLY ;
			    click_start = window_first+i ;

			    if(click_start < 0) click_start = 0 ;
			    if(click_end > p->n_samples-1) click_end = p->n_samples-1 ;

			    if(click_start >= first_sample && click_end <= last_sample) {
				if(repair == TRUE) {
				    //g_print("Repairing %s start:%ld end:%ld\n", channel == 0 ? "left" : "right", click_start, click_end) ;
    /*  				push_status_text("Repairing a click") ;  */
    /*  				update_progress_bar(percentage,PROGRESS_UPDATE_INTERVAL,TRUE) ;  */
				    result = declick_a_click(p, click_start, click_end, channel+1) ;

				    if(result == SINGULAR_MATRIX) {
					click_start -= 10 ;
					click_end += 10 ;
					click_start = MAX(0, click_start) ;
					click_end = MIN(p->n_samples-1, click_start) ;
					result = declick_a_click(p, click_start, click_end, channel+1) ;
				    }
					
    /*  				pop_status_text() ;  */
    /*  				update_progress_bar(percentage,PROGRESS_UPDATE_INTERVAL,TRUE) ;  */
				}
			    } else {
				result = REPAIR_OOB ;
			    }

			    if(result == REPAIR_OOB) {
			    } else if(result != REPAIR_SUCCESS) {
				if(clicks->n_clicks < clicks->max_clicks) {
				    clicks->start[clicks->n_clicks] = click_start ;
				    clicks->end[clicks->n_clicks] = click_end ;
				    clicks->channel[clicks->n_clicks] = channel ;
				    clicks->n_clicks++ ;
				} else {
				    if( max_exceeded_notice == 0) {
					warning("Exceeded 1000 clicks in selection, additional detections not marked") ;
					max_exceeded_notice = 1 ;
				    }
				}
				n_not_repaired[channel]++ ;
			    } else {
				n_repaired[channel]++ ;
				n_this_pass ++ ;
				clicks_repaired = 1 ;
			    }

			    in_click = 0 ;
			} /* if (in_click... */
		    } /* if (sample <= ... */
		} /* for (...sample... */

		/* bj 10/30/02 if in middle of click at window_first, mark it
		 * as not repaired */
		if (in_click && clicks->n_clicks < clicks->max_clicks)
		{
		    clicks->start[clicks->n_clicks] = window_first;
		    clicks->end[clicks->n_clicks] = click_end;
		    clicks->channel[clicks->n_clicks] = channel;
		    clicks->n_clicks++;
		    n_not_repaired[channel]++;
		}
	    } /* for (channel... */

	    /* stopping criteria for this declick window */
	    loop_flag = 0 ;
	    if(iterate_flag && clicks_repaired &&
	       n_this_pass < n_last_pass) loop_flag = 1 ;
	    n_last_pass = n_this_pass;
	} /* while (loop_flag) */
    } /* for (window_first... */

    d_print("channel_mask: %d\n", channel_mask) ;

    {

	sprintf(results_buf, "%d clicks repaired, %d clicks marked, but remain unrepaired",
	    n_repaired[0] + n_repaired[1],
	    n_not_repaired[0] + n_not_repaired[1]) ;
        g_print("%s\n",results_buf);
        stop_timer("DECLICK");


/*  	warning(results_buf) ;  */
    }

    update_progress_bar(0.0,PROGRESS_UPDATE_INTERVAL,TRUE) ;

    return results_buf ;
}

void fit_cubic(fftw_real data[], int n, fftw_real estimated[])
{
    int one_fourth = n / 4 ;
    int leftmin = 0 ;
    int leftmax = leftmin + (one_fourth-1) ;
    int rightmax = n-1 ;
    int rightmin = rightmax - (one_fourth-1) ;
    int n_sel = leftmax-leftmin + rightmax - rightmin + 2 ;

    if(n_sel < 3 ) {
	double B[2] ;
	int i ;
	double x[1] ;

	init_reg(1) ;

	for(i = leftmin ; i <= leftmax ; i++) {
	    x[0] = i ;
	    sum_reg(x, data[i]) ;
	}

	for(i = rightmin ; i <= rightmax ; i++) {
	    x[0] = i ;
	    sum_reg(x, data[i]) ;
	}

	estimate_reg(B) ;

	for(i = leftmin ; i <= rightmax ; i++)
	    estimated[i] = B[0] +B[1]*i ;

	return ;
    }

    if(n_sel < 4) {
	double B[3] ;
	int i ;
	double x[2] ;

	init_reg(2) ;

	for(i = leftmin ; i <= leftmax ; i++) {
	    x[0] = i ;
	    x[1] = i*i ;
	    sum_reg(x, data[i]) ;
	}

	for(i = rightmin ; i <= rightmax ; i++) {
	    x[0] = i ;
	    x[1] = i*i ;
	    sum_reg(x, data[i]) ;
	}

	estimate_reg(B) ;

	for(i = leftmin ; i <= rightmax ; i++)
	    estimated[i] = B[0] +B[1]*i +B[2]*i*i ;

	return ;
    }

    if(1) {
	double B[4] ;
	int i ;
	double x[3] ;

	init_reg(3) ;

	for(i = leftmin ; i <= leftmax ; i++) {
	    x[0] = i ;
	    x[1] = i*i ;
	    x[2] = i*i*i ;
	    sum_reg(x, data[i]) ;
	}

	for(i = rightmin ; i <= rightmax ; i++) {
	    x[0] = i ;
	    x[1] = i*i ;
	    x[2] = i*i*i ;
	    sum_reg(x, data[i]) ;
	}

	estimate_reg(B) ;

	for(i = leftmin ; i <= rightmax ; i++)
	    estimated[i] = B[0] +B[1]*i + B[2]*i*i + B[3]*i*i*i ;

    }
}
