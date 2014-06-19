calcmask
{
    double *H ;
    double *X ;

    if(!use_existing) {
	compute_xicalc()
    }

    H = current_xicalc ;

    X = vmult(H,Y) ;

    johnston_mask(X) ;
}

#define N_BANDS 26

int crit_bands[N_BANDS] = {0,100,200,300,400,510,630,770,920,1080,1270,1480,1720,2000,2320,2700,3150,3700,4400,5300,6400,7700,9500,12000,15500,1.e30};
int imax ;
double thr_hearing_val[N_BANDS] = {38,31,22,18.5,15.5,13,11,9.5,8.75,7.25,4.75,2.75,1.5,0.5,0,0,0,0,2,7,12,15.5,18,24,29} ;
double abs_thresh[N_BANDS] ;


johnston_init
{
    int i ;

    for(imax = 0 ; imax < N_BANDS ; imax++)
	if(crit_bands[imax] > max_frequency) break ;

    for(i = 0 ; i < N_BANDS ; i++)
	abs_thresh[i] = pow(10.0, thr_hearing_val[i]/10.0) ;

    OFFSET_RATIO_DB = 9+(1:imax)';

    n = fft_size ;

    lin_to_bark = (double *) calloc(n*imax, sizeof(double)) ;

    for(j = 0 ; j < n ; i++) {
	for(i = 0 ; i < imax ; i++)
	    lin_to_bark[j*n+i] = 0.0 ;
    }

    i = 0 ;

    for(j = 0 ; j < n ; i++) {
	while(! ((freq[j] >= crit_bands[i]) && (freq[j] < crit_bands[i
	for(i = 0 ; i < imax ; i++)
	    lin_to_bark[j*n+i] = 0.0 ;
    }



}


johnston_mask
{



