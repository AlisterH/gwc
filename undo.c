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

/* undo.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include "gwc.h"
#ifndef TRUNCATE_OLD
#include "soundfile.h"

#define UNDO_OVERWRITE 0
#define UNDO_INSERT    1
#define UNDO_REMOVE    2
#endif /* TRUNCATE_OLD */

static char current_undo_msg[200] ;
static int undo_fd = -1 ;
static int undo_level = 0 ;

int get_undo_levels(void)
{
    return undo_level ;
}

int start_save_undo(char *undo_msg, struct view *v)
{
    char filename[1024] ;
    short l ;

    undo_level++ ;

    sprintf(filename, "gwc_undo_%d.dat", undo_level) ;

    if( (undo_fd = open(filename, O_CREAT|O_TRUNC|O_RDWR, S_IRUSR|S_IWUSR)) == -1) {
	warning("Can't save undo information") ;
	return -1 ;
    }

    if(undo_level == 1)
	strcpy(current_undo_msg, "Nothing to undo") ;

    l = strlen(current_undo_msg) ;

    write(undo_fd, (char *)&l, sizeof(l)) ;
    write(undo_fd, current_undo_msg, l) ;
    {
	long first, last ;
	get_region_of_interest(&first, &last, v) ;
	write(undo_fd, (char *)&first, sizeof(first)) ;
	write(undo_fd, (char *)&last, sizeof(last)) ;
	write(undo_fd, (char *)&v->channel_selection_mask, sizeof(v->channel_selection_mask)) ;
    }
    strcpy(current_undo_msg, undo_msg) ;

    return undo_level ;
}

extern int FRAMESIZE ;

#ifndef TRUNCATE_OLD
static int save_undo_data_impl(long first_sample, long last_sample,
                               int undo_type, int status_update_flag)
#else
int save_undo_data(long first_sample, long last_sample, struct sound_prefs *p, int status_update_flag)
#endif
{
    const int BLOCK_SIZE = 1024 ;
    char buf[BLOCK_SIZE * FRAMESIZE] ;
    long curr ;
    long blocks ;
    gfloat n_sample = (last_sample-first_sample+1) ;

#ifndef TRUNCATE_OLD
    if (undo_type != UNDO_INSERT) {
#endif
    if(n_sample*FRAMESIZE > 10000000) {
	GtkWidget *dialog ;
	char buf[200] ;
	int ret ;

	sprintf(buf, "Undo will need %7.2f Mbytes of disk space (skipping undo commits changes to your original audio file)", n_sample*FRAMESIZE/1000000.0) ;

	dialog = gtk_dialog_new_with_buttons(buf, NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
	   "Skip undo", 0, "Cancel edit action", 1, "Save undo data", 2, NULL) ;

	ret = gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog) ;

	if( ret!= 2) { /* Dont save undo */
	    undo_level-- ;
	    close(undo_fd) ;
	    undo_fd = -1 ;
	    if(ret == 1 || ret == -1)  /* we cancelled */
		return 1 ;
	    else
		return 0 ;
	}

    }
#ifndef TRUNCATE_OLD
    }
#endif

    write(undo_fd, (char *)&first_sample, sizeof(first_sample)) ;
    write(undo_fd, (char *)&last_sample, sizeof(last_sample)) ;
#ifndef TRUNCATE_OLD
    write(undo_fd, (char *)&undo_type, sizeof(undo_type)) ;

    if (undo_type != UNDO_INSERT) {
#endif        
    if(status_update_flag)
	update_status_bar(0.0, STATUS_UPDATE_INTERVAL, TRUE) ;

    blocks = (last_sample - first_sample + 1) / BLOCK_SIZE;

    for(curr = first_sample ; curr <= last_sample ; curr += BLOCK_SIZE) {
        long end;
	gfloat p = (gfloat)(curr-first_sample)/n_sample ;

	if(status_update_flag)
	    update_status_bar(p,STATUS_UPDATE_INTERVAL,FALSE) ;

        end = curr + BLOCK_SIZE - 1;
        if (end > last_sample)
           end = last_sample;
	read_raw_wavefile_data(buf, curr, end) ;
	if (write(undo_fd, buf, FRAMESIZE * (end - curr + 1)) != FRAMESIZE * (end - curr + 1) ) {
            warning("Error saving undo data (out of disk space?), program will exit");
            exit(1);
        }
    }
#ifndef TRUNCATE_OLD
    }
#endif
    if(status_update_flag)
	update_status_bar(0.0, STATUS_UPDATE_INTERVAL, TRUE) ;

    return 0 ;
}

#ifndef TRUNCATE_OLD
int save_undo_data(long first_sample, long last_sample, struct sound_prefs *p, int status_update_flag)
{
    return save_undo_data_impl(first_sample, last_sample, UNDO_OVERWRITE, status_update_flag);
}

int save_undo_data_remove(long first_sample, long last_sample, int status_update_flag)
{
    return save_undo_data_impl(first_sample, last_sample, UNDO_REMOVE, status_update_flag);
}

int save_undo_data_insert(long first_sample, long last_sample, int status_update_flag)
{
    return save_undo_data_impl(first_sample, last_sample, UNDO_INSERT, status_update_flag);
}
#endif /* !TRUNCATE_OLD */

int close_undo(void)
{
    close(undo_fd) ;
    undo_fd = -1 ;
    return undo_level ;
}

char *get_undo_msg(void)
{
    return current_undo_msg ;
}

