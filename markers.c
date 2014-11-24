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

/* markers.c */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#include "gtkledbar.h"
#include "gwc.h"

long cdtext_length;
char *cdtext_data = NULL;

/* The file selection widget and the string to store the chosen filename */

GtkWidget *file_selector;
gchar *selected_filename;
gchar save_cdrdao_toc_filename[PATH_MAX+1];
extern long num_song_markers, song_markers[] ;
extern gchar wave_filename[] ;
extern struct sound_prefs prefs ;
extern struct view audio_view;
extern double song_key_highlight_interval ;
extern double song_mark_silence ;


char *find_text(long length,char *data, char *str) {
   char *ret = NULL;
   char *end = data + length;
   int len;

  while (data < end && ret == NULL) {
     len = strlen(data);
     if (strcmp(data, str) == 0) {
        ret = data + len + 1; 
     } else {
        data = data + len + 1;
           /* Skip field value */
        len = strlen(data);
        data = data + len + 1;
     }
  }
  return ret;
}

int only_blank(char *str)
{
   while (*str != 0) {
      if (*str++ != ' ')
         return 0;
   }
   return 1;
}

static int use_song_marker_pairs ; /* song markers are positioned at start AND end of each song? */

#define ARRAYSIZE(x) (sizeof(x) / sizeof(x[0]))
/* CD Tracks must be multiple of 588 or they will be padded with zeros */
#define SONG_BLOCK_LEN 588
void cdrdao_toc_info(char *filename)
{
    GtkWidget *dlg ;
    GtkWidget *dialog_table ;
    GtkWidget *song_table ;
    int dres ;
    int row = 0;
    struct {
       char *title;
       char *fieldid;
       char *init;
       int always_write;
       GtkWidget *widget;
    } album_info[] = 
    {
       {"Language", "LANGUAGE", "EN", 1, NULL},  /* This is special and must be first */
       {"Title", "TITLE",  "", 1, NULL},
       {"Performer", "PERFORMER", "", 0, NULL},
       {"Songwriter", "SONGWRITER", "", 0, NULL},
       {"Arranger", "ARRANGER", "", 0, NULL},
       {"Composer", "COMPOSER", "", 0, NULL},
       {"Disc_ID", "DISC_ID", "", 0, NULL},
       {"Message", "MESSAGE", "", 1, NULL}
    };
    struct {
       char *title;
       char *fieldid;
       char *init;
       int always_write;
    } song_info[] = 
    {
       {"Title", "TITLE",  "", 1},
       {"Message", "MESSAGE",  "", 1}
    };
    GtkWidget *song_widget[MAX_MARKERS+1][ARRAYSIZE(song_info)];
    int i,j;
    GtkWidget *scrolled;
    int new_cdtext_length = 0;
    char *new_cdtext;
    int new_cdtext_loc;
    char buf[200], buf2[200];

    dlg = gtk_dialog_new_with_buttons("Cdrdao CD Text Information",
			GTK_WINDOW(main_window), GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			 GTK_STOCK_OK, GTK_RESPONSE_OK, NULL, NULL);
    gtk_dialog_set_default_response (GTK_DIALOG(dlg), GTK_RESPONSE_OK);

    //Alister: I don't believe this does anything
    //gtk_window_set_policy(GTK_WINDOW(dlg), FALSE, TRUE, FALSE);

    dialog_table = gtk_table_new(5,2,0) ;

    gtk_table_set_row_spacings(GTK_TABLE(dialog_table), 4) ;
    gtk_table_set_col_spacings(GTK_TABLE(dialog_table), 6) ;

    gtk_widget_show (dialog_table);

    for (i = 0; i < ARRAYSIZE(album_info); i++) {
       char *init;
       init = find_text(cdtext_length, cdtext_data,  album_info[i].fieldid);
       if (init == NULL) {
          init = album_info[i].init;
       }
       album_info[i].widget = add_number_entry_with_label(init, album_info[i].title, dialog_table, row++) ;
    }

    song_table = gtk_table_new(5,2,0) ;
    gtk_table_set_row_spacings(GTK_TABLE(song_table), 4) ;
    gtk_table_set_col_spacings(GTK_TABLE(song_table), 6) ;

    scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
                                    GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled), song_table);
    gtk_widget_set_usize(scrolled,300,300);
    gtk_widget_show (scrolled);
    gtk_widget_show (song_table);

    row = 0;
    for (j = 0; j < num_song_markers+1; j += 1+use_song_marker_pairs) {
       for (i = 0; i < ARRAYSIZE(song_info); i++) {
          char *init;

          snprintf(buf, sizeof(buf), "Song %2d %s", j+1, song_info[i].title);
             /* Duplicated below, and new_cdtext_length adjusted by 3 for %3d */
          snprintf(buf2, sizeof(buf2), "%3d%s", j+1, song_info[i].fieldid);
          init = find_text(cdtext_length, cdtext_data,  buf2);
          if (init == NULL) {
             init = song_info[i].init;
          }
	  song_widget[j][i] = add_number_entry_with_label(init, buf, song_table, row++) ;
       }
    }
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG(dlg)->vbox), dialog_table, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG(dlg)->vbox), scrolled, TRUE, TRUE, 0);

    dres = gwc_dialog_run(GTK_DIALOG(dlg)) ;

    if(dres == 0) {
	FILE *toc;
	toc = fopen(filename,"w");
	if (toc == NULL) {
	   snprintf(buf, sizeof(buf), "Unable to open %s: %s", filename, strerror(errno));
	   warning(buf);
	} else {
	   int found_text = 0;
	   for (i = 0; i < ARRAYSIZE(album_info); i++) {
	       char *str;
	       str = (char *)gtk_entry_get_text(GTK_ENTRY(album_info[i].widget));
		  /* +2 for null at end of both strings*/
	       new_cdtext_length += strlen(str) + strlen(album_info[i].fieldid) + 2 ;
	       if (strlen(album_info[i].init) == 0 && !only_blank(str))
		  found_text = 1;;
	   }
	   for (j = 0; j < num_song_markers+(1-use_song_marker_pairs); j += 1+use_song_marker_pairs) {
	      for (i = 0; i < ARRAYSIZE(song_info); i++) {
		  char *str;
		  str = (char *)gtk_entry_get_text(GTK_ENTRY(song_widget[j][i]));
		     /* We add 3 digit song # to fieldid when storing */
		  new_cdtext_length += strlen(str) + strlen(song_info[i].fieldid) + 3 + 2;
		  if (!only_blank(str))
		      found_text = 1;;
	      }
	   }
	   new_cdtext_loc = 0;
	   if (found_text) {
	      new_cdtext = calloc(new_cdtext_length, 1);
	      fprintf(toc, "CD_TEXT {\n   LANGUAGE_MAP {\n      0: %s\n   }\n", gtk_entry_get_text(GTK_ENTRY(album_info[0].widget)));
	      fprintf(toc, "   LANGUAGE 0 {\n");
	      for (i = 0; i < ARRAYSIZE(album_info); i++) {
		 char *str;
		 str = (char *)gtk_entry_get_text(GTK_ENTRY(album_info[i].widget));
		 if (album_info[i].always_write || !only_blank(str)) {
		       /* First entry, language added to file above */
		    if (i > 0) {
		       fprintf(toc, "      %s \"%s\"\n", album_info[i].fieldid, str);
		    }
		    strcat(&new_cdtext[new_cdtext_loc], album_info[i].fieldid);
		    new_cdtext_loc += strlen(&new_cdtext[new_cdtext_loc]) + 1;
		    strcat(&new_cdtext[new_cdtext_loc], str);
		    new_cdtext_loc += strlen(&new_cdtext[new_cdtext_loc]) + 1;
		 }
	      }
	      fprintf(toc, "   }\n}\n");
	   } else {
	      new_cdtext = NULL;
	   }

	   for (j = 0; j < num_song_markers+(1-use_song_marker_pairs); j += 1+use_song_marker_pairs) {
	      long end;
	      int j1 = j+1 ;
	      long length ;
	      long start ;

	      if(use_song_marker_pairs) {
		  start = song_markers[j] ;

		  if(j1 < num_song_markers) {
		      length = song_markers[j+1] - start + 1 ;
		  } else {
		      /* no last song marker in last pair, assume end of audio for end of last track */
		      length = (prefs.n_samples-1) - start + 1 ;
		  }
	      } else {
		  if (j == 0) {
		    start = 0 ;
		    length = song_markers[j] - start + 1 ;
		  } else if (j == num_song_markers) {
		    start = song_markers[j-1] ;
		    length = (prefs.n_samples-1) - start + 1 ;
		  } else {
		    start = song_markers[j-1] ;
		    length = song_markers[j] - start + 1 ;
		  }
	      }

#define AUDIO_BLOCK_LEN 588
	      length = ((length + AUDIO_BLOCK_LEN / 2) / AUDIO_BLOCK_LEN) * AUDIO_BLOCK_LEN ;

	      end = start + length -1 ;

	      while(end > (prefs.n_samples-1) && end > 2*AUDIO_BLOCK_LEN) {
		end -= AUDIO_BLOCK_LEN ;
	      }

	      length = end - start + 1 ;

	      fprintf(toc, "TRACK AUDIO\n");
	      if (found_text) {
		 fprintf(toc, "   CD_TEXT {\n      LANGUAGE 0 {\n");
		 for (i = 0; i < ARRAYSIZE(song_info); i++) {
		    char *str;
		    str = (char *)gtk_entry_get_text(GTK_ENTRY(song_widget[j][i]));
		    if (song_info[i].always_write || !only_blank(str)) {
		       fprintf(toc, "         %s \"%s\"\n", song_info[i].fieldid, str);
			  /* Duplicated above */
		       snprintf(buf, sizeof(buf), "%3d%s", j+1, song_info[i].fieldid);
		       strcat(&new_cdtext[new_cdtext_loc], buf);
		       new_cdtext_loc += strlen(&new_cdtext[new_cdtext_loc]) + 1;
		       strcat(&new_cdtext[new_cdtext_loc], str);
		       new_cdtext_loc += strlen(&new_cdtext[new_cdtext_loc]) + 1;
		    }
		 }
		 fprintf(toc, "      }\n   }\n");
	      }
	      fprintf(toc, "   FILE \"%s\" %ld %ld\n", wave_filename, start, end - start + 1) ;

	   }
	   fclose(toc);
	   if (cdtext_data != NULL) {
	      free(cdtext_data);
	   }
	   cdtext_data = new_cdtext;
	      /* Loc is the actual length we filled */
	   cdtext_length = new_cdtext_loc;
	}
    }

    gtk_widget_destroy(dlg) ;
}

