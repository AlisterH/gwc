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

/*	FILE - sample_block.c
	PURPOSE - handle functions related to the sample block
	          facility which speeds audio data display
*/

#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include "gwc.h"

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

struct sample_block *sample_buffer = NULL ;

static size_t sb_size ;
static long n_blocks ;
extern gchar wave_filename[255] ;
extern long n_markers, markers[] ;
extern long num_song_markers, song_markers[] ;
extern long cdtext_length;
extern char *cdtext_data;

void ll_read(int fd, void *p, int n, int *pN_tot, int *pN_read)
{
    *pN_tot += n ;
    *pN_read += read(fd, p, n) ;
}

void ll_write(int fd, void *p, int n, int *pN_tot, int *pN_read)
{
    *pN_tot += n ;
    *pN_read += write(fd, p, n) ;
}

void save_sample_block_data(struct sound_prefs *p)
{
    char buf[1000] ;
    char l ;
    int fd ;
    int n_bytes_tot = 0 ;
    int n_bytes_written = 0 ;
    sprintf(buf, "%s.gwc", wave_filename) ;
    fd = open(buf, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR|S_IWUSR) ;

    sprintf(buf, "gwc %d %d", GWC_VERSION_MAJOR, GWC_VERSION_MINOR) ;
    l = (char)strlen(buf) ;
    LL_WRITE(fd, (void *)&l, 1, n_bytes_tot, n_bytes_written) ;
    LL_WRITE(fd, (void *)buf, l, n_bytes_tot, n_bytes_written) ;

    sprintf(buf, "%d %ld %d", p->n_channels,p->n_samples,p->rate) ;
    l = (char)strlen(buf) ;
    LL_WRITE(fd, (void *)&l, 1, n_bytes_tot, n_bytes_written) ;
    LL_WRITE(fd, (void *)buf, l, n_bytes_tot, n_bytes_written) ;

    n_blocks = p->n_samples / SBW ;
    n_blocks += (p->n_samples - n_blocks*SBW > 0 ? 1 : 0) ;
    sb_size = n_blocks*sizeof(struct sample_block) ;

    n_bytes_tot += sb_size ;
    n_bytes_written += write(fd, (void *)sample_buffer, sb_size) ;

    LL_WRITE(fd, (void *)&n_markers, sizeof(long), n_bytes_tot, n_bytes_written) ;
    LL_WRITE(fd, (void *)markers, sizeof(long)*n_markers, n_bytes_tot, n_bytes_written) ;

    LL_WRITE(fd, (void *)&num_song_markers, sizeof(long), n_bytes_tot, n_bytes_written) ;
    LL_WRITE(fd, (void *)song_markers, sizeof(long)*num_song_markers, n_bytes_tot, n_bytes_written) ;

    LL_WRITE(fd, (void *)&cdtext_length, sizeof(long), n_bytes_tot, n_bytes_written) ;
    LL_WRITE(fd, (void *)cdtext_data, cdtext_length, n_bytes_tot, n_bytes_written) ;
    close(fd) ;

    if(n_bytes_tot != n_bytes_written)
	warning("Writing sample blocks had a problem") ;
}