int undo(struct view *v, struct sound_prefs *p)
{
    short l ;
    long first_sample, last_sample, curr ;
    int n_sections ;
#define N_ALLOC_INC 1000
    int n_sections_max = N_ALLOC_INC ;
    char filename[1024] ;
    const int BLOCK_SIZE = 1024 ;
    char buf[BLOCK_SIZE * FRAMESIZE] ;
    long blocks ;
    off_t *data_start_pos ;
    long total_sections;
#ifndef TRUNCATE_OLD
    int undo_type;
#endif
    if(undo_level == 0 || undo_fd != -1) {
	warning("Nothing to undo!") ;
	return undo_level ;
    }

    sprintf(filename, "gwc_undo_%d.dat", undo_level) ;

    if( (undo_fd = open(filename, O_RDONLY)) == -1) {
	warning("Can't undo, undo save data has been deleted from hard drive!") ;
	return undo_level ;
    }

    push_status_text("Performing Undo") ;
    update_status_bar(0.0, STATUS_UPDATE_INTERVAL, TRUE) ;

    read(undo_fd, (char *)&l, sizeof(l)) ;

    read(undo_fd, current_undo_msg, l) ;
    current_undo_msg[l] = '\0' ;

    read(undo_fd, (char *)&v->selected_first_sample, sizeof(v->selected_first_sample)) ;
    read(undo_fd, (char *)&v->selected_last_sample, sizeof(v->selected_last_sample)) ;
    read(undo_fd, (char *)&v->channel_selection_mask, sizeof(v->channel_selection_mask)) ;
    v->selection_region = TRUE ;

    n_sections = 0 ;
    data_start_pos = (off_t *)calloc(n_sections_max, sizeof(off_t)) ;
    data_start_pos[n_sections] = lseek(undo_fd, (off_t)0, SEEK_CUR) ;

    while(read(undo_fd, (char *)&first_sample, sizeof(first_sample)) == sizeof(first_sample) ) {
	read(undo_fd, (char *)&last_sample, sizeof(last_sample)) ;
#ifndef TRUNCATE_OLD
	read(undo_fd, (char *)&undo_type, sizeof(undo_type)) ;
#endif
#if 0
	for(curr = first_sample ; curr <= last_sample ; curr++) {
	    read(undo_fd, (char *)buf, sizeof(short)*2) ;
	}
#endif
#ifndef TRUNCATE_OLD
        if (undo_type != UNDO_INSERT)
#endif
        lseek(undo_fd, (off_t)((last_sample - first_sample + 1) * FRAMESIZE), SEEK_CUR);
	n_sections++ ;
	if(n_sections == n_sections_max) {
	    n_sections_max += N_ALLOC_INC ;
	    data_start_pos = (off_t *)realloc(data_start_pos, n_sections_max*sizeof(off_t)) ;
	}
	data_start_pos[n_sections] = lseek(undo_fd, (off_t)0, SEEK_CUR) ;
    }


    total_sections = n_sections;
    /* the sections of an undo operation have to be applied in the inverse order they
       were stored to properly do an undo */
    for(n_sections-- ; n_sections >= 0 ; n_sections-- ) {
	lseek(undo_fd, data_start_pos[n_sections], SEEK_SET) ;

	read(undo_fd, (char *)&first_sample, sizeof(first_sample)) ;
	read(undo_fd, (char *)&last_sample, sizeof(last_sample)) ;
#ifndef TRUNCATE_OLD
	read(undo_fd, (char *)&undo_type, sizeof(undo_type)) ;

        if (undo_type == UNDO_INSERT) {
            soundfile_remove_samples(first_sample,
                                     last_sample - first_sample + 1, 1);
        } else if (undo_type == UNDO_REMOVE) {
            soundfile_insert_silence(first_sample,
                                     last_sample - first_sample + 1, 1);
        }

        if (undo_type != UNDO_INSERT) {
#endif
        blocks = (last_sample - first_sample + 1) / BLOCK_SIZE;
        for(curr = first_sample ; curr <= last_sample ; curr += BLOCK_SIZE) {
              long end;
	    gfloat p = (gfloat)(curr-first_sample)/(last_sample - first_sample) * (total_sections - n_sections) / total_sections ;

	    update_status_bar(p,STATUS_UPDATE_INTERVAL,FALSE) ;

            end = curr + BLOCK_SIZE - 1;
            if (end > last_sample)
               end = last_sample;
	    read(undo_fd, (char *)buf, FRAMESIZE * (end - curr + 1)) ;
	    write_raw_wavefile_data(buf, curr, end) ;
	}
#ifdef TRUNCATE_OLD
	resample_audio_data(p, first_sample, last_sample)  ;
#else
        }
        if (undo_type == UNDO_INSERT || undo_type == UNDO_REMOVE) {
            p->n_samples = soundfile_count_samples();
            v->n_samples = p->n_samples;
            if (v->last_sample > v->n_samples - 1) {
                v->first_sample = v->n_samples - 1 - (v->last_sample - v->first_sample);
                v->last_sample  = v->n_samples - 1;
            }
            if (v->first_sample < 0)
                v->first_sample = 0;
            resample_audio_data(p, first_sample, p->n_samples - 1);
        } else {
            resample_audio_data(p, first_sample, last_sample);
        }
#endif
    }

    save_sample_block_data(p) ;

    flush_wavefile_data() ;

    close(undo_fd) ;
    unlink(filename) ;
    free(data_start_pos) ;

    undo_fd = -1 ;
    undo_level-- ;

    update_status_bar(0.0, STATUS_UPDATE_INTERVAL, TRUE) ;
    pop_status_text();

    return undo_level ;
}

void undo_purge(void)
{
    char filename[1024] ;

    while(undo_level>0) {
	sprintf(filename, "gwc_undo_%d.dat", undo_level) ;
	unlink(filename) ;
	undo_level-- ;
    }
}
