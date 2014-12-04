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

/* decrackle.c */

#include <stdlib.h>
#include "gtkledbar.h"
#include "gwc.h"

int do_decrackle(struct sound_prefs *pPrefs, long first_sample, 
		 long last_sample, int channel_mask, double factor,
		 gint nmax, gint width) {
  long i ;
  long current ;
  long final, last_read ;
  long n ;
  int ch ;
  double scaled_factor, absum ;
  fftw_real *y[2], *y_hat[2], *y_hat_tmp;

  int *cflag /*[ASIZE]*/, asize ;
  int  scount;

  asize=nmax+2*width;

  for (ch = 0; ch < 2; ch++) {
    y[ch] = calloc(asize, sizeof(fftw_real));
    y_hat[ch] = calloc(asize, sizeof(fftw_real));
  }
  y_hat_tmp = calloc(asize, sizeof(fftw_real));
  cflag = calloc(asize, sizeof(int));

  /*  scaled_factor = factor * pPrefs -> max_allowed; */

  push_status_text("Decrackling audio") ;
  update_progress_bar(0.0,PROGRESS_UPDATE_INTERVAL,TRUE) ;

  for (current = first_sample; 
       current < last_sample;
       current += nmax) {


    gfloat p = (gfloat)(current-first_sample)/
      (gfloat)(last_sample-first_sample) ;
    update_progress_bar(p,PROGRESS_UPDATE_INTERVAL,FALSE) ;

    final = current+nmax-1;
    if (final > last_sample) final=last_sample;
    last_read = final+width;
    if (last_read >= pPrefs -> n_samples) last_read = pPrefs -> n_samples - 1;

/*      printf ("[%d - %d], start  @ %d, end @ %d (%d)\n", */
/*  	    first_sample, last_sample, current, final, last_read); */
/*      printf ("%d %d %d\n", pPrefs->n_samples, pPrefs->bits, pPrefs->stereo); */

    n = read_fft_real_wavefile_data(y[0]+width,  y[1]+width,
				    current, last_read);

/*      printf ("Got %d samples\n", n); */

    /* Fill anything beyond the end of the file with zeros */

    if (n < final + width + 1 - current) {
/*        printf ("Filling\n");  */
      for (i = current+n; i <= final; i++) {
	for (ch = 0; ch < 2; ch ++) {
	  y[ch][i] = 0.;
	}
      }
/*        printf ("Filled\n");  */
    }
    for(ch = 0 ; ch < 2 ; ch++) {
      if((ch+1) & channel_mask) {
	scount = 0 ;
	absum = 0. ;
	for (i = width+1; i<n ; i++) {
	  /*	  absum +=  fabs(y[ch][i]); */
	  absum += fabs(y[ch][i] - y[ch][i-1]) ;
	  scount++ ;
	}
	scaled_factor = factor *absum/scount;
       

	for(i = 0 ; i < n+width ; i++) {
	  double dy0 = 0 ;
	  double dy1 = 0 ;
	  double dy2dx = 0 ;

	  y_hat[ch][i] = y[ch][i] ;
	  if(i < 2) {
	    /* wgt[i] = 1.0 ; */
	    cflag[i]=0;
	  } else {
	    dy0 = y_hat[ch][i-1] - y_hat[ch][i-2] ;
	    dy1 = y[ch][i-0] - y_hat[ch][i-1] ;
	    dy2dx = dy1 - dy0 ;

	    if(dy2dx >  scaled_factor) {
	      cflag[i-1]=1;
	    } else if(dy2dx < -scaled_factor) {
	      cflag[i-1]=1;
	    } else {
	      cflag[i-1]=0;
	    }
	  }
	}

	for(i = width ; i < n ; i++) {
	  int first, last, j;
	  int w = width ;
	  int flag =0;
	  double sum = 0.0 ;
	  double sumwgt = 0.0 ;

	  first = i-w ;
	  if(first < 0) {
	    first = 0 ;
	    printf("first < 0\n");
	  }
	  last = i+w ;
	  if(last > asize-1) {
	    last = asize-1 ;
	    printf("last > asize\n");
	  }


	  for(j = first ; j <= last ; j++) {
	    sum += y_hat[ch][j];
	    sumwgt ++;
	  }
	  flag = cflag[i-1] | cflag[i] | cflag[i+1];

	  if (cflag[i]) { 
	    y_hat_tmp[i] = sum/sumwgt ;
	  } else if (flag) {
	    y_hat_tmp[i] = (sum/sumwgt+ y_hat[ch][i])/2.;
	  } else {
	    y_hat_tmp[i] = y_hat[ch][i] ;
	  }
	}

	for(i = 0 ; i < n+width ; i++) {
	  y_hat[ch][i] = y_hat_tmp[i] ;
	}
      } else {
	for(i = 0 ; i < n+width ; i++) {
	  y_hat[ch][i] = y[ch][i] ;
	}
      }
    }
    write_fft_real_wavefile_data(y_hat[0]+width,  y_hat[1]+width,
				 current, final) ;

    /* Copy the last width points of this window to the "pre-window" of
       the next window */

    for (i = 0; i < width; i++) {
      for (ch = 0; ch < 2; ch ++) {
	y_hat[ch][i] = y_hat[ch][i+n-width];
	y[ch][i] = y[ch][i+n-width];
      }
    }
  }


  update_progress_bar(0.0,PROGRESS_UPDATE_INTERVAL,TRUE) ;

  pop_status_text() ;

  main_redraw(FALSE, TRUE) ;

  for (ch = 0; ch < 2; ch++) {
    free(y[ch]) ;
    free(y_hat[ch]);
  }
  free(y_hat_tmp);
  free(cflag);

  return 0;
}