int load_sample_block_data(struct sound_prefs *p)
{
    char buf[1000] ;
    int fd ;
    int n_bytes_tot = 0 ;
    int n_bytes_read = 0 ;
    char l ;

    sprintf(buf, "%s.gwc", wave_filename) ;

    fd = open(buf, O_RDONLY) ;

    if(fd != -1) {
	int n_channels, rate ;
	long n_samples ;
	    int v_maj, v_min ;

	LL_READ(fd, (void *)&l, 1, n_bytes_tot, n_bytes_read) ;
	LL_READ(fd, (void *)buf, l, n_bytes_tot, n_bytes_read) ;
	buf[(int)l] = '\0' ;

	if(buf[0] != 'g' || buf[1] != 'w' || buf[2] != 'c') {
	    close(fd) ;
	    return 0 ;
	}
	
	sscanf(buf, "%*s%d%d", &v_maj,&v_min) ;
	if(v_maj >= 0 && v_min >= 17) {
	    /* life is good, no need to recreate block data file */
	} else {
	    /* Need to recreate block data file, this one is from an older version */
	    close(fd) ;
	    return 0 ;
	}

	LL_READ(fd, (void *)&l, 1, n_bytes_tot, n_bytes_read) ;
	LL_READ(fd, (void *)buf, l, n_bytes_tot, n_bytes_read) ;
	buf[(int)l] = '\0' ;
	sscanf(buf, "%d%ld%d", &n_channels,&n_samples,&rate) ;

	if(n_channels != p->n_channels || n_samples != p->n_samples || rate != p->rate) {
	    /* something has changed, we must rebuild the sample block data */
	    printf("n_chan wav:%d gwc:%d\n", p->n_channels, n_channels) ;
	    printf("rate wav:%d gwc:%d\n", p->rate, rate) ;
	    printf("n_samples wav:%ld gwc:%ld\n", p->n_samples, n_samples) ;
	    close(fd) ;
	    return 0 ;
	}

	n_blocks = p->n_samples / SBW ;
	n_blocks += (p->n_samples - n_blocks*SBW > 0 ? 1 : 0) ;
	sb_size = n_blocks*sizeof(struct sample_block) ;

	LL_READ(fd, (void *)sample_buffer, sb_size, n_bytes_tot, n_bytes_read) ;

	if(v_maj >= 0 && v_min >= 18) {
/*  	    int i ;  */
	    LL_READ(fd, (void *)&n_markers, sizeof(long), n_bytes_tot, n_bytes_read) ;
	    LL_READ(fd, (void *)markers, sizeof(long)*n_markers, n_bytes_tot, n_bytes_read) ;
/*  	    for(i = 0 ; i < n_markers ; i++)  */
/*  		g_print("marker:%ld\n", markers[i]) ;  */

	    LL_READ(fd, (void *)&num_song_markers, sizeof(long), n_bytes_tot, n_bytes_read) ;
	    LL_READ(fd, (void *)song_markers, sizeof(long)*num_song_markers, n_bytes_tot, n_bytes_read) ;

	    LL_READ(fd, (void *)&cdtext_length, sizeof(long), n_bytes_tot, n_bytes_read) ;
            if (cdtext_length > 0) {
                if (cdtext_data != NULL) {
                    free(cdtext_data) ;
                }
                cdtext_data = calloc(cdtext_length, 1); 
	        LL_READ(fd, (void *)cdtext_data, cdtext_length, n_bytes_tot, n_bytes_read) ;
            } else {
                cdtext_data = NULL;
            }
	} else {
	    n_markers = 0 ;
	}

	close(fd) ;

	if(n_bytes_tot != n_bytes_read)
	    warning("Reading sample blocks had a problem") ;
	return 1 ;
    } else {
	return 0 ;
    }
}

/* void sum_sample_block(struct sample_block *sb, double left[], double right[], long n) */
void sum_sample_block(struct sample_block *sb, fftw_real left[], fftw_real right[], long n)
{
    long i ;
    double sum_x2[2] ;

    sb->n_samples = n ;
    sum_x2[0] = 0.0 ;
    sum_x2[1] = 0.0 ;
    sb->max_value[0] = 0.0 ;
    sb->max_value[1] = 0.0 ;


    for(i = 0 ; i < n ; i++) {
	if(fabs(left[i]) > sb->max_value[0]) sb->max_value[0] = fabs(left[i]) ;
	if(fabs(right[i]) > sb->max_value[1]) sb->max_value[1] = fabs(right[i]) ;
	sum_x2[0] += (double)left[i]*(double)left[i] ;
	sum_x2[1] += (double)right[i]*(double)right[i] ;
    }

    sb->rms[0] = sqrt(sum_x2[0]/(n+1.e-30)) ;
    sb->rms[1] = sqrt(sum_x2[1]/(n+1.e-30)) ;

}

void stat_sample_block(struct sample_block *sb, struct sound_prefs *p, long block_number)
{
    long first = block_number*SBW ;
    long last = first + (SBW-1) ;
    long n ;
/*    double left[SBW], right[SBW] ; */
    fftw_real left[SBW], right[SBW] ;

    if(last > p->n_samples - 1) last = p->n_samples - 1 ;
    n = read_fft_real_wavefile_data(left, right, first, last) ;
    sum_sample_block(sb, left, right, n) ;

}

void resample_audio_data(struct sound_prefs *p, long first, long last)
{
    long first_block = first/SBW ;
    long last_block = last/SBW ;
    long current_block ;
#ifndef TRUNCATE_OLD
    resize_sample_buffer(p);
#endif
    for(current_block = first_block ; current_block <= last_block ; current_block++) {
	struct sample_block *sb = &sample_buffer[current_block] ;
	stat_sample_block(sb, p, current_block) ;
    }
}
    

void rescan_sample_buffer(struct sound_prefs *p)
{
    long current_block ;
#ifndef TRUNCATE_OLD
    resize_sample_buffer(p);
#endif
    n_blocks = p->n_samples / SBW ;
    n_blocks += (p->n_samples - n_blocks*SBW > 0 ? 1 : 0) ;

    push_status_text("Scanning audio for display information") ;
    update_progress_bar(0.0,PROGRESS_UPDATE_INTERVAL,TRUE) ;

    for(current_block = 0 ; current_block < n_blocks ; current_block++) {
	struct sample_block *sb = &sample_buffer[current_block] ;
	update_progress_bar((gfloat)current_block/(gfloat)n_blocks,PROGRESS_UPDATE_INTERVAL,FALSE) ;
	stat_sample_block(sb, p, current_block) ;
    }
    save_sample_block_data(p) ;
    update_progress_bar(0.0,PROGRESS_UPDATE_INTERVAL,TRUE) ;
    pop_status_text() ;
}