void store_cdrdao_toc(gpointer user_data)
{
    if(strcmp(save_cdrdao_toc_filename, wave_filename)) {
	int l ;

	l = strlen(save_cdrdao_toc_filename) ;

	d_print("Save cdrdao_toc to %s\n", save_cdrdao_toc_filename) ;

	cdrdao_toc_info(save_cdrdao_toc_filename) ;

    } else {
	warning("Cannot save selection over the currently open file!") ;
    }

}

void save_cdrdao_toc(GtkWidget * widget, gpointer data)
{
   char tmppath[PATH_MAX+6];
   GtkFileFilter * ff, * ffa;

   if (num_song_markers == 0) {
      info("No songs marked,  Use Markers->Mark Songs");
   } else {
	strcpy(tmppath, wave_filename);

	/* Create the selector */
	file_selector =
	    gtk_file_chooser_dialog_new("Filename to save cdrdao toc to:",
                                        GTK_WINDOW(main_window),
                                        GTK_FILE_CHOOSER_ACTION_SAVE,
                                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                        GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                        NULL);

	bcopy(".toc", strrchr(tmppath, '.'), 4);
	gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER
					  (file_selector), basename(tmppath));
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER
					    (file_selector), dirname(tmppath));
	gtk_file_chooser_set_do_overwrite_confirmation
            		                    (GTK_FILE_CHOOSER(file_selector), TRUE);

	ff = gtk_file_filter_new();
	gtk_file_filter_set_name(ff,"TOC files");
	gtk_file_filter_add_pattern(ff,"*.toc");
	gtk_file_filter_add_pattern(ff,"*.TOC");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(file_selector),ff);
  
	ffa = gtk_file_filter_new();
	gtk_file_filter_set_name(ffa,"All files");
	gtk_file_filter_add_pattern(ffa,"*");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(file_selector),ffa);


	/* Display the dialog */
        if (gtk_dialog_run (GTK_DIALOG (file_selector)) == GTK_RESPONSE_ACCEPT)
        {
                strncpy(save_cdrdao_toc_filename,
                        gtk_file_chooser_get_filename(GTK_FILE_CHOOSER
 					              (file_selector)), PATH_MAX);
        	gtk_widget_destroy (GTK_WIDGET (file_selector));
                store_cdrdao_toc(wave_filename);
        } else {
        gtk_widget_destroy (GTK_WIDGET (file_selector));
	}
   }
}

