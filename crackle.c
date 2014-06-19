#include <stdio.h>
#include <stdlib.h>
#include "gwc.h"

#define NMAX 201

double drand48(void)
{
    return (double)rand()/(double)RAND_MAX ;
}

int main(int argc, char **argv)
{
    double t[NMAX] ;
    double y[NMAX] ; /* sinusoidal y value */
    double ypr[NMAX] ; /* y + occasional random component */
    double y_hat[NMAX] ; /* estimated original y */
    double wgt[NMAX] ;
    double factor ;
    double x ;
    int i ;


    for(i = 0 ; i < NMAX ; i++) {
	t[i] = (double)i/(double)(NMAX-1) ;
	y[i] = sin(t[i]*2.0*M_PI) ;

	if(drand48() > 0.8)
	    ypr[i] = y[i] + 0.9*(drand48()-0.5) ;
	else
	    ypr[i] = y[i] ;
    }

    for(factor = 0.01 ; factor < 0.05 ; factor += 0.01) {
	for(i = 0 ; i < NMAX ; i++) {
	    double dy0 = 0 ;
	    double dy1 = 0 ;
	    double dy2dx = 0 ;
	    if(i < 2) {
		y_hat[i] = ypr[i] ;
		wgt[i] = 1.0 ;
	    } else {
		dy0 = y_hat[i-1] - y_hat[i-2] ;
		dy1 = ypr[i-0] - y_hat[i-1] ;
		dy2dx = dy1 - dy0 ;

		if(dy2dx >  factor) {
		    wgt[i] = factor/dy2dx ;
		    dy2dx = factor ;
		    dy1 = dy2dx + dy0 ;
/*  		    dy1 = (1.0 + factor)*dy0 ;  */
		    y_hat[i] = dy1 + y_hat[i-1] ;
		} else if(dy2dx < -factor) {
		    wgt[i] = -factor/dy2dx ;
		    dy2dx = -factor ;
		    dy1 = dy2dx + dy0 ;
/*  		    dy1 = (1.0 - factor)*dy0 ;  */
		    y_hat[i] = dy1 + y_hat[i-1] ;
		} else {
		    y_hat[i] = ypr[i] ;
		    wgt[i] = 1.0 ;
		}
	    }

	}

	for(i = 0 ; i < NMAX ; i++) {
	    int first, last, j, n=0 ;
	    int w = 4 ;
	    double sum = 0.0 ;
	    double sumwgt = 0.0 ;
	    double dy0 = 0 ;
	    double dy1 = 0 ;
	    double dy2dx = 0 ;

	    if(i >= 2) {
		dy0 = y_hat[i-1] - y_hat[i-2] ;
		dy1 = y_hat[i-0] - y_hat[i-1] ;
		dy2dx = dy1 - dy0 ;
	    }

	    first = i-w ;
	    if(first < 0) first = 0 ;
	    last = i+w ;
	    if(last > NMAX-1) last = NMAX-1 ;

	    for(j = first ; j <= last ; j++) {
		sum += y_hat[j]*wgt[j] ;
		sumwgt += wgt[j] ;
	    }

	    printf("%5.3lf %3d %lg %lg %lg %lg %lg %lg\n", factor, i, y[i], ypr[i], sum/sumwgt, dy0, dy1, dy2dx) ;
	}
    }
}