#ifndef TRUNCATE_OLD
void resize_sample_buffer(struct sound_prefs *p)
{
    n_blocks = p->n_samples / SBW ;
    n_blocks += (p->n_samples - n_blocks*SBW > 0 ? 1 : 0) ;

    if(sample_buffer == NULL) {
        sample_buffer = (struct sample_block *) calloc(n_blocks, sizeof(struct sample_block)) ;
        sb_size = n_blocks*sizeof(struct sample_block) ;
    } else {
        size_t new_size = n_blocks*sizeof(struct sample_block);
        if (new_size > sb_size) {
            struct sample_block *new_buffer = realloc(sample_buffer, new_size);
            if (new_buffer != NULL) {
                /* like calloc: set new buffer to '0' */
                memset((char*)new_buffer+sb_size, 0, new_size-sb_size);
                sample_buffer = new_buffer;
                sb_size = new_size;
            }
        }
    }
}
#endif

void fill_sample_buffer(struct sound_prefs *p)
{

    n_blocks = p->n_samples / SBW ;
    n_blocks += (p->n_samples - n_blocks*SBW > 0 ? 1 : 0) ;

    if(sample_buffer != NULL) free(sample_buffer) ;

    sample_buffer = (struct sample_block *) calloc(n_blocks, sizeof(struct sample_block)) ;
    sb_size = n_blocks*sizeof(struct sample_block) ;

    if(!load_sample_block_data(p)) {
	g_print("Building display information, n_samples=%ld, hang on...\n", p->n_samples) ;
	push_status_text("Loading audio information") ;

	if(load_sample_block_data(p) == 0) {
	    rescan_sample_buffer(p) ;
	}

	pop_status_text() ;
    }

    p->sample_buffer_exists = TRUE ;
}

int get_sample_buffer(struct sample_block **result) {
   *result = sample_buffer;
   return  n_blocks ;
}

void get_sample_stats(struct sample_display_block *result, long first, long last, double blocks_per_pixel)
{
    long first_block = first/SBW ;
    long last_block = last/SBW ;
    long i ;
    double sum_wgts = 0.0 ;

    result->n_samples = 0 ;
    result->rms[0] = 0.0 ;
    result->rms[1] = 0.0 ;
    result->max_value[0] = 0.0 ;
    result->max_value[1] = 0.0 ;

    if(blocks_per_pixel > 1) {
	for(i = first_block ; i <= last_block; i++) {
	    struct sample_block *sb = &sample_buffer[i] ;
            long n_in_block;
            if (i != first_block && i != last_block) {
	       long first_sample = MAX(first, i*SBW) ;
	       long last_sample = MIN(last, (i+1)*SBW-1) ;
	       double p;

	       n_in_block = last_sample - first_sample + 1 ;
               p = (double)(n_in_block) / (double)sb->n_samples ;
	       result->rms[0] += p*sb->rms[0] ;
	       result->rms[1] += p*sb->rms[1] ;
	       sum_wgts += p ;
            } else {
	       n_in_block = SBW ;
	       result->rms[0] += sb->rms[0] ;
	       result->rms[1] += sb->rms[1] ;
	       sum_wgts += 1.0 ;
            }
	    if(sb->max_value[0] > result->max_value[0]) result->max_value[0] = sb->max_value[0] ;
	    if(sb->max_value[1] > result->max_value[1]) result->max_value[1] = sb->max_value[1] ;
	    result->n_samples += n_in_block ;
	}
	result->rms[0] /= sum_wgts+1.e-30 ;
	result->rms[1] /= sum_wgts+1.e-30 ;
    } else {
/*	double left[SBW], right[SBW] ; */
	fftw_real left[SBW], right[SBW] ;
	int n = read_fft_real_wavefile_data(left, right, first, last) ;
	result->n_samples = n ;
	for(i = 0 ; i < n ; i++) {
	    if(fabs(left[i]) > result->max_value[0]) result->max_value[0] = fabs(left[i]) ;
	    if(fabs(right[i]) > result->max_value[1]) result->max_value[1] = fabs(right[i]) ;
	    result->rms[0] += left[i]*left[i] ;
	    result->rms[1] += right[i]*right[i] ;
	    sum_wgts += 1.0 ;
	}
	result->rms[0] = sqrt(result->rms[0]/(sum_wgts+1.e-30)) ;
	result->rms[1] = sqrt(result->rms[1]/(sum_wgts+1.e-30)) ;
    }
}