void save_cdrdao_tocs(GtkWidget * widget, gpointer data)
{
    use_song_marker_pairs = 0 ;
    save_cdrdao_toc(widget, data) ;
}

void save_cdrdao_tocp(GtkWidget * widget, gpointer data)
{
    use_song_marker_pairs = 1 ;
    save_cdrdao_toc(widget, data) ;
}

int _add_song_marker(long loc)
{
   int i,j;

   if (num_song_markers >= MAX_MARKERS - 1) {
      set_status_text("No more song markers available");
      return 0 ;
   } else {
      for (i = 0; i < num_song_markers; i++) {
         if (song_markers[i] > loc) {
            break;
         }
      }
      for (j = num_song_markers - 1; j >= i;  j--) {
         song_markers[j+1] = song_markers[j];
      }
      song_markers[i] = loc ;
      num_song_markers++; 
      return 1 ;
   }
}

void add_song_marker(void)
{
   long loc = audio_view.selected_first_sample;

   if(_add_song_marker(loc))
      main_redraw(FALSE, TRUE);
}

void add_song_marker_pair(void)
{
   int r ;
   long loc = audio_view.selected_first_sample;
   r = _add_song_marker(loc) ;

   loc = audio_view.selected_last_sample;
   r += _add_song_marker(loc) ;

   if(r > 0)
      main_redraw(FALSE, TRUE);
}

void delete_song_marker(void)
{
   int i,j;

   i = 0;
   while (i < num_song_markers) {
      if (song_markers[i] >= audio_view.selected_first_sample &&
           song_markers[i] <= audio_view.selected_last_sample) {
         for (j = i; j < num_song_markers - 1;  j++) {
            song_markers[j] = song_markers[j+1];
         }
         num_song_markers--; 
      } else {
         i++;
      }
   }
   main_redraw(FALSE, TRUE);
}

void adjust_song_marker_positions(long pos, long delta)
{
    int i,j;

    i = 0;
    while (i < num_song_markers) {
        if (song_markers[i] >= pos) {
            song_markers[i] += delta;
            if (song_markers[i] <= pos || song_markers[i] >= prefs.n_samples) {
                for (j = i; j < num_song_markers - 1; j++) {
                    song_markers[j] = song_markers[j+1];
                }
                num_song_markers--;
            } else {
                i++;
            }
        } else {
            i++;
        }
    }
}

void move_song_marker(void)
{
   int i;
   long loc = audio_view.selected_first_sample;
   long err;
   int min_err_loc = 0;
   long min_err = LONG_MAX;

   if (num_song_markers == 0) {
      set_status_text("No song markers");
   } else {
      for (i = 0; i < num_song_markers; i++) {
          err = abs(loc - song_markers[i]);
          if (err < min_err) {
             min_err = err;
             min_err_loc = i;
          }
      }
      song_markers[min_err_loc] = (loc / SONG_BLOCK_LEN) * SONG_BLOCK_LEN;
      main_redraw(FALSE, TRUE);
   }
}

void select_song_marker(void)
{
   int i;
   long loc = audio_view.selected_last_sample;

   if (num_song_markers == 0) {
      set_status_text("No song markers");
   } else {
      if (loc > song_markers[num_song_markers-1]) {
         loc = 0;
      }
      for (i = 0; i < num_song_markers && song_markers[i] < loc; i++);
      audio_view.selected_last_sample = MIN(prefs.n_samples - 1, song_markers[i] + prefs.rate * song_key_highlight_interval / 2) ;
      audio_view.selected_first_sample = MAX(0, song_markers[i] - prefs.rate * song_key_highlight_interval / 2) ;
      audio_view.selection_region = TRUE ;
      main_redraw(FALSE, TRUE);
   }
}


#define DETECT_GAPS_NOT
#ifdef DETECT_GAPS
double sample_rms(struct sample_block *sample_buffer, int i, int w)
{
    int istart=i-w/2 ;
    if(istart < 0) istart=0 ;
    int iend=i+w/2 ;
    double sum=0 ;
    double n=iend-istart+1.0 ;

    for(i=istart ; i <= iend ; i++) {
	sum += (sample_buffer[i].rms[0]+sample_buffer[i].rms[1])/2.0 ;
    }

    return sum/n ;
}
#endif

void mark_songs(GtkWidget * widget, gpointer data)
{
    struct sample_block *sample_buffer ;
    int n_blocks ;
    int i;
    double max_song = 0.0, min_song = 999999999.0;
    double song_amp;
    double song_window_amp = 0.0;
    double *delay;
    /* These might be good as preferences */
    double MIN_SONG_LEN = 35.0;
    long min_song_blocks;
    /* Length of sliding window in seconds to average audio over */
    double AVG_LEN = song_mark_silence*.75;
    int avg_blocks;
    long min_silence_blocks;
    double SILENCE_EST = .3;
    double silence;
    double sec_per_block;
    double silence_scale;
    int found_short;
    int valid_delay;
    int delay_cntr;
    int last_silence;
    int silence_cntr;
    char buf[200];
    int last_song_block;

    num_song_markers = 0;
    n_blocks = get_sample_buffer(&sample_buffer) ;
   
    if (n_blocks > 0) {
       sec_per_block = (double) sample_buffer[0].n_samples / prefs.rate;
    } else {
       sec_per_block = 0.0;
    }

    if (n_blocks * sec_per_block < MIN_SONG_LEN * 3)  {
        snprintf(buf, sizeof(buf), "Must have at least %4.0f seconds of music", MIN_SONG_LEN*3);
        info(buf);
        return;
    }

    min_silence_blocks = MAX(.25,song_mark_silence) / sec_per_block;
    min_song_blocks = MIN_SONG_LEN / sec_per_block;
    avg_blocks = MAX(.25*.75,AVG_LEN) / sec_per_block;

    delay = malloc(avg_blocks * sizeof(delay[0]));

    /* First find minimum and maximum level in a sliding window */
    valid_delay = 0;
    delay_cntr = 0;
    song_window_amp = 0.0;
    for (i = 0; i < avg_blocks; i++)
        delay[i] = 0.0;
    for (i = min_song_blocks; i < n_blocks - min_song_blocks; i++) {
       song_amp = (sample_buffer[i].max_value[0] + sample_buffer[i].max_value[1]);
       song_window_amp += song_amp - delay[delay_cntr];
       delay[delay_cntr] = song_amp;
       delay_cntr = (delay_cntr + 1) % avg_blocks;
       if (delay_cntr == 0)
          valid_delay = 1;
       if (valid_delay) {
          if (song_window_amp > max_song)
             max_song = song_window_amp; 
          if (song_window_amp < min_song)
             min_song = song_window_amp; 
       }
    }

    /* Now step the threshold up until we find something too short to be a */
    /* song. */
    found_short = 0;
    for (silence_scale = 2.0; silence_scale < 32.0 && !found_short; ) {
	last_silence = -min_song_blocks;
	valid_delay = 0;
	delay_cntr = 0;
	song_window_amp = 0.0;
	silence_cntr = 0;
	for (i = 0; i < avg_blocks; i++)
	    delay[i] = 0.0;
	for (i = min_song_blocks; i < n_blocks - min_song_blocks && !found_short; i++) {
	   song_amp = (sample_buffer[i].max_value[0] + sample_buffer[i].max_value[1]);
	   song_window_amp += song_amp - delay[delay_cntr];
	   delay[delay_cntr] = song_amp;
	   delay_cntr = (delay_cntr + 1) % avg_blocks;
	   if (delay_cntr == 0)
	      valid_delay = 1;
	   if (valid_delay) {
	      if (song_window_amp > min_song * silence_scale) {
		 silence_cntr = 0;
	      } else {
		 silence_cntr++;
		 if (silence_cntr > min_silence_blocks) {
		    if (i - last_silence > min_silence_blocks * 3 && i - last_silence < min_song_blocks) {
		       found_short = 1;
		    }
		    last_silence = i;
		 }
	      }
	   }
	}
        if (!found_short)
	   silence_scale *= 1.5;
     }


    /* Pick a threshold between the minimum level and the two high level from */
    /* above.  Use it to mark the songs.  Might be good to look for minimum */
    /* silence level to help center the song break better */
    silence = min_song + min_song * silence_scale * SILENCE_EST;
    valid_delay = 0;
    delay_cntr = 0;
    song_window_amp = 0.0;
    silence_cntr = 0;
    last_silence = 0;
    last_song_block = 0;

    for (i = 0; i < avg_blocks; i++)
	delay[i] = 0.0;

    for (i = min_song_blocks; i < n_blocks - min_song_blocks; i++) {
       song_amp = (sample_buffer[i].max_value[0] + sample_buffer[i].max_value[1]);
       song_window_amp += song_amp - delay[delay_cntr];
       delay[delay_cntr] = song_amp;
       delay_cntr = (delay_cntr + 1) % avg_blocks;
       if (delay_cntr == 0)
	  valid_delay = 1;
       if (valid_delay) {
	  if (song_window_amp > silence) {
	     if (last_silence && i - last_song_block > min_song_blocks) {
		int loc = (i + (last_silence - min_silence_blocks)) / 2 * sample_buffer[i].n_samples;
		song_markers[num_song_markers++] = (loc / SONG_BLOCK_LEN) * SONG_BLOCK_LEN;
		if (num_song_markers >= MAX_MARKERS - 2)
		   break;
                last_song_block = i;
	     }
	     last_silence = 0;
	     silence_cntr = 0;
	  } else {
	     silence_cntr++;
	     if (silence_cntr > min_silence_blocks) {
		if (!last_silence) {
		   last_silence = i;
		}
	     }
	  }
       }
    }


#ifdef DETECT_GAPS
    if(1) {
	/* this attempts to detect entire gaps between songs, it does not work
	 * well yet and should be disabled except for testing... */
	long tmp_markers[MAX_MARKERS] ;
	int n_tmp = num_song_markers ;

	for(i = 0 ; i < num_song_markers ; i++)
	    tmp_markers[i] = song_markers[i] ;

	num_song_markers=0 ;

	for(i=0 ; i < n_tmp ; i++) {
	    int iblk = tmp_markers[i]/SBW ;
#define bsw 256
#define hbsw 64
	    double midpt_rms = sample_rms(sample_buffer,iblk,bsw) ;
	    int j ;

	    for(j = hbsw ; j < min_song_blocks ; j += hbsw) { 
		double lead_rms = sample_rms(sample_buffer,iblk-j,bsw) ;
		if(lead_rms > midpt_rms*1.3)
		    break ;
	    }

	    if(j > hbsw)
		song_markers[num_song_markers++] = (iblk-j)*SBW ;

	    for(j = hbsw ; j < min_song_blocks ; j += hbsw) { 
		double tail_rms = sample_rms(sample_buffer,iblk+j,bsw) ;
		if(tail_rms > midpt_rms*1.5)
		    break ;
	    }

	    if(j > hbsw)
		song_markers[num_song_markers++] = (iblk+j)*SBW ;
	}
    }
#endif
   
    snprintf(buf, sizeof(buf), "Marked %ld songs", num_song_markers + 1);
    set_status_text(buf);
    
    free(delay);

    main_redraw(FALSE, TRUE) ;
}
