/*****************************************************************************
*   Gnome Wave Cleaner Version 0.21
*   Copyright (C) 2001,2002,2003,2004,2005,2006 Jeffrey J. Welty
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

/* gwc.c */

#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#include "gwc.h"
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <glib.h>
#include "gtkledbar.h"
#include "encoding.h"
#include "soundfile.h"
#include "audio_edit.h"
#include <sndfile.h>

#include "icons/amplify.xpm"
#include "icons/pinknoise.xpm"
#include "icons/declick.xpm"
#include "icons/gtk-wave-cleaner.xpm"
#include "icons/declick_w.xpm"
#include "icons/declick_m.xpm"
#include "icons/decrackle.xpm"
#include "icons/estimate.xpm"
#include "icons/filter.xpm"
#include "icons/noise_sample.xpm"
#include "icons/remove_noise.xpm"
#include "icons/start.xpm"
#include "icons/stop.xpm"
#include "icons/zoom_sel.xpm"
#include "icons/zoom_in.xpm"
#include "icons/zoom_out.xpm"
#include "icons/view_all.xpm"
#include "icons/select_all.xpm"
#include "icons/spectral.xpm"
#ifndef TRUNCATE_OLD
#include "icons/silence.xpm"
#endif

#ifdef MAC_OS_X
// Note that we only tested if we are building on OSX, and are just assuming we are building with the GDK QUARTZ backend.
// We should really check that, as we could be building with the X11 backend.
#include <gtkmacintegration/gtkosxapplication.h>
//#import <Cocoa/Cocoa.h>
#endif

char pathname[PATH_MAX+1] = "./";
GtkWidget *dial[2];
GtkWidget *audio_canvas_w;

GtkWidget *audio_drawing_area;
GdkPixmap *audio_pixmap = NULL;
GdkPixmap *highlight_pixmap = NULL;
GdkPixmap *cursor_pixmap = NULL;
GtkObject *scroll_pos;
GtkWidget *hscrollbar;
GtkWidget *detect_only_widget;
GtkWidget *leave_click_marks_widget;
GtkWidget *main_window;

GtkWidget *l_file_time;
GtkWidget *l_file_samples;
GtkWidget *l_file_channels;
GtkWidget *l_file_rate;
GtkWidget *l_first_time;
GtkWidget *l_selected_time;
GtkWidget *l_last_time;
GtkWidget *l_samples;

struct sound_prefs prefs;
struct denoise_prefs denoise_prefs;
struct encoding_prefs encoding_prefs;
struct view audio_view;
struct click_data click_data;
int audio_playback = FALSE;
int audio_is_looping = FALSE;
//int cursor_playback = FALSE;
int batch_mode = 0 ;
long cursor_samples_per_playback_block;
gint playback_timer = -1 ;
gint cursor_timer;
gint spectral_view_flag = FALSE;
gint repair_clicks = 1;
double view_scale = 1.0;
double declick_sensitivity = 0.75;
double weak_declick_sensitivity = 1.00;
double strong_declick_sensitivity = 0.75;
double weak_fft_declick_sensitivity = 3.0 ;
double strong_fft_declick_sensitivity = 5.0 ;
int declick_detector_type = FFT_DETECT ;
double stop_key_highlight_interval = 0.5;
double song_key_highlight_interval = 15;
double song_mark_silence = 2.0;
int sonogram_log = 0;
gint declick_iterate_flag = 0;
double decrackle_level = 0.2;
gint decrackle_window = 2000;
gint decrackle_average = 3;
gint encoding_type = GWC_OGG;

extern double spectral_amp;

#ifdef HAVE_ALSA
char audio_device[256]="default";
#else
char audio_device[256]="/dev/dsp";
#endif

gint window_x;
gint window_y;
gint window_width = 800;
gint window_height = 580;
gboolean window_maximised;

gint doing_progressbar_update = FALSE;

DENOISE_DATA denoise_data = { 0, 0, 0, 0, FALSE };

gint debug = 0;

gchar save_selection_filename[PATH_MAX+1];
gchar wave_filename[PATH_MAX+1];
gchar last_filename[PATH_MAX+1];
gchar *file_extension;
gchar *tmpdir;
gchar *CLIPBOARD_FILE;

long markers[MAX_MARKERS];
long n_markers = 0;
long num_song_markers = 0;
long song_markers[MAX_MARKERS];

/* The file selection widget and the string to store the chosen filename */

GtkWidget *file_selector;
gchar *selected_filename;
gint file_is_open = FALSE;
gint file_processing = FALSE;
int stop_playback_force = 1 ;

void d_print(char *fmt, ...)
{
    if (debug) {
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
    }
}

static int audio_debug = 1 ;

void usage(char *prog)
{
	fprintf(stderr, "Usage:\n\
%s\n\
%s [file]\n\
%s <file> <batch[s] declick sensitivity start_position stop_position>\n\
%s <file> <batch[s] declick-hpf sensitivity start_position stop_position>\n\
%s <file> <batch[s] amplify amount start_position stop_position>\n\
%s <file> <batch[s] denoise sample_start sample_end denoise_start denoise_end>\n\
%s <file> <batch[s] normalize>\n\
%s <file> <batch[s] dsp start_position stop_position>\n\
%s <file> <batch[s] reverb start_position stop_position>\n\
%s <file> <batch[s] truncate keep_start keep_end>\n\
Positions are in hh:mm:ss for batch or in samples for batchs. Stop_position can \
also be end to denote the stop_position being the end of file.\n",
prog, prog, prog, prog, prog, prog, prog, prog, prog, prog);
	exit(EXIT_FAILURE);
}


void audio_debug_print(char *fmt, ...)
{
    if (audio_debug) {
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
    }
}

char *sample_to_time_text(long i, int rate, char *prefix, char *buf)
{
    int m, s, ms;

    m = i / (rate * 60);
    i -= m * rate * 60;

    s = i / rate;
    i -= s * rate;
    ms = 1000 * i / rate;

    sprintf(buf, "%s%d:%02d:%03d", prefix, m, s, ms);
    return buf;
}

void shellsort_long(long a[], int n)
{
    int gap, i, j;
    long tmp;

    for (gap = n / 2; gap > 0; gap /= 2) {
	for (i = gap; i < n; i++) {
	    for (j = i - gap; j >= 0 && a[j] > a[j + gap]; j -= gap) {
		tmp = a[j];
		a[j] = a[j + gap];
		a[j + gap] = tmp;
	    }
	}
    }

}

#ifndef TRUNCATE_OLD
static int is_region_selected(void)
{
    if (!audio_view.selection_region) {
        info("Please select a region of audio data.");
        return 0;
    }
    return 1;
}
#endif /* !TRUNCATE_OLD */


void append_cdrdao(struct view *v)
{
    // without this check it will append to cdrdao.toc even if no file is open
    if (file_is_open) {
        FILE *fp = fopen("cdrdao.toc", "a");
        long first, last ;

        if (fp == NULL) {
            fp = fopen("cdrdao.toc", "w");
        }

        if (fp == NULL) {
            warning(g_strconcat("Cannot write to ", g_get_current_dir(), "/cdrdao.toc: ", strerror(errno), NULL));  //this check prevents a segfault if we can't write to the file
        }
        else {
            get_region_of_interest(&first, &last, v) ;
            fprintf(fp, "TRACK AUDIO\n");
            fprintf(fp, "FILE \"%s\" %ld %ld\n", wave_filename, first, last - first + 1);
            fclose(fp);
            set_status_text(g_strconcat("Selection appended to ", g_get_current_dir(), "/cdrdao.toc", NULL));  //we should really review the code looking for other actions that should set the status like this
        }
    }
}



void display_times(void)
{
    char buf[50];
    long first, last;
    get_region_of_interest(&first, &last, &audio_view);

#ifndef OLD
    sprintf(buf, "Audio Channels: %d", prefs.stereo+1);
    gtk_label_set_text(GTK_LABEL(l_file_channels), buf);
    sprintf(buf, "Samplerate: %d Hz", prefs.rate);
    gtk_label_set_text(GTK_LABEL(l_file_rate), buf);
    gtk_label_set_text(GTK_LABEL(l_file_time),
		       sample_to_time_text(prefs.n_samples, prefs.rate,
					   "Track Length ", buf));
    sprintf(buf, "Track samples: %ld", audio_view.n_samples);
    gtk_label_set_text(GTK_LABEL(l_file_samples), buf);
    gtk_label_set_text(GTK_LABEL(l_first_time),
		       sample_to_time_text(first, prefs.rate, "First ",
					   buf));
    gtk_label_set_text(GTK_LABEL(l_last_time),
		       sample_to_time_text(last, prefs.rate, "Last ",
					   buf));
    gtk_label_set_text(GTK_LABEL(l_selected_time),
		       sample_to_time_text(last-first-1, prefs.rate, "Selected ",
					   buf));
    sprintf(buf, "Samples: %ld", last - first + 1);
    gtk_label_set_text(GTK_LABEL(l_samples), buf);
#else
    gtk_label_set_text(GTK_LABEL(l_file_time),
		       sample_to_time_text(prefs.n_samples, prefs.rate, "",
					   buf));
    gtk_label_set_text(GTK_LABEL(l_first_time),
		       sample_to_time_text(first, prefs.rate, "", buf));
    gtk_label_set_text(GTK_LABEL(l_last_time),
		       sample_to_time_text(last, prefs.rate, "", buf));
    sprintf(buf, " %ld", last - first + 1);
    gtk_label_set_text(GTK_LABEL(l_samples), buf);
#endif
}

void set_scroll_bar(long n, long first, long last)
{
    GtkAdjustment *a = (GtkAdjustment *) scroll_pos;
    double dn = n;
    double df = first;
    double dl = last;
    double ps = dl - df;

    a->lower = 0;
    a->upper = dn;
    a->value = df;
    a->page_size = ps;
    a->step_increment = ps / 8.0;
    a->page_increment = ps * 0.95;

/*      scroll_pos = gtk_adjustment_new(1.0, 0.0, 100.0, 10.0, 20.0, 20.0) ;  */
/*      a->lower = 0 ;  */
/*      a->upper = 100.0 ;  */
/*      a->value = 10.0 ;  */
/*      a->step_increment = 10.0 ;  */
/*      a->page_increment = 20.0 ;  */
/*      a->page_size = 20.0 ;  */

    gtk_adjustment_changed(a);
}

void scroll_bar_changed(GtkWidget * widget, gpointer data)
{
    GtkAdjustment *a = (GtkAdjustment *) scroll_pos;

    audio_view.first_sample = MAX(0, MIN(prefs.n_samples - 1, a->value));
    audio_view.last_sample =
	MAX(0, MIN(prefs.n_samples - 1, a->value + a->page_size));

    main_redraw(FALSE, TRUE);
    /* pause 1/3 second to allow the user to release the mouse button */
    usleep(333);
}

void get_region_of_interest(long *first, long *last, struct view *v)
{
    *first = v->selected_first_sample;
    *last = v->selected_last_sample;

    if (v->selection_region == FALSE) {
	*first = v->first_sample;
	*last = v->last_sample;
    }
}

GKeyFile* read_config(void)
{
    gchar     *config_file;
    GKeyFile  *key_file = NULL;
    GError    *error = NULL;

    config_file = g_build_filename (g_get_user_config_dir (), APPNAME, SETTINGS_FILE, NULL);
    //fprintf(stderr, "%s \n", config_file);
    key_file = g_key_file_new ();
    if (! g_key_file_load_from_file (key_file, config_file, G_KEY_FILE_KEEP_COMMENTS, &error)) {
	if (error->code != G_IO_ERROR_NOT_FOUND)
			// this is expected when running for the first time
			// therefore don't use g_warning, which will error if G_DEBUG environment variable is set to "fatal-warnings"
            g_message ("Could not load options file: %s\n", error->message);
        g_clear_error (&error);
    }
    g_free (config_file);
    return(key_file);
}

void write_config(GKeyFile *key_file)
{
    gchar     *file_data;
    gchar     *config_file;

    file_data = g_key_file_to_data (key_file, NULL, NULL);
    if (g_mkdir_with_parents (g_build_filename (g_get_user_config_dir (), APPNAME, NULL), 0755) != -1) {
        config_file = g_build_filename (g_get_user_config_dir (), APPNAME, SETTINGS_FILE, NULL);
        g_file_set_contents (config_file, file_data, -1, NULL);
        g_free (config_file);
    }
    else
        g_printerr ("Could not write settings file: %s\n", strerror (errno)); 
    g_free (file_data);
    g_key_file_free (key_file);
}

void load_preferences(void)
{
    GKeyFile  *key_file = read_config();
    // We should probably have a separate test for each preference...
    if (g_key_file_has_group(key_file, "config") == TRUE) {
        strcpy(pathname, g_key_file_get_string(key_file, "config", "pathname", NULL));
        strcpy(last_filename, g_key_file_get_string(key_file, "config", "last_filename", NULL));
        audio_view.first_sample = g_key_file_get_integer(key_file, "config", "first_sample_viewed", NULL);
        audio_view.last_sample = g_key_file_get_integer(key_file, "config", "last_sample_viewed", NULL);
        // What's going on here with num_song_markers?
    num_song_markers = 0;
        weak_declick_sensitivity = g_key_file_get_double(key_file, "config", "weak_declick_sensitivity", NULL);
        strong_declick_sensitivity = g_key_file_get_double(key_file, "config", "strong_declick_sensitivity", NULL);
        declick_iterate_flag = g_key_file_get_integer(key_file, "config", "declick_iterate", NULL);
        weak_fft_declick_sensitivity = g_key_file_get_double(key_file, "config", "weak_fft_declick_sensitivity", NULL);
        strong_fft_declick_sensitivity = g_key_file_get_double(key_file, "config", "strong_fft_declick_sensitivity", NULL);
        declick_detector_type = g_key_file_get_integer(key_file, "config", "declick_detector_type", NULL);
        decrackle_level = g_key_file_get_double(key_file, "config", "decrackle_level", NULL);
        decrackle_window = g_key_file_get_integer(key_file, "config", "decrackle_window", NULL);
        decrackle_average = g_key_file_get_integer(key_file, "config", "decrackle_average", NULL);
        stop_key_highlight_interval = g_key_file_get_double(key_file, "config", "stop_key_highlight_interval", NULL);
        song_key_highlight_interval = g_key_file_get_double(key_file, "config", "song_key_highlight_interval", NULL);
        song_mark_silence = g_key_file_get_double(key_file, "config", "song_mark_silence", NULL);
        sonogram_log = g_key_file_get_double(key_file, "config", "sonogram_log", NULL);
/*      audio_view.truncate_tail = g_key_file_get_integer(key_file, "config", "truncate_tail", NULL) ;  */
/*      audio_view.truncate_head = g_key_file_get_integer(key_file, "config", "truncate_head", NULL) ;  */
        strcpy(audio_device, g_key_file_get_string(key_file, "config", "audio_device", NULL));
    }
    if (g_key_file_has_group(key_file, "window") == TRUE) {
    window_width = g_key_file_get_integer(key_file, "window", "width", NULL);
    window_height = g_key_file_get_integer(key_file, "window", "height", NULL);
    window_x = g_key_file_get_integer(key_file, "window", "x", NULL);
    window_y = g_key_file_get_integer(key_file, "window", "y", NULL);
    window_maximised = g_key_file_get_boolean(key_file, "window", "maximised", NULL);
    }
    g_key_file_free (key_file);
}

void save_preferences(void)
{
    GKeyFile  *key_file = read_config();

    g_key_file_set_string(key_file, "config", "pathname", pathname);
    g_key_file_set_string(key_file, "config", "last_filename", last_filename);
    g_key_file_set_integer(key_file, "config", "first_sample_viewed", audio_view.first_sample);
    g_key_file_set_integer(key_file, "config", "last_sample_viewed", audio_view.last_sample);
    g_key_file_set_double(key_file, "config", "weak_declick_sensitivity", weak_declick_sensitivity);
    g_key_file_set_double(key_file, "config", "strong_declick_sensitivity", strong_declick_sensitivity);
    g_key_file_set_integer(key_file, "config", "declick_iterate", declick_iterate_flag);
    g_key_file_set_double(key_file, "config", "weak_fft_declick_sensitivity", weak_fft_declick_sensitivity);
    g_key_file_set_double(key_file, "config", "strong_fft_declick_sensitivity", strong_fft_declick_sensitivity);
    g_key_file_set_integer(key_file, "config", "declick_detector_type", declick_detector_type);
    g_key_file_set_double(key_file, "config", "decrackle_level", decrackle_level);
    g_key_file_set_integer(key_file, "config", "decrackle_window", decrackle_window);
    g_key_file_set_integer(key_file, "config", "decrackle_average", decrackle_average);
    g_key_file_set_double(key_file, "config", "stop_key_highlight_interval", stop_key_highlight_interval);
    g_key_file_set_double(key_file, "config", "song_key_highlight_interval", song_key_highlight_interval);
    g_key_file_set_double(key_file, "config", "song_mark_silence", song_mark_silence);
    g_key_file_set_integer(key_file, "config", "sonogram_log", sonogram_log);
    g_key_file_set_string(key_file, "config", "audio_device", audio_device);
    g_key_file_set_integer(key_file, "window", "width", window_width);
    g_key_file_set_integer(key_file, "window", "height", window_height);
    g_key_file_set_integer(key_file, "window", "x", window_x);
    g_key_file_set_integer(key_file, "window", "y", window_y);
    g_key_file_set_boolean(key_file, "window", "maximised", window_maximised);

/*      g_key_file_set_integer(key_file, "config", "truncate_head", audio_view.truncate_head) ;  */
/*      g_key_file_set_integer(key_file, "config", "truncate_tail", audio_view.truncate_tail) ;  */
    write_config(key_file);
}

/*  void main_set_preferences(GtkWidget * widget, gpointer data)  */
/*  {  */
/*      preferences_dialog(prefs);  */
/*  }  */

void display_message(char *msg, char *title) 
{
    GtkWidget *dlg, *txt;

    dlg = gtk_dialog_new_with_buttons(title,
    				      GTK_WINDOW(main_window),
				      GTK_DIALOG_DESTROY_WITH_PARENT,
				      GTK_STOCK_OK,
				      GTK_RESPONSE_NONE,
				      NULL);

    txt = gtk_label_new(msg);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox), txt, TRUE, TRUE, 0) ;

    /* Alister: disable since we have the file_selector closing even if opening fails
    // Also for the matching code in other functions
    // Alister: doing this (and for the other dialogs?) means that if the user raises
    // another window above gwc, then clicks on the dialog, the main_window
    // isn't raised, only the two dialogs.
    // But that is better than if we don't do this, in which case the user can raise the
    // file_selector dialog above the display_message dialog.
    if (GTK_WIDGET_VISIBLE(file_selector)) {
        gtk_window_set_transient_for((GtkWindow*) dlg, (GtkWindow*) file_selector);
    }*/
    gtk_widget_show_all(dlg) ;

    gtk_dialog_run(GTK_DIALOG(dlg)) ;

    gtk_widget_destroy(txt) ;
    gtk_widget_destroy(dlg) ;

    main_redraw(FALSE, TRUE);
}

void warning(char *msg)
{
    display_message(msg, "WARNING");
}

void info(char *msg)
{
    display_message(msg, "");
}

int yesnocancel(char *msg)
{
    GtkWidget *dlg, *text;
    gint dres;

    dlg =
	gtk_dialog_new_with_buttons("Question",
				    GTK_WINDOW(main_window),
				    GTK_DIALOG_DESTROY_WITH_PARENT,
				    GTK_STOCK_CANCEL,
				    GTK_RESPONSE_CANCEL,
				    GTK_STOCK_NO,
				    GTK_RESPONSE_NO,
				    GTK_STOCK_YES,
				    GTK_RESPONSE_YES,
				    NULL);
    // This is right for the keep changes dialog, but not for the truncation dialog #ifdef TRUNCATE_OLD
    // But it is probably time to remove that... I didn't cater to it when migrating from GnomeUIInfo to GtkUIManager and GtkAction // 20210210 need to check if this is still right
    gtk_dialog_set_default_response (GTK_DIALOG(dlg), GTK_RESPONSE_YES);

    text = gtk_label_new(msg);
    gtk_widget_show(text);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox), text, TRUE, TRUE, 0);

    gtk_widget_show_all(dlg) ;

    /*
    if (GTK_WIDGET_VISIBLE(file_selector)) {
        gtk_window_set_transient_for((GtkWindow*) dlg, (GtkWindow*) file_selector);
    }*/
    dres = gtk_dialog_run(GTK_DIALOG(dlg));

    gtk_widget_destroy(dlg) ;


    if (dres == GTK_RESPONSE_NONE || dres == GTK_RESPONSE_CANCEL) {
	dres = 2 ;		/* return we clicked cancel */
    } else if (dres == GTK_RESPONSE_YES) {
	dres = 0 ; 		/* return we clicked yes */
    } else {
	dres = 1 ;
    }

    main_redraw(FALSE, TRUE);

    return dres;
}

int yesno(char *msg)
{
    GtkWidget *dlg, *text;
    int dres;

    dlg = gtk_dialog_new_with_buttons("Question",
    				        GTK_WINDOW(main_window),
				        GTK_DIALOG_DESTROY_WITH_PARENT,
					 GTK_STOCK_NO,
					 GTK_RESPONSE_NO,
					GTK_STOCK_YES,
					GTK_RESPONSE_YES,
					 NULL);
    gtk_dialog_set_default_response (GTK_DIALOG(dlg), GTK_RESPONSE_NO);

    text = gtk_label_new(msg);
    gtk_widget_show(text);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox), text, TRUE, TRUE, 0);
    /*
    if (GTK_WIDGET_VISIBLE(file_selector)) {
        gtk_window_set_transient_for((GtkWindow*) dlg, (GtkWindow*) file_selector);
    }*/
    gtk_widget_show_all(dlg) ;

    dres = gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg) ;

    if (dres == GTK_RESPONSE_NONE || dres == GTK_RESPONSE_NO) {
	dres = 1 ;		/* return we clicked no */
    } else {
	dres = 0 ; 		/* return we clicked yes */
    }

    main_redraw(FALSE, TRUE);

    return dres;
}

int prompt_user(char *msg, char *s, int maxlen)
{
    GtkWidget *dlg, *text, *entry ;
    int dres;

    dlg = gtk_dialog_new_with_buttons("Input Requested",
    				        GTK_WINDOW(main_window),
				        GTK_DIALOG_DESTROY_WITH_PARENT,
					 GTK_STOCK_CANCEL,
					 GTK_RESPONSE_CANCEL,
					GTK_STOCK_OK,
					GTK_RESPONSE_OK,
					 NULL);
    gtk_dialog_set_default_response (GTK_DIALOG(dlg), GTK_RESPONSE_OK);

    text = gtk_label_new(msg);
    gtk_widget_show(text);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox), text, TRUE, TRUE, 0);

    entry = gtk_entry_new_with_max_length(maxlen);
    gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);

    if(strlen(s) > 0)
	gtk_entry_set_text(GTK_ENTRY(entry), s);

    gtk_widget_show(entry);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox), entry, TRUE, TRUE, 0);

    /*
    if (GTK_WIDGET_VISIBLE(file_selector)) {
        gtk_window_set_transient_for((GtkWindow*) dlg, (GtkWindow*) file_selector);
    }*/
    gtk_widget_show_all(dlg) ;

    dres = gtk_dialog_run(GTK_DIALOG(dlg));

    if (dres == GTK_RESPONSE_NONE || dres == GTK_RESPONSE_NO) {
	dres = 1 ;		/* return we clicked cancel */
    } else {
	dres = 0 ; 		/* return we clicked yes */
	strcpy(s, gtk_entry_get_text(GTK_ENTRY(entry)));
    }

    gtk_widget_destroy(dlg) ;

    main_redraw(FALSE, TRUE);

    return dres;
}

void help(GtkWidget * widget, gpointer data)
{
	
/* This shows the help in yelp and requires /usr/share/gnome/help/gwc/C/gwc.html (or similar, if not using html documentation)
#if GTK_CHECK_VERSION(2,14,0)
//  GdkScreen *screen;
//  Need some more includes or something for this display stuff
//  DDisplay *ddisp;
//  ddisp = ddisplay_active();
//  screen = gtk_widget_get_screen (GTK_WIDGET(ddisp->menu_bar));
//  opens /usr/share/gnome/help/gwc/gwc.html (or similar, if not using html documentation)
//  if (gtk_show_uri(screen, "ghelp:gwc", gtk_get_current_event_time (), NULL))
  if (gtk_show_uri(NULL, "ghelp:gwc", gtk_get_current_event_time (), NULL))
    return;
#endif
*/

  // We prefer to show the help in a browser than yelp.
  // We should probably modify this so that it works if running GWC from the build directory without installing, and/or
  // so that it shows a warning message if the help file does not exist.  But should figure out test for gvfs first.

  char *uri = g_strconcat ("file://", HELPDIR, "/", APPNAME, "/", APPNAME, ".html", NULL);
  #ifdef MAC_OS_X
	  if ( gtkosx_application_get_bundle_id() )
	    uri = g_strconcat ("file://", g_uri_escape_string(gtkosx_application_get_resource_path(), "/", TRUE), HELPDIR, "/", APPNAME, "/", APPNAME, ".html", NULL);
	  //g_message("testing %s", uri);
	  char *command = g_strdup_printf("%s %s &", "open", uri);
	  system(command);
	  g_free(command);
	// I had compile problems trying to do it using this method!
	/*	NSURL	*ns_url;
		gboolean retval;

		//neither work
		//NSAutoreleasePool *pool [[NSAutoreleasePool alloc] init];
		@autoreleasepool
		{
			ns_url = NSURL URLWithString: [NSString stringWithUTF8String: "file:///users/alister/gwc/doc/gtk-wave-cleaner.html"];
			retval = [[NSWorkspace sharedWorkspace] openURL: ns_url];
		}
		return retval;
	*/	
		
  # else
  /*
  // This is infuriating as it silently fails if gvfs is not installed, and it will freeze gwc
  // if gvfs is installed but broken (e.g. because the dbus session isn't working correctly)!
	if GTK_CHECK_VERSION(2,14,0)
  	{
		// not sure if this does what I want
  		GdkScreen *screen = gtk_widget_get_screen (main_window);
  		// First try gtk_show_uri(), which fails if gvfs is not installed
  		// Then use xdg-open, which should work in almost all cases.
  		// If we were keen we could copy pragha's src/utils.c, which then tries firefox, mozilla, opera...
  		if ( !gtk_show_uri(screen, uri, gtk_get_current_event_time (), NULL) )
  		{
			char *command = g_strdup_printf("%s %s &", command ? command : "xdg-open", uri);
			system(command);
			g_free(command);
		}
	}
	else
  	{ */
		// I used to think that xdg-open was inferior because it used a hard-coded list of browsers,
		// but it actually uses the $BROWSER environment variable if set
  		char *command = g_strdup_printf("%s %s &",  "xdg-open", uri);
  		system(command);
  		g_free(command);
//	}
  #endif
  g_free(uri);
}

void declick_with_sensitivity(double sensitivity)
{
    long first, last;
    char *result_msg;
    gint leave_click_marks ;

    repair_clicks =
	gtk_toggle_button_get_active((GtkToggleButton *)detect_only_widget) == TRUE ? FALSE : TRUE;

    leave_click_marks =
	gtk_toggle_button_get_active((GtkToggleButton *)leave_click_marks_widget) == TRUE ? TRUE : FALSE ;

    if (repair_clicks == TRUE)
	start_save_undo("Undo declick", &audio_view);

    get_region_of_interest(&first, &last, &audio_view);

    push_status_text(repair_clicks ==
		     TRUE ? "Declicking selection" : "Detecting clicks");

    click_data.max_clicks = MAX_CLICKS;

    result_msg =
	do_declick(&prefs, first, last, audio_view.channel_selection_mask,
		   sensitivity, repair_clicks, &click_data,
		   declick_iterate_flag,leave_click_marks);

    if (repair_clicks == TRUE) {
	resample_audio_data(&prefs, first, last);
	save_sample_block_data(&prefs);
    }

    pop_status_text();
    set_status_text(result_msg);

    if (repair_clicks == TRUE) {
	close_undo();
    }

    main_redraw(FALSE, TRUE);
}

void declick(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE)
	&& (audio_playback == FALSE)) {

	file_processing = TRUE;

	if(declick_detector_type == HPF_DETECT)
	    declick_with_sensitivity(strong_declick_sensitivity);
	else
	    declick_with_sensitivity(strong_fft_declick_sensitivity);

	file_processing = FALSE;
    }
}

void declick_weak(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE)
	&& (audio_playback == FALSE)) {

	file_processing = TRUE;

	if(declick_detector_type == HPF_DETECT)
	    declick_with_sensitivity(weak_declick_sensitivity);
	else
	    declick_with_sensitivity(weak_fft_declick_sensitivity);

	file_processing = FALSE;
    }
}

void estimate(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE)
	&& (audio_playback == FALSE)) {
	long first, last;
	file_processing = TRUE;
	get_region_of_interest(&first, &last, &audio_view);
	dethunk(&prefs, first, last, audio_view.channel_selection_mask);
	main_redraw(FALSE, TRUE);
	set_status_text("Estimate done.");
	file_processing = FALSE;
    }
}

void manual_declick(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE)
	&& (audio_playback == FALSE)) {
	int doit = TRUE;
	long first, last;
	file_processing = TRUE;
	get_region_of_interest(&first, &last, &audio_view);

	if (last - first > 299) {
	    char msg_buf[1000] ;
	    double n = last-first+1 ;
	    double a = 100 ;
	    double elements = (
				    n +
				    (n-a) * n +
				    (n-a) * n +
				    (n-a) * n +
				    (n-a) * (n-a) +
				    (n-a) * (n-a) +
				    n * n
				) ;
	    double bytes = elements * sizeof(double) / (double) (1 << 20) ;
	    char *units = "Megabytes" ;

	    if(bytes > 1000) {
		bytes /= (double) 1024 ;
		units = "Gigabytes" ;
	    }

	    if(bytes > 1000) {
		bytes /= (double) 1024 ;
		units = "Terabytes" ;
	    }

	    sprintf(msg_buf, "Repairing > 300 samples  may cause a crash\nYou have selected %lg samples, which will require about %8.0lf %s of memory and a long time.",
			    n, bytes, units ) ;
	    doit = FALSE;
	    if (!yesno(msg_buf))
		doit = TRUE;
	}

	if (doit == TRUE) {

	    start_save_undo("Undo declick", &audio_view);

	    push_status_text("Declicking selection");
	    declick_a_click(&prefs, first, last,
			    audio_view.channel_selection_mask);
	    resample_audio_data(&prefs, first, last);
	    save_sample_block_data(&prefs);
	    pop_status_text();
	    set_status_text("Manual declick done.");

	    close_undo();

	    main_redraw(FALSE, TRUE);
	}

	file_processing = FALSE;
    }
}

void decrackle(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE)
	&& (audio_playback == FALSE)) {
	int cancel;
	long first, last;

	file_processing = TRUE;
	push_status_text("Saving undo information");

	start_save_undo("Undo decrackle", &audio_view);
	get_region_of_interest(&first, &last, &audio_view);
	cancel = save_undo_data(first, last, &prefs, TRUE);

	close_undo();

	pop_status_text();

	if (cancel != 1) {
	    push_status_text("Decrackling selection");
	    do_decrackle(&prefs, first, last,
			 audio_view.channel_selection_mask,
			 decrackle_level, decrackle_window,
			 decrackle_average);
	    resample_audio_data(&prefs, first, last);
	    save_sample_block_data(&prefs);
	    pop_status_text();
	    set_status_text("Decrackle done.");
	}

	main_redraw(FALSE, TRUE);
	file_processing = FALSE;
    }
}

void noise_sample(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE)
	&& (audio_playback == FALSE)) {
	file_processing = TRUE;
	get_region_of_interest(&denoise_data.noise_start,
			       &denoise_data.noise_end, &audio_view);
	denoise_data.ready = TRUE;
	load_denoise_preferences() ;
	//print_noise_sample(&prefs, &denoise_prefs, denoise_data.noise_start, denoise_data.noise_end) ;
	file_processing = FALSE;
    }
}

void remove_noise(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE)
	&& (audio_playback == FALSE)) {
	file_processing = TRUE;
	if (denoise_data.ready == FALSE) {
	    warning("Please select the noise sample first");
	} else {
	    get_region_of_interest(&denoise_data.denoise_start,
				   &denoise_data.denoise_end, &audio_view);
	    load_denoise_preferences();
	    print_denoise("remove_noise", &denoise_prefs);
	    {
		int cancel;

		if (denoise_prefs.FFT_SIZE >
		    (denoise_data.noise_end - denoise_data.noise_start +
		     1)) {
		    warning
			("FFT_SIZE must be <= # samples in noise sample!");
		    main_redraw(FALSE, TRUE);
		    file_processing = FALSE ;
		    return;
		}

		push_status_text("Saving undo information");

		start_save_undo("Undo denoise", &audio_view);
		cancel =
		    save_undo_data(denoise_data.denoise_start,
				   denoise_data.denoise_end, &prefs, TRUE);
		close_undo();

		pop_status_text();

		if (cancel != 1) {
		    push_status_text("Denoising selection");
		    denoise(&prefs, &denoise_prefs,
			    denoise_data.noise_start,
			    denoise_data.noise_end,
			    denoise_data.denoise_start,
			    denoise_data.denoise_end,
			    audio_view.channel_selection_mask);
		    resample_audio_data(&prefs, denoise_data.denoise_start,
					denoise_data.denoise_end);
		    save_sample_block_data(&prefs);
		    pop_status_text();
		}
		set_status_text("Denoise done.");

		main_redraw(FALSE, TRUE);
	    }
	}
	file_processing = FALSE;
    }
}

void undo_callback(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE)
	&& (audio_playback == FALSE)) {
	file_processing = TRUE;
	undo(&audio_view, &prefs);
	main_redraw(FALSE, TRUE);
	file_processing = FALSE;
    } else {
	if (file_is_open == FALSE) {
	    warning("Nothing to Undo Yet.");
	} else
	    warning
		("Please try Undo when processing or Audio Playback has stopped");
    }
}


void scale_up_callback(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE)
	&& (audio_playback == FALSE)) {
	file_processing = TRUE;
	view_scale *= 1.25;
	main_redraw(FALSE, TRUE);
	file_processing = FALSE;
    }
}

void cut_callback(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE)
	&& (audio_playback == FALSE)) {
#ifdef TRUNCATE_OLD
	long start, end;

	file_processing = TRUE;
	get_region_of_interest(&start, &end, &audio_view);

	if (start < prefs.n_samples - end) {
	    audio_view.truncate_head = end + 1;
	} else {
	    audio_view.truncate_tail = start - 1;
	}

	main_redraw(FALSE, TRUE);
	file_processing = FALSE;
#else /* !TRUNCATE_OLD */
        if (is_region_selected()) {
            long first, last;
            get_region_of_interest(&first, &last, &audio_view);

            if (first == 0 && last == prefs.n_samples - 1) {
                info("Can't cut ALL audio data from file.");
            } else {
                file_processing = TRUE;
                int rc = audioedit_cut_selection(&audio_view);
                    if (rc == 0) {
                        set_status_text("Cut done.");
                        main_redraw(FALSE, TRUE);
                    }
                    else
                        warning(g_strconcat("Cut failed - could not write to ", CLIPBOARD_FILE, NULL));
                file_processing = FALSE;
            }
        }
#endif /* !TRUNCATE_OLD */
    }
}
#ifndef TRUNCATE_OLD
void copy_callback(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE)
	&& (audio_playback == FALSE)) {
        if (is_region_selected()) {
            file_processing = TRUE;
                int rc = audioedit_copy_selection(&audio_view);
                if (! rc == 0)
                    warning(g_strconcat("Copy failed - could not write to ", CLIPBOARD_FILE, NULL));
            file_processing = FALSE;
        }
    }
}

void paste_callback(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE)
	&& (audio_playback == FALSE)) {
        if (is_region_selected()) {
            if (audioedit_has_clipdata()) {
                file_processing = TRUE;
                audioedit_paste_selection(&audio_view);
                set_status_text("Paste done.");
                main_redraw(FALSE, TRUE);
                file_processing = FALSE;
            } else {
                info("No audio data in internal clipboard.");
            }
        }
    }
}

void delete_callback(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE)
	&& (audio_playback == FALSE)) {
        if (is_region_selected()) {
            long first, last;
            get_region_of_interest(&first, &last, &audio_view);

            if (first == 0 && last == prefs.n_samples - 1) {
                info("Can't delete ALL audio data from file.");
            } else {
                file_processing = TRUE;
                audioedit_delete_selection(&audio_view);
                set_status_text("Delete done.");
                main_redraw(FALSE, TRUE);
                file_processing = FALSE;
            }
        }
    }
}

void silence_callback(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE)
	&& (audio_playback == FALSE)) {
        if (is_region_selected()) {

	    if(!yesno("Insert silence can take a long time, continue?")) {
		file_processing = TRUE;
		audioedit_insert_silence(&audio_view);
		set_status_text("Silence done.");
		main_redraw(FALSE, TRUE);
		file_processing = FALSE;
	    }
        }
    }
}
#endif /* !TRUNCATE_OLD */

void scale_reset_callback(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE)
	&& (audio_playback == FALSE)) {
	file_processing = TRUE;
	view_scale = 1.00;
	spectral_amp = 1.00;
	main_redraw(FALSE, TRUE);
	file_processing = FALSE;
    }
}

void scale_down_callback(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE)
	&& (audio_playback == FALSE)) {
	file_processing = TRUE;
	view_scale /= 1.25;
	main_redraw(FALSE, TRUE);
	file_processing = FALSE;
    }
}

int count = 0;

long prev_cursor_millisec = -1;

/* This is a callback function. The data arguments are ignored
* in this example. More on callbacks below. */
void stop_all_playback_functions(GtkWidget * widget, gpointer data)
{
    if (playback_timer != -1) {
	gtk_timeout_remove(playback_timer);
	playback_timer = -1 ;
    }

/*      if(stop_playback_force && cursor_playback == TRUE) {  */
/*  	gtk_timeout_remove(cursor_timer);  */
/*  	cursor_playback = FALSE;  */
/*      }  */

    stop_playback(stop_playback_force);

    audio_playback = FALSE;

    led_bar_light_percent(dial[0], 0.0, 0.0);
    led_bar_light_percent(dial[1], 0.0, 0.0);
}

/* This is a callback function. The data arguments are ignored
* in this example. More on callbacks below. */
void record(GtkWidget * widget, gpointer data)
{
/*   stop_recording() ;  */
}

void gnome_flush(void)
{
    while (gtk_events_pending())
	gtk_main_iteration();
}

/*  * When the timer goes off:  */
/*      - Send data to the audio device, if there is more to send  */
/*      - Determine the cursor position from the audio device # samples played  */
/*      - if the cursor position is within 1/20 sec of last sample to play  */
/*  	. stop the timer  */
/*      	. close the audio device, with flag to drain (play) all remaining samples  */
/*  	. set the cursor position at the last sample to play  */
/*      - redraw the screen  */

gint playback_timer_function(gpointer data)
{
    gfloat l, r;
    gfloat l10, r10; // max led level in past 10 blocks
    long last, first;

    get_region_of_interest(&first, &last, &audio_view);

    //audio_debug_print("enter update_cursor with audio_playback:%d\n", audio_playback) ;

    if (audio_playback == TRUE) {
	process_audio(&l, &r) ;

	audio_view.expected_frames_played += audio_view.expected_frames_per_timer_update ;
	set_playback_cursor_position(&audio_view, audio_view.timer_msec);

	float secs_remaining = (float)(last-audio_view.cursor_position)/(float)prefs.rate ;

	if(!audio_is_looping && secs_remaining < .01) {
	    audio_debug_print("update_cursor is ending audio_playback CP:%ld, last:%ld diff:%ld\n", audio_view.cursor_position, last, last-audio_view.cursor_position) ;
	    audio_debug_print("prefs.rate:%ld\n", prefs.rate) ;
	    gtk_timeout_remove(playback_timer);
	    stop_playback(0) ;
	    audio_view.cursor_position=last ;
	    audio_playback = FALSE ;
	    led_bar_light_percent(dial[0], 0., 0.);
	    led_bar_light_percent(dial[1], 0., 0.);
	}  else {
	    //audio_debug_print("update_cursor with audio_playback:%ld, secs:%f\n", audio_view.cursor_position, secs_remaining) ;
	    get_led_levels(&l, &r, &l10, &r10, audio_view.cursor_position-first) ;
	    led_bar_light_percent(dial[0], l, l10);
	    led_bar_light_percent(dial[1], r, r10);
	}

	main_redraw(TRUE, TRUE);
	//audio_debug_print(".\n") ;
    }
    return (TRUE);
}

/*  * To start playback:  */
/*      - Open the audio device  */
/*      - Start a timer (at 1/20th second)  */
/*      - Send data to the audio device  */
/*      - set the cursor position at the first sample to play  */
/*      - redraw the screen  */

/* This is a callback function. The data arguments are ignored
* in this example. More on callbacks below. */
void start_gwc_playback(GtkWidget * widget, gpointer data)
{				/* Play audio */
    int device_framesize ;

    audio_debug_print("entering start_gwc_playback with audio_playback=%d\n", audio_playback) ;

    if (file_is_open == TRUE && file_processing == FALSE && audio_playback == FALSE) {

	audio_view.cursor_position = start_playback(audio_device, &audio_view, &prefs, 0.0, &device_framesize);

	if(audio_view.cursor_position < 0)
	    return ; // an error occurred

	audio_view.playback_prev_cursor_position = audio_view.cursor_position ;

	audio_playback = TRUE;
	audio_debug_print("START GWC PLAYBACK audio cursor position=%ld\n", audio_view.cursor_position) ;

	if (audio_playback == TRUE) {
	    gfloat l, r;
	    audio_debug_print("start_gwc_playback starting playback timers\n") ;
	    process_audio(&l, &r) ;
	    long timer_msec = 25 ;  // this will cause 40 screen updates (and loading of audio buffers) per second

#ifdef HAVE_ALSA
	    timer_msec = 25 ;
#else
# ifdef HAVE_PULSE_AUDIO
	    float msec_for_timer = (float)device_framesize/(float)prefs.rate*1000. ;
	    timer_msec = (long)(msec_for_timer)-4 ;
	    if(timer_msec > 25) timer_msec = 25 ;
	    audio_debug_print("PulseAudio, framesize:%d msec for playback timer is=%d\n", device_framesize, timer_msec) ;
# else
#  ifdef MAC_OS_X /* MacOSX */
	    timer_msec = 25 ;
#  else
	    timer_msec = 25 ;
#  endif 
# endif 
#endif

	    audio_view.expected_frames_per_timer_update = (float)timer_msec/1000.f*(float)prefs.rate ;
	    audio_view.expected_frames_played = 0.f ;
	    audio_view.timer_msec = timer_msec ; // milliseconds the timer is set to repeat
	    playback_timer = gtk_timeout_add(audio_view.timer_msec, playback_timer_function, NULL);
	}
    }

    audio_debug_print("leaving start_gwc_playback with audio_playback=%d\n", audio_playback) ;

    audio_is_looping = FALSE ;
}

void detect_only_func(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE)
	&& (audio_playback == FALSE)) {
	file_processing = TRUE;
	repair_clicks =
	    gtk_toggle_button_get_active((GtkToggleButton *) widget) ==
	    TRUE ? FALSE : TRUE;
	g_print("detect_only_func called: %s\n", (char *) data);
	file_processing = FALSE;
    }
}

/* This is a callback function. The data arguments are ignored
* in this example. More on callbacks below. */
void amplify(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE)
	&& (audio_playback == FALSE)) {
	file_processing = TRUE;
	if (amplify_dialog(prefs, &audio_view)) {
	    long first, last;
	    int cancel;
	    get_region_of_interest(&first, &last, &audio_view);

	    push_status_text("Saving undo information");
	    start_save_undo("Undo amplify", &audio_view);
	    cancel = save_undo_data(first, last, &prefs, TRUE);
	    close_undo();
	    pop_status_text();

	    if (cancel != 1) {
		amplify_audio(&prefs, first, last,
			      audio_view.channel_selection_mask);
		save_sample_block_data(&prefs);
		set_status_text("Amplify done.");
	    }
	    main_redraw(FALSE, TRUE);
	}
	file_processing = FALSE;
    }
}

/* This is a callback function. The data arguments are ignored
* in this example. More on callbacks below. */
void reverb(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE)
	&& (audio_playback == FALSE)) {
	file_processing = TRUE;
	if (reverb_dialog(prefs, &audio_view)) {
	    long first, last;
	    int cancel;
	    get_region_of_interest(&first, &last, &audio_view);

	    push_status_text("Saving undo information");
	    start_save_undo("Undo reverb", &audio_view);
	    cancel = save_undo_data(first, last, &prefs, TRUE);
	    close_undo();
	    pop_status_text();

	    if (cancel != 1) {
		reverb_audio(&prefs, first, last,
			      audio_view.channel_selection_mask);
		save_sample_block_data(&prefs);
		set_status_text("Reverb done.");
	    }
	    main_redraw(FALSE, TRUE);
	}
	file_processing = FALSE;
    }
}

/* This is a callback function. The data arguments are ignored
* in this example. More on callbacks below. */
void pinknoise_cb(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE)
	&& (audio_playback == FALSE)) {
	file_processing = TRUE;

	if (pinknoise_dialog(prefs, &audio_view)) {
	    long first, last;
	    int cancel;
	    get_region_of_interest(&first, &last, &audio_view);

	    push_status_text("Saving undo information");
	    start_save_undo("Undo pinknoise", &audio_view);
	    cancel = save_undo_data(first, last, &prefs, TRUE);
	    close_undo();
	    pop_status_text();

	    if (cancel != 1) {
		pinknoise(&prefs, first, last,
			      audio_view.channel_selection_mask);
		save_sample_block_data(&prefs);
		set_status_text("Pink Noise done.");
	    }
	    main_redraw(FALSE, TRUE);
	}

	file_processing = FALSE;
    }
}

/* This is a callback function. The data arguments are ignored
* in this example. More on callbacks below. */
void filter_cb(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE)
	&& (audio_playback == FALSE)) {
	file_processing = TRUE;

	if (filter_dialog(prefs, &audio_view)) {
	    long first, last;
	    int cancel;
	    get_region_of_interest(&first, &last, &audio_view);

	    push_status_text("Saving undo information");
	    start_save_undo("Undo filter", &audio_view);
	    cancel = save_undo_data(first, last, &prefs, TRUE);
	    close_undo();
	    pop_status_text();

	    if (cancel != 1) {
		filter_audio(&prefs, first, last,
			      audio_view.channel_selection_mask);
		save_sample_block_data(&prefs);
		set_status_text("Filter done.");
	    }
	    main_redraw(FALSE, TRUE);
	}

	file_processing = FALSE;
    }
}

void zoom_select(GtkWidget * widget, gpointer data)
{
	if ((file_processing == FALSE) && (file_is_open == TRUE)) {
		stop_all_playback_functions(widget, data);
		file_processing = TRUE;

		if(audio_view.selected_first_sample == audio_view.selected_last_sample) {
			audio_view.first_sample = audio_view.selected_first_sample - prefs.rate+1 ;
			audio_view.last_sample = audio_view.selected_last_sample + prefs.rate ;
		} else {
			audio_view.first_sample = audio_view.selected_first_sample;
			audio_view.last_sample = audio_view.selected_last_sample;
		}

		if(audio_view.selected_first_sample < 0) audio_view.selected_first_sample = 0 ;
		if(audio_view.selected_last_sample > prefs.n_samples-1) audio_view.selected_last_sample = prefs.n_samples-1 ;
		set_scroll_bar(prefs.n_samples - 1, audio_view.first_sample,
				   audio_view.last_sample);
		/* set_scroll_bar redraws */
		/*main_redraw(FALSE, TRUE) ; */
	file_processing = FALSE;
	}
}

void select_all(GtkWidget * widget, gpointer data)
{
	audio_view.selected_first_sample = audio_view.first_sample;
	audio_view.selected_last_sample = audio_view.last_sample;
	audio_view.selection_region = TRUE;
	main_redraw(FALSE, TRUE);
}

void select_none(GtkWidget * widget, gpointer data)
{
	audio_view.selection_region = FALSE;
	audio_view.channel_selection_mask = 3 ;
	main_redraw(FALSE, TRUE);
}

void select_markers(GtkWidget * widget, gpointer data)
{
    int i ;
    long new_first = -1 ;
    long new_last = -1 ;

    for (i = 0; i < n_markers; i++) {
	if(markers[i] < audio_view.selected_first_sample) {
	    if(new_first == -1)
		new_first = markers[i] ;
	    else
		new_first = MAX(new_first, markers[i]) ;
	}
	if(markers[i] > audio_view.selected_last_sample) {
	    if(new_last == -1)
		new_last = markers[i] ;
	    else
		new_last = MIN(new_last, markers[i]) ;
	}
    }

    if(new_first != -1) audio_view.selected_first_sample = new_first ;
    if(new_last != -1) audio_view.selected_last_sample = new_last ;

    main_redraw(FALSE, TRUE);
}

void toggle_marker_at(long sample)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE)
	&& (audio_playback == FALSE)) {
	int i;
	int j;

	file_processing = TRUE;

	for (i = 0; i < n_markers; i++) {
	    if (markers[i] == sample) {
		n_markers--;
		for (j = i; j < n_markers; j++)
		    markers[j] = markers[j + 1];
		file_processing = FALSE;
		return;
	    }
	}

	if (n_markers < MAX_MARKERS) {
	    markers[n_markers] = sample;
	    n_markers++;
	} else {
	    warning("Maximum number of markers already set");
	}

	file_processing = FALSE;
    }
}

void split_audio_on_markers(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE)
	&& (audio_playback == FALSE)) {
	int i = 0 ;
	int trackno = 1 ;

	long first_sample = 0 ;
	long last_sample = song_markers[0] ;

/*  	for(i = 0 ; i < num_song_markers ; i++) {  */
/*  	    printf("marker %2d at sample %d\n", i, song_markers[i]) ;  */
/*  	}  */
/*    */
/*  	i=0 ;  */
	

	while(last_sample < 10000 && i < num_song_markers) {
	    first_sample = last_sample ;
	    i++ ;
	    last_sample = song_markers[i] ;
	}

	file_processing = TRUE;

	while(last_sample <= prefs.n_samples-1 && i <= num_song_markers) {

	    char filename[100] ;

		// Alister: I'm sure this could be much tidier
		// Not that the first case is really needed
	    if (strlen(file_extension)==0) {
			if(trackno < 10) {
			//Alister: don't just output as xxx.cdda.wav
			//we aren't necessarily working with a .wav file
			//and why would we need the .cdda?
			snprintf(filename,99,"track0%d",trackno) ;
			} else {
			snprintf(filename,99,"track%d",trackno) ;
			}
		}
		else {
			if(trackno < 10) {
			//Alister: don't just output as xxx.cdda.wav
			//we aren't necessarily working with a .wav file
			//and why would we need the .cdda?
			snprintf(filename,99,"track0%d.%s",trackno,file_extension) ;
			} else {
			snprintf(filename,99,"track%d.%s",trackno,file_extension) ;
			}
		}

	    if(last_sample-first_sample >= 10000) {
		save_as_wavfile(filename, first_sample, last_sample) ;
		printf("Save as wavfile %s %ld->%ld\n", filename, first_sample, last_sample) ;
		trackno++ ;
	    }

	    first_sample=last_sample+1 ;
	    i++ ;

	    if(i < num_song_markers) {
		last_sample = song_markers[i] ;
	    } else {
		last_sample = prefs.n_samples-1 ;
	    }

	}

	file_processing = FALSE;
    }
}

void adjust_marker_positions(long pos, long delta)
{
    int i,j;

    i = 0;
    while (i < n_markers) {
        if (markers[i] >= pos) {
            markers[i] += delta;
            if (markers[i] <= pos || markers[i] >= prefs.n_samples) {
                for (j = i; j < n_markers - 1; j++) {
                    markers[j] = markers[j+1];
                }
                n_markers--;
            } else {
                i++;
            }
        } else {
            i++;
        }
    }
}

/* This is a callback function. The data arguments are ignored
* in this example. More on callbacks below. */
void view_all(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE)) {
		stop_all_playback_functions(widget, data);
		file_processing = TRUE;
		audio_view.first_sample = 0;
		audio_view.last_sample = prefs.n_samples - 1;
		set_scroll_bar(prefs.n_samples - 1, audio_view.first_sample,
				   audio_view.last_sample);
		/* set_scroll_bar redraws */
		/*main_redraw(FALSE, TRUE) ; */
		file_processing = FALSE;
	}
}

/* This is a callback function. The data arguments are ignored
* in this example. More on callbacks below. */
void zoom_out(GtkWidget * widget, gpointer data)
{
	if ((file_processing == FALSE) && (file_is_open == TRUE)) {
		stop_all_playback_functions(widget, data);
		long w = audio_view.last_sample - audio_view.first_sample;

		file_processing = TRUE;

		w *= 2;

		audio_view.first_sample -= w / 4;
		if (audio_view.first_sample < 0)
			audio_view.first_sample = 0;
		audio_view.last_sample = audio_view.first_sample + w;
		if (audio_view.last_sample > prefs.n_samples - 1)
			audio_view.last_sample = prefs.n_samples - 1;
		set_scroll_bar(prefs.n_samples - 1, audio_view.first_sample,
				   audio_view.last_sample);
		/* set_scroll_bar redraws */
		/*main_redraw(FALSE, TRUE) ; */
		file_processing = FALSE;
	}
}

/* This is a callback function. The data arguments are ignored
* in this example. More on callbacks below. */
void zoom_in(GtkWidget * widget, gpointer data)
{
	if ((file_processing == FALSE) && (file_is_open == TRUE)) {
		stop_all_playback_functions(widget, data);
		long w = audio_view.last_sample - audio_view.first_sample;
		file_processing = TRUE;

		w /= 2;

		audio_view.first_sample += w / 2;
		if (audio_view.first_sample < 0)
			audio_view.first_sample = 0;
		audio_view.last_sample = audio_view.first_sample + w;
		if (audio_view.last_sample > prefs.n_samples - 1)
			audio_view.last_sample = prefs.n_samples - 1;
		set_scroll_bar(prefs.n_samples - 1, audio_view.first_sample,
				   audio_view.last_sample);
		/* set_scroll_bar redraws */
		/*main_redraw(FALSE, TRUE) ; */
		file_processing = FALSE;
	}
}

void toggle_start_marker(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE)
	&& (audio_playback == FALSE)) {
	long first, last;
	get_region_of_interest(&first, &last, &audio_view);
	toggle_marker_at(first);
	main_redraw(FALSE, TRUE);
    }
}

void toggle_end_marker(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE)
	&& (audio_playback == FALSE)) {
	long first, last;
	get_region_of_interest(&first, &last, &audio_view);
	toggle_marker_at(last);
	main_redraw(FALSE, TRUE);
    }
}

void clear_markers_in_view(GtkWidget * widget, gpointer data)
{

    if ((file_processing == FALSE) && (file_is_open == TRUE)
	&& (audio_playback == FALSE)) {
	long first, last;
	int i;


	get_region_of_interest(&first, &last, &audio_view);

	for (i = 0; i < n_markers; i++) {
	    if (markers[i] >= first && markers[i] <= last) {
		toggle_marker_at(markers[i]);
		i--;		/* current marker deleted, next marker dropped into position i */
	    }
	}
	main_redraw(FALSE, TRUE);
    }
}

long msec_in_region_of_interest()
{
	long first, last;
	get_region_of_interest(&first, &last, &audio_view);
	return ((last-first)*1000)/prefs.rate ;
}

gboolean  key_press_cb(GtkWidget * widget, GdkEventKey * event, gpointer data)
{
    extern double spectral_amp;
    gboolean handled = TRUE ;

/*      g_print("key_press_cb\n") ;  */

    /* GDK_b, GDK_c, GDK_e, GDK_n, GDK_z used through menus */
    switch (event->keyval) {
	case GDK_space:
	    if (audio_playback == FALSE)
		start_gwc_playback(widget, data);
	    else {
		stop_all_playback_functions(widget, data);
	    }
	    break;
	case GDK_l:
	    if (audio_playback == FALSE) {
		if(msec_in_region_of_interest() < 250){
		    warning("Must select > 0.25 seconds of audio to loop") ;
		} else {
		    start_gwc_playback(widget, data);
		    audio_is_looping = TRUE ;
		}
	    } else {
		audio_is_looping = FALSE ;
		stop_all_playback_functions(widget, data);
	    }
	    break;
	case GDK_s:
	    if (audio_playback == TRUE) {
		set_playback_cursor_position(&audio_view,
					     prev_cursor_millisec);
		gtk_timeout_remove(cursor_timer);
		stop_all_playback_functions(widget, data);
		audio_view.selected_last_sample = audio_view.cursor_position;
		audio_view.selected_first_sample =
		    audio_view.selected_last_sample -
		    prefs.rate * stop_key_highlight_interval;
		if (audio_view.selected_first_sample < 0)
		    audio_view.selected_first_sample = 0;
		audio_view.selection_region = TRUE;
		main_redraw(FALSE, TRUE);
	    }
	    break;
	case GDK_3:
	    if (audio_playback == FALSE) {
		/* go foward by one revolution of a 33 1/3 rpm record */
		int sample_shift = prefs.rate * 60.0 / 33.333333333 ;
		//printf("shift is %d samples\n", sample_shift) ;
		if ( sample_shift + audio_view.last_sample < prefs.n_samples - 1) {
		    audio_view.last_sample += sample_shift ;
		    audio_view.first_sample += sample_shift ;
		    set_scroll_bar(prefs.n_samples - 1, audio_view.first_sample,
		       audio_view.last_sample);
		}
	    }
	    break;
	case GDK_2:
	    if (audio_playback == FALSE) {
		/* go backward by one revolution of a 33 1/3 rpm record */
		int sample_shift = prefs.rate * 60.0 / 33.333333333 ;
		if (sample_shift < audio_view.first_sample) {
		    audio_view.last_sample -= sample_shift ;
		    audio_view.first_sample -= sample_shift ;
		    set_scroll_bar(prefs.n_samples - 1, audio_view.first_sample,
		       audio_view.last_sample);
		}
	    }
	    break;
	case GDK_d:
	    scale_down_callback(widget, data);
	    break;
	case GDK_j:
	    spectral_amp *= 1.7;
	    main_redraw(FALSE, TRUE);
	    break;
	case GDK_k:
	    spectral_amp /= 1.7;
	    main_redraw(FALSE, TRUE);
	    break;
	case GDK_r:
	    scale_reset_callback(widget, data);
	    break;
	case GDK_u:
	    scale_up_callback(widget, data);
	    break;
	case GDK_a:
	    if (event->state & GDK_MOD1_MASK) /* Alt-a */
		manual_declick(NULL, NULL);
	    else
		append_cdrdao(&audio_view);
	    break;
	default:
	    handled = FALSE ;
	    break ;
    }

    return handled ;
}


int cleanup_and_close(struct view *v, struct sound_prefs *p)
{
    stop_playback(1);

    if(batch_mode == 0 && file_is_open && get_undo_levels() > 0) {
	int r = yesnocancel("Keep changes to the audio file?") ;

	if(r == 2)
	    return 0 ;

	if(r == 1) {
	    fprintf(stderr, "Undoing all changes\n") ;
	    int _p = get_undo_levels() ;
	    while(undo(v, p) > 0) 
	    {
		if ( get_undo_levels() == _p )
		return 0 ;
		_p -- ;
		}
		;
	}
    }

    if (file_is_open)
	save_sample_block_data(&prefs);

    if (close_wavefile(&audio_view)) {
	window_maximised = gdk_window_get_state(gtk_widget_get_window(main_window)) & GDK_WINDOW_STATE_MAXIMIZED;
	if(window_maximised == FALSE)
	{
		gtk_window_get_position(GTK_WINDOW(main_window), &window_x, &window_y); 
		gtk_window_get_size(GTK_WINDOW(main_window), &window_width, &window_height);
	}
	save_preferences();
	gtk_accel_map_save( g_build_filename (g_get_user_config_dir (), APPNAME, ACCELERATORS_FILE, NULL) );
	undo_purge();
    }

    return 1 ;
}


gint delete_event(GtkWidget * widget, GdkEvent * event, gpointer data)
{
	if( file_processing == TRUE )
	{
		warning("Can't quit while file is being processed.");
		return TRUE;
	}
	if( batch_mode == TRUE )
	{
		warning("Can't quit while in batch mode. (Will automatically close.)");
		return TRUE;
	}
	if(!cleanup_and_close(&audio_view, &prefs)) 
		return TRUE;
	rmdir(tmpdir);
	gtk_main_quit();
	return FALSE;
		
}

/* I believe I have fixed the crash when clicking outside the about dialog.
I'm guessing opening the website link requires gvfs and a working dbus session.
*/
void about(GtkWidget *window)
{
    const gchar *authors[] = { "Jeffrey J. Welty", "James Tappin", "Ian Leonard", "Bill Jetzer", "Charles Morgon", "Frank Freudenberg", "Thiemo Gehrke", "Rob Frohne", "Alister Hood", NULL };
    const gchar* LICENSE="This program is free software; you can redistribute it and/or "
			 "modify it under the terms of the GNU General Public License "
			 "as published by the Free Software Foundation; either version 2 "
			 "of the License, or (at your option) any later version.\n\n"
			 "This program is distributed in the hope that it will be useful, "
			 "but WITHOUT ANY WARRANTY; without even the implied warranty of "
			 "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
			 "GNU General Public License for more details.\n"
			 "You should have received a copy of the GNU General Public License "
			 "along with this program; if not, write to the Free Software "
			 "Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.";

    gtk_show_about_dialog(GTK_WINDOW( NULL ),
				    "version", VERSION,
				    "copyright", "Copyright 2001, 2002, 2003, 2004, 2005 Redhawk.org",
				    "license", LICENSE,
				    "wrap-license", TRUE,
				    "website", "http://gwc.sourceforge.net/",
				    "comments", "Removes noise (hiss, pops and clicks) from audio files in WAV and similar formats.\ne.g recordings of scratchy vinyl records.",
				    "authors", authors,
				    "logo-icon-name", APPNAME,
				    NULL);
	// Note I think we'd have to refactor a bit to make this dialog modal in GTK2
}

void main_redraw(int cursor_flag, int redraw_data)
{
    if (doing_progressbar_update == TRUE)
	return;

    if (file_is_open == TRUE)
	redraw(&audio_view, &prefs, audio_drawing_area, cursor_flag,
	       redraw_data, spectral_view_flag);

    if (file_is_open == TRUE && cursor_flag == FALSE)
	display_times();
}

void display_sonogram(GtkWidget * widget, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE)
	&& (audio_playback == FALSE)) {
	file_processing = TRUE;
	spectral_view_flag = !spectral_view_flag;
	main_redraw(FALSE, TRUE);
	file_processing = FALSE;
    }
}

void open_wave_filename(void)
{
    char tmp[PATH_MAX+1];
    struct sound_prefs tmp_prefs;

    gnome_flush();

       /* Close writes to wave_filename so put back the name of the
	  current open file */
       // Note that the current file is closed (but still visible) even if opening the new file fails!
    strcpy(tmp,wave_filename);
    strcpy(wave_filename, last_filename);

    if(!cleanup_and_close(&audio_view, &prefs)) 
	return ;

    strcpy(wave_filename,tmp);

       /* all this to store the last directory where a file was opened */
       // Eh?  It was storing the full path, but mangling it if the file was in /
/*    strcpy(tmp, wave_filename);
    strcpy(pathname, dirname(tmp));
    strcat(pathname, "/");

    strcpy(tmp, wave_filename);
    strcat(pathname, basename(tmp));
*/
    // This seems to do the job
    // But note that it still stores the file location even if it isn't able to be opened (e.g. because it isn't a wav file).  Is this intended?
    strcpy(pathname, tmp);
    
    // Should also really reject wav files that we can't work with, or fix it so they do work! 
    if (is_valid_audio_file(wave_filename)) {
	tmp_prefs = open_wavefile((char *) wave_filename, &audio_view);
	if (tmp_prefs.successful_open) {
	    prefs = tmp_prefs;
	    spectral_view_flag = FALSE;
	    if (prefs.wavefile_fd != -1) {
#ifdef TRUNCATE_OLD
		audio_view.truncate_head = 0;
		audio_view.truncate_tail = (prefs.n_samples - 1);
#endif /* TRUNCATE_OLD */
		audio_view.n_samples = prefs.n_samples;
		if (audio_view.first_sample == -1) {
		    audio_view.first_sample = 0;
		    audio_view.last_sample = (prefs.n_samples - 1);
		} else {
		    audio_view.first_sample =
			MIN(prefs.n_samples - 1,
			    audio_view.first_sample);
		    audio_view.first_sample =
			MAX(0, audio_view.first_sample);
		    audio_view.last_sample =
			MIN(prefs.n_samples - 1,
			    audio_view.last_sample);
		    audio_view.last_sample =
			MAX(0, audio_view.last_sample);
		}
		audio_view.selection_region = FALSE;
	    // If we don't do this and we had another file open with just left or right channel selected, only that channel will be cleaned
	    // But that is very unexpected!
	    audio_view.channel_selection_mask = 3 ;
		file_is_open = TRUE;
		fill_sample_buffer(&prefs);

		/* display entire file data if this file changed since last edit session */
		if (strcmp(wave_filename, last_filename)) {
		    audio_view.first_sample = 0;
		    audio_view.last_sample = prefs.n_samples - 1;
		    strcpy(last_filename, wave_filename);
		    num_song_markers = 0;
		}

		set_scroll_bar(prefs.n_samples - 1,
			       audio_view.first_sample,
			       audio_view.last_sample);
		main_redraw(FALSE, TRUE);
		
		// get the file extension, so we know what to call files we export
		// note that for a hidden file with no extension e.g. ~/.name
		// name will be used as the extension.
		// this is obviously wrong, but actually kind of cool as it allows you to 
		// split more than one file into tracks without overwriting outputs or restarting gwc from a different working directory.
		// i.e. track01.name1, track02.name1, then track01.name2, track02.name2
		gchar *name = basename(wave_filename) ;
		file_extension = "";
		while (*name)
		{
		  if (*name++ == '.')
		  file_extension = name;
		}
		
	    } else {
		file_is_open = FALSE;
		warning("failed to open audio file");
	    }
	} else {
	    strcpy(wave_filename, last_filename);
	}
    } else {
	file_is_open = FALSE;
	// I think we can do away with this extra warning now.
	//warning("No file selected, or file format not recognized");
    }
}

void old_open_wave_filename(void)
{
    char tmp[PATH_MAX+1];

    gnome_flush();

    /* all this to store the last directory where a file was opened */
    strcpy(tmp, wave_filename);
    strcpy(pathname, dirname(tmp));
    strcat(pathname, "/");

    strcpy(tmp, wave_filename);
    strcat(pathname, basename(tmp));

    {
	struct sound_prefs tmp_prefs;

	cleanup_and_close(&audio_view, &prefs);

	if (is_valid_audio_file(wave_filename)) {
	    tmp_prefs = open_wavefile((char *) wave_filename, &audio_view);
	    if (tmp_prefs.successful_open) {
		prefs = tmp_prefs;
		spectral_view_flag = FALSE;
		if (prefs.wavefile_fd != -1) {
#ifdef TRUNCATE_OLD
		    audio_view.truncate_head = 0;
		    audio_view.truncate_tail = (prefs.n_samples - 1);
#endif /* TRUNCATE_OLD */
		    audio_view.n_samples = prefs.n_samples;
		    if (audio_view.first_sample == -1) {
			audio_view.first_sample = 0;
			audio_view.last_sample = (prefs.n_samples - 1);
		    } else {
			audio_view.first_sample =
			    MIN(prefs.n_samples - 1,
				audio_view.first_sample);
			audio_view.first_sample =
			    MAX(0, audio_view.first_sample);
			audio_view.last_sample =
			    MIN(prefs.n_samples - 1,
				audio_view.last_sample);
			audio_view.last_sample =
			    MAX(0, audio_view.last_sample);
		    }
		    audio_view.selection_region = FALSE;
		    file_is_open = TRUE;
		    fill_sample_buffer(&prefs);

		    /* display entire file data if this file changed since last edit session */
		    if (strcmp(wave_filename, last_filename)) {
			audio_view.first_sample = 0;
			audio_view.last_sample = prefs.n_samples - 1;
			strcpy(last_filename, wave_filename);
			num_song_markers = 0;
		    }

		    set_scroll_bar(prefs.n_samples - 1,
				   audio_view.first_sample,
				   audio_view.last_sample);
		    main_redraw(FALSE, TRUE);
		} else {
		    file_is_open = FALSE;
		    warning("failed to open audio file");
		}
	    } else {
		strcpy(wave_filename, last_filename);
	    }
	} else {
	    file_is_open = FALSE;
	    warning("No file selected, or file format not recognized");
	}
    }
}

static void drag_data_received(GtkWidget *widget, GdkDragContext *d, gint32 i, gint32 j, GtkSelectionData *s, guint32 x, guint32 y)
{
    gchar **uris = gtk_selection_data_get_uris(s) ;
    gchar *filename = g_filename_from_uri(uris[0], NULL, NULL) ; // use a separate variable so we don't overwrite wave_filename
			    // e.g. if drag-n-dropping a broken symlink.  Consistent with opening from the command line.
			    // Inconsistent with other failures e.g. a file format we don't understand.
			    // Should review if this is a good idea or not
    if ( access (filename, F_OK) == 0 ) {
        strcpy(wave_filename, filename) ;
        open_wave_filename() ;
    }
    else
		warning(strcat(filename, ": File not found"));
    g_strfreev(uris) ;
    g_free(filename) ;
}

#ifdef MAC_OS_X
void app_open_file_cb (GtkosxApplication *theApp, gchar *path, gpointer p)
{
    strcpy(wave_filename, path);
    open_wave_filename();
}
#endif

void store_selection_filename(gpointer user_data)
{
    if (strcmp(save_selection_filename, wave_filename)) {
	save_selection_as_wavfile(save_selection_filename, &audio_view);
    } else {
	warning("Cannot save selection over the currently open file!");
    }
}

void open_file_selection(GtkWidget * widget, gpointer data)
{
    GtkFileFilter * ff, * ffa;

    if ((file_processing == FALSE) && (audio_playback == FALSE))
    {

	/* Create the selector */
	file_selector =
	    gtk_file_chooser_dialog_new("Please select a file for editing.",
                                        GTK_WINDOW(main_window),
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                        GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                        NULL);

	gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(file_selector),
				      pathname);

	ff = gtk_file_filter_new();
	gtk_file_filter_set_name(ff,"Known extensions");
	// I think we could borrow code from mhwaveedit which gets the list of formats from libsndfile, 
	// but I don't think it would tell us all the extensions, and we'd need to make it only tell us
	// formats which support rdwr
	//
	// We could also borrow the dialog from mhwaveedit which allows you to open a raw audio file,
	// And it would also be good to be able to load raw audio specifying its format from the command line
	gtk_file_filter_add_pattern(ff,"*.wav");
	gtk_file_filter_add_pattern(ff,"*.wavpcm");
	gtk_file_filter_add_pattern(ff,"*.aiff");
	gtk_file_filter_add_pattern(ff,"*.aif");
	// when I try to create this format it actually seems the same as aiff, so not sure if we really do support it.
	gtk_file_filter_add_pattern(ff,"*.aiffc");
	gtk_file_filter_add_pattern(ff,"*.aifc");
	gtk_file_filter_add_pattern(ff,"*.au");
	gtk_file_filter_add_pattern(ff,"*.snd");
	gtk_file_filter_add_pattern(ff,"*.avr");
	gtk_file_filter_add_pattern(ff,"*.caf");
	gtk_file_filter_add_pattern(ff,"*.htk");
	gtk_file_filter_add_pattern(ff,"*.iff");
	gtk_file_filter_add_pattern(ff,"*.mat");
	gtk_file_filter_add_pattern(ff,"*.mat4");
	gtk_file_filter_add_pattern(ff,"*.mat5");
	gtk_file_filter_add_pattern(ff,"*.mpc");
	gtk_file_filter_add_pattern(ff,"*.paf");
	gtk_file_filter_add_pattern(ff,"*.pvf");
	gtk_file_filter_add_pattern(ff,"*.rf64");
	gtk_file_filter_add_pattern(ff,"*.sd2");
	gtk_file_filter_add_pattern(ff,"*.sds");
	gtk_file_filter_add_pattern(ff,"*.sf");
	gtk_file_filter_add_pattern(ff,"*.voc");
	gtk_file_filter_add_pattern(ff,"*.w64");
	gtk_file_filter_add_pattern(ff,"*.wve");
	gtk_file_filter_add_pattern(ff,"*.WAV");
	gtk_file_filter_add_pattern(ff,"*.WAVPCM");
	gtk_file_filter_add_pattern(ff,"*.AIFF");
	gtk_file_filter_add_pattern(ff,"*.AIF");
	gtk_file_filter_add_pattern(ff,"*.AIFFC");
	gtk_file_filter_add_pattern(ff,"*.AIFC");
	gtk_file_filter_add_pattern(ff,"*.AU");
	gtk_file_filter_add_pattern(ff,"*.SND");
	gtk_file_filter_add_pattern(ff,"*.AVR");
	gtk_file_filter_add_pattern(ff,"*.CAF");
	gtk_file_filter_add_pattern(ff,"*.HTK");
	gtk_file_filter_add_pattern(ff,"*.IFF");
	gtk_file_filter_add_pattern(ff,"*.MAT");
	gtk_file_filter_add_pattern(ff,"*.MAT4");
	gtk_file_filter_add_pattern(ff,"*.MAT5");
	gtk_file_filter_add_pattern(ff,"*.MPC");
	// note that this format seems to be broken when written by ffmpeg, but mhwaveedit is OK
	gtk_file_filter_add_pattern(ff,"*.PAF");
	gtk_file_filter_add_pattern(ff,"*.PVF");
	gtk_file_filter_add_pattern(ff,"*.RF64");
	gtk_file_filter_add_pattern(ff,"*.SD2");
	gtk_file_filter_add_pattern(ff,"*.SDS");
	gtk_file_filter_add_pattern(ff,"*.SF");
	// note that this format seems to be broken when written by ffmpeg, but mhwaveedit is OK
	gtk_file_filter_add_pattern(ff,"*.VOC");
	gtk_file_filter_add_pattern(ff,"*.W64");
	// note that this format seems to be broken when written by mhwaveedit, but sox is OK
	gtk_file_filter_add_pattern(ff,"*.WVE");
	// note libsndfile also supports reading and writing FLAC & ALAC, but does not support opening them in RDWR mode
	// and it supports reading and writing OGG, but I wasn't able to generate an OGG that it supported!
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(file_selector),ff);
  
	// Not sure why, but after switching to show all files, when next opened the dialog doesn't
	// show which filter is selected ???
	// But if we put this code first it is ALWAYS selected
	// ???
	ffa = gtk_file_filter_new();
	gtk_file_filter_set_name(ffa,"All files");
	gtk_file_filter_add_pattern(ffa,"*");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(file_selector),ffa);

	/* Display the dialog */
	if (gtk_dialog_run (GTK_DIALOG (file_selector)) == GTK_RESPONSE_ACCEPT)
        {
			    gchar filename[PATH_MAX+1]; // use a separate variable so we don't overwrite wave_filename
			    // e.g. if opening a broken symlink.  Consistent with opening from the command line.
			    // Inconsistent with other failures e.g. a file format we don't understand.
			    // Should review if this is a good idea or not
                strncpy(filename,
	                gtk_file_chooser_get_filename(GTK_FILE_CHOOSER
						      (file_selector)), PATH_MAX);
                gtk_widget_destroy (GTK_WIDGET (file_selector));
        		if ( access (filename, F_OK) == 0 ) {
		        	strcpy(wave_filename, filename);
		        	open_wave_filename();
		        }
		        else
		        	warning(strcat(filename, ": File not found"));
        }
    else {
        gtk_widget_destroy (GTK_WIDGET (file_selector));
	}
    }
}

void save_selection_as_encoded(int fmt, char *filename, char *filename_new, struct view *v, char *trackname)
{
    long total_samples;
    total_samples = v->selected_last_sample - v->selected_first_sample + 1;

    if (total_samples < 0 || total_samples > v->n_samples) {
	warning("Invalid selection");
	return;
    }

    /* save part of file as encoded fmt - fmt,old file, new file, start sample, number of samples */
    encode(fmt, filename, filename_new, v->selected_first_sample,
	   total_samples, trackname);
}

void store_selected_filename_as_encoded(gpointer user_data)
{
    int enc_format = NULL_FMT ;
    char trackname[1024] = "" ;

    if (encoding_type == GWC_OGG) enc_format = OGG_FMT ;
    if (encoding_type == GWC_MP3) enc_format = MP3_FMT ;
    if (encoding_type == GWC_MP3_SIMPLE) enc_format = MP3_SIMPLE_FMT ;

    if(!prompt_user("Enter the trackname:", trackname, 1023)) {
	file_processing = TRUE;

	save_selection_as_encoded(enc_format, wave_filename,
				  save_selection_filename,
				  &audio_view, trackname);
	file_processing = FALSE;
    }
}

void remove_extension(char* s) {
  char* dot = 0;
  while (*s) {
    if (*s == '.') dot = s;  // last dot
    else if (*s == '/' || *s == '\\') dot = 0;  // ignore dots before path separators
    s++;
  }
  if (dot) *dot = '\0';
}

void save_as_encoded()
{
    GtkFileFilter * ff, * ffa;

    if ((file_processing == FALSE) && (file_is_open == TRUE)
	&& (audio_playback == FALSE)) {

	char tmppath[PATH_MAX+6];

	if (audio_view.selection_region == TRUE) {
	    strcpy(tmppath, pathname);

	    /* Create the selector */
	    file_selector = gtk_file_chooser_dialog_new("Encode to filename:",
                                                        GTK_WINDOW(main_window),
                                                        GTK_FILE_CHOOSER_ACTION_SAVE,
                                                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                                        GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                                        NULL);

	    ff = gtk_file_filter_new();
	    ffa = gtk_file_filter_new();
	    gtk_file_filter_set_name(ffa,"All files");
	    gtk_file_filter_add_pattern(ffa,"*");

	    if (encoding_type == GWC_OGG) {
		/* make it a .ogg extension */
		//The corner case where this code might produce unexpected results is if you are working on hidden files without extensions e.g. "~/.recording1"
		//But since we are choosing a name in the dialog that is ok.
		remove_extension(tmppath) ;
		strcat(tmppath, ".ogg");
		gtk_file_filter_set_name(ff,"OGG files");
		gtk_file_filter_add_pattern(ff,"*.ogg");
		gtk_file_filter_add_pattern(ff,"*.OGG");
	    } else {
		/* make it a .mp3 extension */
		//The corner case where this code might produce unexpected results is if you are working on hidden files without extensions e.g. "~/.recording1"
		//But since we are choosing a name in the dialog that is ok.
		remove_extension(tmppath) ;
		strcat(tmppath, ".mp3");
		gtk_file_filter_set_name(ff,"MP3 files");
		gtk_file_filter_add_pattern(ff,"*.mp3");
		gtk_file_filter_add_pattern(ff,"*.MP3");
	    }

	    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(file_selector),ff);
	    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(file_selector),ffa);

	    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER
					    (file_selector), basename(tmppath));
	    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER
					    (file_selector), dirname(tmppath));
	    gtk_file_chooser_set_do_overwrite_confirmation
            		                    (GTK_FILE_CHOOSER(file_selector), TRUE);

	    /* Display the dialog */
	    if (gtk_dialog_run (GTK_DIALOG (file_selector)) == GTK_RESPONSE_ACCEPT)
            {
                    strncpy(save_selection_filename,
	                    gtk_file_chooser_get_filename(GTK_FILE_CHOOSER
	    					      (file_selector)), PATH_MAX);
	            gtk_widget_destroy (GTK_WIDGET (file_selector));
                    store_selected_filename_as_encoded(wave_filename);
            } else {
            gtk_widget_destroy (GTK_WIDGET (file_selector));
	    }

	} else {
	    info("Please highlight a region to save first");
	}

    }
}

void save_as_ogg_selection(GtkWidget * widget, gpointer data)
{
    encoding_type = GWC_OGG;
    save_as_encoded();
}

void save_as_mp3_selection(GtkWidget * widget, gpointer data)
{
    encoding_type = GWC_MP3;
    save_as_encoded();
}

void save_as_mp3_simple_selection(GtkWidget * widget, gpointer data)
{
    encoding_type = GWC_MP3_SIMPLE;
    save_as_encoded();
}

void save_as_selection(GtkWidget * widget, gpointer data)
{
 	/* Alister - disable, because we aren't necessarily saving a wav file
 	   GtkFileFilter * ff, * ffa;*/

    if ((file_processing == FALSE) && (file_is_open == TRUE)
	&& (audio_playback == FALSE)) {

	if (audio_view.selection_region == TRUE) {

	    /* Create the selector */
	    file_selector =
		gtk_file_chooser_dialog_new("Filename to save selection to:",
                                            GTK_WINDOW(main_window),
                                            GTK_FILE_CHOOSER_ACTION_SAVE,
                                            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                            GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                            NULL);

	    gtk_file_chooser_set_filename(GTK_FILE_CHOOSER
					    (file_selector), pathname);
	    gtk_file_chooser_set_do_overwrite_confirmation
            		                    (GTK_FILE_CHOOSER(file_selector), TRUE);

	/* Alister - disable, because we aren't necessarily saving a wav file
	ff = gtk_file_filter_new();
	gtk_file_filter_set_name(ff,"Wave files");
	gtk_file_filter_add_pattern(ff,"*.wav");
	gtk_file_filter_add_pattern(ff,"*.WAV");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(file_selector),ff);
  
	ffa = gtk_file_filter_new();
	gtk_file_filter_set_name(ffa,"All files");
	gtk_file_filter_add_pattern(ffa,"*");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(file_selector),ffa);*/

	    if (gtk_dialog_run (GTK_DIALOG (file_selector)) == GTK_RESPONSE_ACCEPT)
            {
                    strncpy(save_selection_filename,
	                    gtk_file_chooser_get_filename(GTK_FILE_CHOOSER
	    					      (file_selector)), PATH_MAX);
            	    gtk_widget_destroy (GTK_WIDGET (file_selector));
                    store_selection_filename(wave_filename);
            } else {
            gtk_widget_destroy (GTK_WIDGET (file_selector));
            }
	} else {
	    info("Please highlight a region to save first");
	}
    }
}

static struct {
	gchar *stockid;
	const char **icon_xpm;
} stock_icons[] = {
	{"filter_icon", filter_xpm },
	{"pinknoise_icon", pinknoise_xpm },
	{"amplify_icon", amplify_xpm },
	{"declick_icon", declick_xpm },
	{"gwc_icon", gtk_wave_cleaner_xpm },
	{"declick_w_icon", declick_w_xpm },
	{"declick_m_icon", declick_m_xpm },
	{"decrackle_icon", decrackle_xpm },
	{"estimate_icon", estimate_xpm },
	{"noise_sample_icon", noise_sample_xpm },
	{"remove_noise_icon", remove_noise_xpm },
	{"silence_icon", silence_xpm },
	{"zoom_sel_icon", zoom_sel_xpm },
	{"zoom_in_icon", zoom_in_xpm },
	{"zoom_out_icon", zoom_out_xpm },
	{"view_all_icon", view_all_xpm },
	{"select_all_icon", select_all_xpm },
	{"spectral_icon", spectral_xpm },
	{"start_icon", start_xpm },
	{"stop_icon", stop_xpm }
};

static gint n_stock_icons = G_N_ELEMENTS (stock_icons);

static void
register_stock_icons (void)
{
	GtkIconFactory *icon_factory;
	GtkIconSet *icon_set;
	GdkPixbuf *pixbuf;
	gint i;

	icon_factory = gtk_icon_factory_new ();

	for (i = 0; i < n_stock_icons; i++)
	{
		pixbuf = gdk_pixbuf_new_from_xpm_data(stock_icons[i].icon_xpm);
		icon_set = gtk_icon_set_new_from_pixbuf (pixbuf);
		g_object_unref(pixbuf);
		gtk_icon_factory_add (icon_factory, stock_icons[i].stockid, icon_set);
		gtk_icon_set_unref (icon_set);
	}

	gtk_icon_factory_add_default(icon_factory);

	g_object_unref(icon_factory);
}

/* Normal items */
static const GtkActionEntry entries[] = {
  { "FileMenu", NULL, "_File" },
  { "Open", GTK_STOCK_OPEN, "_Open...", "<control>O", "Open a file", G_CALLBACK(open_file_selection) },
  { "SaveSelection", NULL, "Save selection as...", "<control>S", "Save the current selection to a new wavfile", G_CALLBACK(save_as_selection) },
  { "SaveSimple", NULL, "Simple encode selection as MP3...", NULL, "Save the current selection to an MP3 encoded file, simple options", G_CALLBACK(save_as_mp3_simple_selection) },
  { "SaveMP3", NULL, "Encode selection as MP3...", NULL, "Save the current selection to an MP3 encoded file", G_CALLBACK(save_as_mp3_selection) },
  { "SaveOGG", NULL, "Encode selection as OGG/Vorbis...", NULL, "Save the current selection to an OGG Vorbis encoded file", G_CALLBACK(save_as_ogg_selection) },
  { "SaveCDRDAO", NULL, "Create cdrdao TOC file...", NULL, "Create a cdrdao table of contents file for marked songs", G_CALLBACK(save_cdrdao_tocs) },
  { "SaveMarkers", NULL, "Create cdrdao TOC file, using marker pairs...", NULL, "Create a cdrtao table of contents file for marked songs, using pairs of song markers", G_CALLBACK(save_cdrdao_tocp) },
  { "SaveSplit", NULL, "Split audio on song markers", NULL, "Create individual track files", G_CALLBACK(split_audio_on_markers) },
  { "Quit", GTK_STOCK_QUIT, "Q_uit", "<control>Q", "Close GWC", G_CALLBACK(delete_event) },
  { "EditMenu", NULL, "_Edit" },
  { "Undo", GTK_STOCK_UNDO, "Undo", "<control>Z", "Undo the last edit", G_CALLBACK(undo_callback) },
  { "Filter", "filter_icon", "Apply DSP Frequency Filters", NULL, "lowpass, highpass, notch or bandpass biquad filtering", G_CALLBACK(filter_cb) },
  { "PinkNoise", "pinknoise_icon", "Generate Pink Noise", NULL, "Replace current view or selection with pink noise", G_CALLBACK(pinknoise_cb) },
  { "Amplify", "amplify_icon", "Amplify", NULL, "Amplify or attenuate the current view or selection", G_CALLBACK(amplify) },
  { "DeclickStrong", "declick_icon", "Declick Strong", NULL, "Automatically find and repair pops/clicks in current view or selection", G_CALLBACK(declick) },
  { "DeclickWeak", "declick_w_icon", "Declick Weak", NULL, "Automatically find and repair weaker pops/clicks in current view or selection", G_CALLBACK(declick_weak) },
  { "DeclickManual", "declick_m_icon", "Declick Manual", NULL, "Apply LSAR signal estimation to repair a single selected/viewed click", G_CALLBACK(manual_declick) },
  { "Decrackle", "decrackle_icon", "Decrackle", "C", "Remove crackle from old, deteriorated vinyl", G_CALLBACK(decrackle) },
  { "Estimate", "estimate_icon", "Estimate", NULL, "Estimate signal (> 300 samples) in current view or selection", G_CALLBACK(estimate) },
  { "Sample", "noise_sample_icon", "Sample", NULL, "Use current view or selection as a noise sample", G_CALLBACK(noise_sample) },
  { "Denoise", "remove_noise_icon", "Denoise", NULL, "Remove noise from current view or selection", G_CALLBACK(remove_noise) },
  { "Silence", "silence_icon", "Silence", NULL, "Insert silence with size of current selection", G_CALLBACK(silence_callback) },
  { "Reverb", NULL, "Reverb", NULL, "Apply reverberation to the current view or selection", G_CALLBACK(reverb) },
  { "Cut", GTK_STOCK_CUT, "Cut", NULL, "Cut current selection to internal clipboard", G_CALLBACK(cut_callback) },
  { "Copy", GTK_STOCK_COPY, "Copy", NULL, "Copy current selection to internal clipboard", G_CALLBACK(copy_callback) },
  { "Paste", GTK_STOCK_PASTE, "Paste", NULL, "Insert internal clipboard before current selection", G_CALLBACK(paste_callback) },
  { "Delete", GTK_STOCK_DELETE, "Delete", NULL, "Delete current selection from audio data", G_CALLBACK(delete_callback) },
  { "ViewMenu", NULL, "_View" },
  { "ZoomSelect", "zoom_sel_icon", "Zoom to selection", NULL, "Zoom in on selected portion", G_CALLBACK(zoom_select) },
  { "ZoomIn", "zoom_in_icon", "Zoom in", NULL, "Zoom in", G_CALLBACK(zoom_in) },
  { "ZoomOut", "zoom_out_icon", "Zoom out", NULL, "Zoom out", G_CALLBACK(zoom_out) },
  { "ViewAll", "view_all_icon", "View all", NULL, "View entire audio file", G_CALLBACK(view_all) },
  { "SelectAll", "select_all_icon", "Select current view", "<control>A", "Select everything visible in the window", G_CALLBACK(select_all) },
  { "SelectNone", NULL, "Select none", "Escape", "Clear the selection", G_CALLBACK(select_none) },
  { "Spectral", "spectral_icon", "Spectral view", NULL, "Toggle between waveform view and spectrogram", G_CALLBACK(display_sonogram) },
  { "MarkersMenu", NULL, "_Markers" },
  { "ToggleBegin", NULL, "Toggle beginning marker", "B", "Add or remove an edit marker at the beginning of the highlighted selection (or current view)", G_CALLBACK(toggle_start_marker) },
  { "ToggleEnd", NULL, "Toggle ending marker", "E", "Add or remove an edit marker at the end of the highlighted selection (or current view)", G_CALLBACK(toggle_end_marker) },
  { "ClearMarkers", NULL, "Clear markers", NULL, "Remove all edit markers in the highlighted selection (or current view)", G_CALLBACK(clear_markers_in_view) },
  { "ExpandSelection", NULL, "Expand selection to nearest markers", "M", "Select the region between two edit markers", G_CALLBACK(select_markers) },
  { "MarkSongs", NULL, "Detect songs", NULL, "Mark all songs in the file using silence detection", G_CALLBACK(mark_songs) },
  { "MoveMarker", NULL, "Move song marker", NULL, "Move the closest song marker to the beginning of the current (or most recent) selection", G_CALLBACK(move_song_marker) },
  { "AddMarker", NULL, "Add song marker", NULL, "Add a song marker at the beginning of the current (or most recent) selection", G_CALLBACK(add_song_marker) },
  { "AddMarkerPair", NULL, "Add song marker pair", NULL, "Add a song marker at the beginning AND end of the current (or most recent) selection", G_CALLBACK(add_song_marker_pair) },
  { "DeleteMarker", NULL, "Clear song markers", NULL, "Remove all song markers in the current (or most recent) selection", G_CALLBACK(delete_song_marker) },
  { "NextMarker", NULL, "Next song marker", "N", "Select 15 seconds of audio around the next song marker", G_CALLBACK(select_song_marker) },
  { "SettingsMenu", NULL, "_Settings" },
  { "DeclickPrefs", NULL, "Declick", NULL, "Set declick sensitivity, iteration", G_CALLBACK(declick_set_preferences) },
  { "DecracklePrefs", NULL, "Decrackle", NULL, "Set decrackle sensitivity", G_CALLBACK(decrackle_set_preferences) },
  { "DenoisePrefs", NULL, "Denoise", NULL, "Set denoise parameters", G_CALLBACK(denoise_set_preferences) },
  { "MP3SimplePrefs", NULL, "MP3 simple encoding", NULL, "Set MP3 Simple Encoding parameters", G_CALLBACK(set_mp3_simple_encoding_preferences) },
  { "MP3Prefs", NULL, "MP3 encoding", NULL, "Set MP3 Encoding parameters", G_CALLBACK(set_mp3_encoding_preferences) },
  { "OggPrefs", NULL, "Ogg encoding", NULL, "Set Ogg Encoding parameters", G_CALLBACK(set_ogg_encoding_preferences) },
  { "MiscPrefs", NULL, "Miscellaneous", NULL, "Miscellaneous parameters", G_CALLBACK(set_misc_preferences) },
  { "HelpMenu", NULL, "_Help" },
  { "Help", GTK_STOCK_HELP, "User Guide", NULL, "Instructions for using GWC", G_CALLBACK(help) },
  { "About", GTK_STOCK_ABOUT, "About", NULL, "About GWC", G_CALLBACK(about) },
  { "PlayAudio", "start_icon", "Start audio playback", NULL, "Playback the selected or current view of audio", G_CALLBACK(start_gwc_playback) },
  { "StopAudio", "stop_icon", "Stop", NULL, "Stop audio playback", G_CALLBACK(stop_all_playback_functions) }
};

static const char *ui_description =
"<ui>"
"  <menubar name='MainMenu'>"
"    <menu action='FileMenu'>"
"      <menuitem action='Open'/>"
"      <menuitem action='SaveSelection'/>"
"      <menuitem action='SaveSimple'/>"
"      <menuitem action='SaveMP3'/>"
"      <menuitem action='SaveOGG'/>"
"      <menuitem action='SaveCDRDAO'/>"
"      <menuitem action='SaveMarkers'/>"
"      <menuitem action='SaveSplit'/>"
"      <menuitem action='Quit'/>"
"    </menu>"
"    <menu action='EditMenu'>"
"      <menuitem action='Undo'/>"
"      <menuitem action='Filter'/>"
"      <menuitem action='PinkNoise'/>"
"      <menuitem action='Amplify'/>"
"      <menuitem action='DeclickStrong'/>"
"      <menuitem action='DeclickWeak'/>"
"      <menuitem action='DeclickManual'/>"
"      <menuitem action='Decrackle'/>"
"      <menuitem action='Estimate'/>"
"      <menuitem action='Sample'/>"
"      <menuitem action='Denoise'/>"
"      <menuitem action='Silence'/>"
"      <menuitem action='Reverb'/>"
"      <separator/>"
"      <menuitem action='Cut'/>"
"      <menuitem action='Copy'/>"
"      <menuitem action='Paste'/>"
"      <menuitem action='Delete'/>"
"    </menu>"
"    <menu action='ViewMenu'>"
"      <menuitem action='ZoomSelect'/>"
"      <menuitem action='ZoomIn'/>"
"      <menuitem action='ZoomOut'/>"
"      <menuitem action='ViewAll'/>"
"      <menuitem action='SelectAll'/>"
"      <menuitem action='SelectNone'/>"
"      <menuitem action='Spectral'/>"
"    </menu>"
"    <menu action='MarkersMenu'>"
"      <menuitem action='ToggleBegin'/>"
"      <menuitem action='ToggleEnd'/>"
"      <menuitem action='ClearMarkers'/>"
"      <menuitem action='ExpandSelection'/>"
"      <separator/>"
"      <menuitem action='MarkSongs'/>"
"      <menuitem action='MoveMarker'/>"
"      <menuitem action='AddMarker'/>"
"      <menuitem action='AddMarkerPair'/>"
"      <menuitem action='DeleteMarker'/>"
"      <menuitem action='NextMarker'/>"
"    </menu>"
"    <menu action='SettingsMenu'>"
"      <menuitem action='DeclickPrefs'/>"
"      <menuitem action='DecracklePrefs'/>"
"      <menuitem action='DenoisePrefs'/>"
"      <menuitem action='MP3SimplePrefs'/>"
"      <menuitem action='MP3Prefs'/>"
"      <menuitem action='OggPrefs'/>"
"      <menuitem action='MiscPrefs'/>"
"    </menu>"
"    <menu action='HelpMenu'>"
"      <menuitem action='Help'/>"
"      <menuitem action='About'/>"
"    </menu>"
"  </menubar>"
"  <toolbar name='MainToolbar' action='MainToolbarAction'>"
"    <placeholder name='ToolItems'>"
"      <separator/>"
"      <toolitem action='Undo'/>"
"      <toolitem action='Amplify'/>"
"      <toolitem action='DeclickStrong'/>"
"      <toolitem action='DeclickWeak'/>"
"      <toolitem action='DeclickManual'/>"
"      <toolitem action='Decrackle'/>"
"      <toolitem action='Estimate'/>"
"      <toolitem action='Sample'/>"
"      <toolitem action='Denoise'/>"
"      <toolitem action='Silence'/>"
"      <toolitem action='Cut'/>"
"      <toolitem action='Copy'/>"
"      <toolitem action='Paste'/>"
"      <toolitem action='Delete'/>"
"      <separator/>"
"      <separator/>"
"      <toolitem action='PlayAudio'/>"
"      <toolitem action='StopAudio'/>"
"      <toolitem action='ZoomSelect'/>"
"      <toolitem action='ZoomIn'/>"
"      <toolitem action='ZoomOut'/>"
"      <toolitem action='ViewAll'/>"
"      <toolitem action='SelectAll'/>"
"      <toolitem action='Spectral'/>"
"      <separator/>"
"    </placeholder>"
"  </toolbar>"
"</ui>";

/* Toggle items */
// todo
/*static const GtkToggleActionEntry toggle_entries[] = {
  { "FullScreen", NULL, "_Full Screen", "F11", "Switch between full screen and windowed mode", full_screen_action_callback, FALSE }
};*/

GtkStatusbar *status_bar;
GtkWidget *progress_bar;
guint status_message, push_message;

void update_progress_bar(gfloat percentage, gfloat min_delta,
		   gboolean init_flag)
{
#ifdef BY_DATA_LENGTH
    static gfloat last_percentage_displayed = -1.0;

    if (percentage - last_percentage_displayed > min_delta
	|| init_flag == TRUE) {
	doing_progressbar_update = TRUE;
	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR(progress_bar), percentage);
	gnome_flush();
	doing_progressbar_update = FALSE;
	last_percentage_displayed = percentage;
    }
#else
    static struct timeval last_displayed = { 0, 0 };
    static struct timezone tz = { 0, 0 };
    static struct timeval this;
    long delta_ms;

    gettimeofday(&this, &tz);

    delta_ms =
	(this.tv_sec - last_displayed.tv_sec) * 1000 + (this.tv_usec -
							last_displayed.
							tv_usec) / 1000;

    if (delta_ms > 1000 * min_delta || init_flag == TRUE) {
	doing_progressbar_update = TRUE;
	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR(progress_bar), percentage);
	gnome_flush();
	doing_progressbar_update = FALSE;
	last_displayed = this;
    }
    #endif
}

// Show "tooltips" in the statusbar for menu items
static void on_menu_item_selected(GtkItem *item)
{
    guint id = gtk_statusbar_get_context_id(status_bar, "MenuItemHints");
    gchar *hint;
    g_object_get(gtk_widget_get_action(GTK_WIDGET(item)), "tooltip", &hint, NULL);

    if (hint) {
        gtk_statusbar_push(status_bar, id, hint);
        g_free(hint);
    }
}

static void on_menu_item_deselected(GtkItem *item)
{
    guint id = gtk_statusbar_get_context_id(status_bar, "MenuItemHints");
    gchar *hint;
    // gtk_action_get_label requires at least GTK 2.16!  I guess we could use gtk_action_get_name for lower versions if we care...
    g_object_get(gtk_widget_get_action(GTK_WIDGET(item)), "tooltip", &hint, NULL);

    if (hint) {
		gtk_statusbar_pop(status_bar, id);
        g_free(hint);
    }
}

// Also show "labels" in statusbar for toolbar items
static gboolean on_tool_item_enter_event (GtkAction *action)
{
    guint id = gtk_statusbar_get_context_id(status_bar, "ToolItemHints");
    // gtk_action_get_label requires at least GTK 2.16!  I guess we could use gtk_action_get_name for lower versions if we care...
    const gchar *tooltip = gtk_action_get_label (action);
    gtk_statusbar_push (status_bar, id, tooltip);
    // don't prevent the normal gtk handlers from being invoked for the event.
    return FALSE;
}

static gboolean on_tool_item_leave_event (GtkWidget *tool_item)
{
    guint id = gtk_statusbar_get_context_id(status_bar, "ToolItemHints");
    gtk_statusbar_pop (status_bar, id);
    // don't prevent the normal gtk handlers from being invoked for the event.
    return FALSE;
}

// Set up callback functions to do the menu hints in the statusbar
// Code adapted from mousepad
static void ui_manager_connect_proxy (  GtkUIManager *ui_manager,
					GtkAction *action,
					GtkWidget *proxy )
{
  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (GTK_IS_UI_MANAGER (ui_manager));

  if (GTK_IS_MENU_ITEM (proxy))
    {
      /* menu item's don't work with gdk window events */
      g_signal_connect (proxy, "select", G_CALLBACK (on_menu_item_selected), NULL);
      g_signal_connect (proxy, "deselect", G_CALLBACK (on_menu_item_deselected), NULL);
    }
  else if (GTK_IS_TOOL_ITEM (proxy))
    {
      GtkWidget *child;

      /* tool items will have GtkButton or other widgets in them, we want the child */
      child = gtk_bin_get_child (GTK_BIN (proxy));

      /* get events for mouse enter/leave and focus in/out */
      gtk_widget_add_events (child, GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);

      /* connect to signals for the events */
      g_signal_connect_swapped (child, "enter-notify-event", G_CALLBACK (on_tool_item_enter_event), action);
      g_signal_connect (child, "leave-notify-event", G_CALLBACK (on_tool_item_leave_event), NULL);
    }
}

// Do we actually need this?
static void ui_manager_disconnect_proxy ( GtkUIManager *ui_manager,
					  GtkAction *action,
					  GtkWidget *proxy )
{
  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (GTK_IS_MENU_ITEM (proxy) || GTK_IS_TOOL_ITEM (proxy));
  g_return_if_fail (GTK_IS_UI_MANAGER (ui_manager));

  if (GTK_IS_MENU_ITEM (proxy))
    {
      /* disconnect from the select/deselect signals */
      g_signal_handlers_disconnect_by_func (proxy, on_menu_item_selected, NULL);
      g_signal_handlers_disconnect_by_func (proxy, on_menu_item_deselected, NULL);
    }
  else if (GTK_IS_TOOL_ITEM (proxy))
    {
      GtkWidget *child;

      /* tool items will have GtkButton or other widgets in them, we want the child */
      child = gtk_bin_get_child (GTK_BIN (proxy));

      /* disconnect from the gdk window event signals */
      g_signal_handlers_disconnect_by_func (child, on_tool_item_enter_event, NULL);
      g_signal_handlers_disconnect_by_func (child, on_tool_item_leave_event, NULL);
    }
}

void set_status_text(gchar * msg)
{
    gtk_statusbar_pop (status_bar, status_message);
    gtk_statusbar_push (status_bar, status_message, msg);
}

void push_status_text(gchar * msg)
{
    gtk_statusbar_push (status_bar, push_message, msg);
//    push_values = g_list_prepend (push_values, GUINT_TO_POINTER (value));
}

void pop_status_text(void)
{
    gtk_statusbar_pop (status_bar, push_message);
//    push_values = g_list_remove_link (push_values, push_values);
}

/* Create a new backing pixmap of the appropriate size */
static gint audio_area_configure_event(GtkWidget * widget, GdkEventConfigure * event)
{
    if (audio_pixmap)
	gdk_pixmap_unref(audio_pixmap);

    audio_pixmap = gdk_pixmap_new(widget->window,
				  widget->allocation.width,
				  widget->allocation.height, -1);
    if (highlight_pixmap)
	gdk_pixmap_unref(highlight_pixmap);

    highlight_pixmap = gdk_pixmap_new(widget->window,
				      widget->allocation.width,
				      widget->allocation.height, -1);
    if (cursor_pixmap)
	gdk_pixmap_unref(cursor_pixmap);

    cursor_pixmap = gdk_pixmap_new(widget->window,
				   1, widget->allocation.height, -1);

    gdk_draw_rectangle(audio_pixmap,
		       widget->style->white_gc,
		       TRUE,
		       0, 0,
		       widget->allocation.width,
		       widget->allocation.height);
    gdk_draw_rectangle(highlight_pixmap,
		       widget->style->white_gc,
		       TRUE,
		       0, 0,
		       widget->allocation.width,
		       widget->allocation.height);
    audio_view.canvas_width = widget->allocation.width;
    audio_view.canvas_height = widget->allocation.height;
    main_redraw(FALSE, TRUE);

    return TRUE;
}

int audio_area_button_event(GtkWidget *c, GdkEventButton *event, gpointer data)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE))
	{
		return _audio_area_button_event(c, event);
	}
	return FALSE;
}

int audio_area_motion_event(GtkWidget *c, GdkEventMotion *event)
{
    if ((file_processing == FALSE) && (file_is_open == TRUE))
    	{
		return _audio_area_motion_event(c, event);
	}
	return FALSE;
}

/* Redraw the screen from the backing pixmap */
gint audio_expose_event(GtkWidget * widget, GdkEventExpose * event)
{
    gdk_draw_pixmap(widget->window,
		widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
		highlight_pixmap,
		event->area.x, event->area.y,
		event->area.x, event->area.y,
		event->area.width, event->area.height);

/*      g_print("expose x:%d y:%d width:%d height:%d\n",  */
/*  	       event->area.x, event->area.y,  */
/*  	       event->area.width, event->area.height);  */

    return FALSE;
}

void gwc_window_set_title(char *title)
{
    char buf[255];
    snprintf(buf, sizeof(buf), "%s - Gtk Wave Cleaner", title);
    gtk_window_set_title(GTK_WINDOW(main_window), buf);
}

GtkWidget *mk_label_and_pack(GtkBox * box, char *text)
{
    GtkWidget *w = gtk_label_new(text);
    gtk_box_pack_start(box, w, TRUE, TRUE, 0);
    return w;
}

void gwc_signal_handler(int sig)
{
    if (close_wavefile(&audio_view)) {
	save_preferences();
	undo_purge();
    }

    switch (sig) {
    case SIGSEGV:
	display_message("Segmentation Fault", "Fatal");
	break;
    case SIGBUS:
	display_message("Bus Error", "Fatal");
	break;
    }

    exit(1);
}

long time_to_sample(char *time, struct sound_prefs *p)
{
    int nf;
    int h, m;
    double s;
    long position = 0;

    nf = sscanf(time, "%d:%d:%lf", &h, &m, &s);

    if (nf == 3) {
	position = (h * 3600 + m * 60) * p->rate + s * p->rate;
    } else if (nf == 2) {
	nf = sscanf(time, "%d:%lf", &m, &s);
	position = (m * 60) * p->rate + s * p->rate;
    } else if (nf == 1) {
	nf = sscanf(time, "%lf", &s);
	position = s * p->rate;
    }

    return position;
}

long batch_atol(char *time)
{
	if( strcmp(time,"end") == 0 )
		return prefs.n_samples-1;
	else
		return atol(time);
}

long batch_time_to_sample(char *time, struct sound_prefs *p)
{
	if( strcmp(time,"end") == 0 )
		return p->n_samples-1;
	else
		return time_to_sample(time, p);	
}

void batch(int argc, char **argv)
{
#define BYTIME 0
#define BYSAMPLE 1

    int type = BYTIME ; 
    if (file_is_open == TRUE) {
		batch_mode = 1 ;

		if( argv[3] == NULL )
			usage(argv[0]);

		if(!strcmp(argv[2], "batchs")) {
		type = BYSAMPLE ;
		}
		if(!strcasecmp(argv[3], "declick")) {
		if(argc < 7) {
			fprintf(stderr, "Usage: %s <wavfile> <batch[s] declick sensitivity start_position stop_position>\n", argv[0]) ;
		} else {
			declick_detector_type = FFT_DETECT;
			double sens = atof(argv[4]) ;

			if(type == BYTIME) {
			audio_view.selected_first_sample = time_to_sample(argv[5],&prefs) ;
			audio_view.selected_last_sample = batch_time_to_sample(argv[6],&prefs) ;
			} else {
			audio_view.selected_first_sample = atol(argv[5]) ;
			audio_view.selected_last_sample = batch_atol(argv[6]) ;
			}

			audio_view.selection_region = TRUE;
			audio_view.channel_selection_mask = 0x03;
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(detect_only_widget), FALSE);
			click_data.max_clicks = MAX_CLICKS ;
			click_data.n_clicks = 0 ;
			g_print("Declick from %ld to %ld\n", audio_view.selected_first_sample, audio_view.selected_last_sample) ;
			declick_with_sensitivity(sens);
		}
		}
		else if(!strcasecmp(argv[3], "declick-hpf")) {
		if(argc < 7) {
			fprintf(stderr, "Usage: %s <wavfile> <batch[s] declick sensitivity start_position stop_position>\n", argv[0]) ;
		} else {
			declick_detector_type = HPF_DETECT;
			double sens = atof(argv[4]) ;

			if(type == BYTIME) {
			audio_view.selected_first_sample = time_to_sample(argv[5],&prefs) ;
			audio_view.selected_last_sample = batch_time_to_sample(argv[6],&prefs) ;
			} else {
			audio_view.selected_first_sample = atol(argv[5]) ;
			audio_view.selected_last_sample = batch_atol(argv[6]) ;
			}

			audio_view.selection_region = TRUE;
			audio_view.channel_selection_mask = 0x03;
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(detect_only_widget), FALSE);
			click_data.max_clicks = MAX_CLICKS ;
			click_data.n_clicks = 0 ;
			g_print("Declick from %ld to %ld\n", audio_view.selected_first_sample, audio_view.selected_last_sample) ;
			declick_with_sensitivity(sens);
		}
		}
		else if(!strcasecmp(argv[3], "amplify")) {
		if(argc < 7) {
			fprintf(stderr, "Usage: %s <wavfile> <batch[s] amplify amount start_position stop_position>\n", argv[0]) ;
		} else {
			double amount = atof(argv[4]) ;
			long first, last ;

			if(type == BYTIME) {
			audio_view.selected_first_sample = time_to_sample(argv[5],&prefs) ;
			audio_view.selected_last_sample = batch_time_to_sample(argv[6],&prefs) ;
			} else {
			audio_view.selected_first_sample = atol(argv[5]) ;
			audio_view.selected_last_sample = batch_atol(argv[6]) ;
			}
			first = audio_view.selected_first_sample ;
			last = audio_view.selected_last_sample ;

			audio_view.selection_region = TRUE;
			audio_view.channel_selection_mask = 0x03;

			g_print("amplify from %ld to %ld\n", audio_view.selected_first_sample, audio_view.selected_last_sample) ;
			simple_amplify_audio(&prefs, first, last, audio_view.channel_selection_mask, amount) ;
		}
		}
		else if(!strcasecmp(argv[3], "denoise")) {
		if(argc < 8) {
			fprintf(stderr, "Usage: %s <wavfile> <batch[s] denoise sample_start sample_end denoise_start denoise_end>\n", argv[0]) ;
		} else {
			if(type == BYTIME) {
			denoise_data.noise_start = time_to_sample(argv[4],&prefs);
			denoise_data.noise_end = batch_time_to_sample(argv[5],&prefs);
			audio_view.selected_first_sample = time_to_sample(argv[6],&prefs);
			audio_view.selected_last_sample = batch_time_to_sample(argv[7],&prefs);
			} else {
			denoise_data.noise_start = atol(argv[4]);
			denoise_data.noise_end = batch_atol(argv[5]);
			audio_view.selected_first_sample = atol(argv[6]);
			audio_view.selected_last_sample = batch_atol(argv[7]);
			}

			audio_view.selection_region = TRUE;
			audio_view.channel_selection_mask = 0x03;
			get_region_of_interest(&denoise_data.denoise_start, &denoise_data.denoise_end, &audio_view) ;
			denoise_data.ready = TRUE ;
			load_denoise_preferences() ;
			print_denoise("batch remove_noise",&denoise_prefs) ;
			if(denoise_prefs.FFT_SIZE > (denoise_data.noise_end-denoise_data.noise_start+1)) {
			fprintf(stderr, "FFT_SIZE must be <= # samples in noise sample!\n") ;
			return;
			}
			g_print("Denoise from %ld to %ld using noise sample from %ld to %ld\n", denoise_data.denoise_start, denoise_data.denoise_end, denoise_data.noise_start, denoise_data.noise_end) ;
			push_status_text("Denoising selection") ;
			denoise(&prefs, &denoise_prefs, denoise_data.noise_start, denoise_data.noise_end,
				denoise_data.denoise_start, denoise_data.denoise_end, audio_view.channel_selection_mask) ;
			resample_audio_data(&prefs, denoise_data.denoise_start, denoise_data.denoise_end) ;
			save_sample_block_data(&prefs) ;
			pop_status_text() ;
		}
		}
		else if(!strcasecmp(argv[3], "normalize") || !strcasecmp(argv[3], "normalise")) {
		g_print("Normalize audiofile\n");
		batch_normalize(&prefs,0,prefs.n_samples-1,prefs.n_channels > 1 ? 0x03 : 0x01);
		}
		else if(!strcasecmp(argv[3], "dsp")) {
		if(argc < 6) {
			fprintf(stderr, "Usage: %s <wavfile> <batch[s] dsp start_position stop_position>\n", argv[0]) ;
		} else {
			long first, last ;

			if(type == BYTIME) {
			audio_view.selected_first_sample = time_to_sample(argv[4],&prefs) ;
			audio_view.selected_last_sample = batch_time_to_sample(argv[5],&prefs) ;
			} else {
			audio_view.selected_first_sample = atol(argv[4]) ;
			audio_view.selected_last_sample = batch_atol(argv[5]) ;
			}
			first = audio_view.selected_first_sample ;
			last = audio_view.selected_last_sample ;

			audio_view.selection_region = TRUE;
			audio_view.channel_selection_mask = 0x03;
			get_region_of_interest(&first, &last, &audio_view);
			g_print("Applying DSP filter from %ld to %ld\n", first, last);
			filter_audio(&prefs, first, last, audio_view.channel_selection_mask);
			save_sample_block_data(&prefs);
		}
		}
		else if(!strcasecmp(argv[3], "reverb")) {
		if(argc < 6) {
			fprintf(stderr, "Usage: %s <wavfile> <batch[s] reverb start_position stop_position>\n", argv[0]) ;
		} else {
			long first, last ;

			if(type == BYTIME) {
			audio_view.selected_first_sample = time_to_sample(argv[4],&prefs) ;
			audio_view.selected_last_sample = batch_time_to_sample(argv[5],&prefs) ;
			} else {
			audio_view.selected_first_sample = atol(argv[4]) ;
			audio_view.selected_last_sample = batch_atol(argv[5]) ;
			}
			first = audio_view.selected_first_sample ;
			last = audio_view.selected_last_sample ;
			audio_view.selection_region = TRUE;
			audio_view.channel_selection_mask = 0x03;
			get_region_of_interest(&first, &last, &audio_view);
			g_print("Adding reverb from %ld to %ld\n", first, last);
			load_reverb_preferences() ;
			reverb_audio(&prefs, first, last, audio_view.channel_selection_mask);
			save_sample_block_data(&prefs);
		}
		}
		else if(!strcasecmp(argv[3], "truncate")) {
		if(argc < 6) {
			fprintf(stderr, "Usage: %s <wavfile> <batch[s] truncate keep_start keep_end>\n", argv[0]) ;
		} else {
			long first, last ;
		#ifdef TRUNCATE_OLD
			if(type == BYTIME) {
			audio_view.truncate_head = time_to_sample(argv[4],&prefs) ;
			audio_view.truncate_tail = batch_time_to_sample(argv[5],&prefs) ;
			} else {
			audio_view.truncate_head = atol(argv[4]) ;
			audio_view.truncate_tail = batch_atol(argv[5]) ;
			}

			g_print("Truncating.  Keeping samples %ld to %ld\n", audio_view.truncate_head, audio_view.truncate_tail) ;
			truncate_wavfile(&audio_view);
			audio_view.truncate_head = 0;
			audio_view.truncate_tail = audio_view.n_samples;
		#else
			if(type == BYTIME) {
			audio_view.selected_first_sample = time_to_sample(argv[4],&prefs) ;
			audio_view.selected_last_sample = batch_time_to_sample(argv[5],&prefs) ;
			} else {
			audio_view.selected_first_sample = atol(argv[4]) ;
			audio_view.selected_last_sample = batch_atol(argv[5]) ;
			}
			audio_view.selection_region = TRUE;
			first = audio_view.selected_first_sample ;
			last = audio_view.selected_last_sample ;
			g_print("Truncating.  Keeping samples %ld to %ld\n", first, last);

			if(first == 0) {
			long total_samples =  last-first+1 ;
			sndfile_truncate(total_samples) ;
			} else {
			truncate_wavfile(&audio_view, 0); /* 0 == don't save undo data */
			}
		#endif
		}
		}
		else
		usage(argv[0]);
	}

    cleanup_and_close(&audio_view, &prefs);

// ajh: don't think this is needed
//    batch_mode = 0 ;

    return ;
}

int main(int argc, char *argv[])
{
    GtkWidget *main_vbox, *menubar, *toolbar,
	*led_vbox, *track_times_vbox, *times_vbox, 
	*bottom_hbox, *detect_only_box, *leave_click_marks_box;
    GtkActionGroup *action_group;
    GtkUIManager *ui_manager;
    GError *error;

    int i;

    /* *************************************************************** */
    /* Lindsay Harris addition for SMP operations  */
    /*
     *   On SMP systems,  the stack size does not grow, for reasons beyond
     *  my understanding.   So,  set an increase in size now.  For my
     *  SuSE 8.2 system, the initial size is 2MB,  whereas the "denoise"
     *  operation requires about 3.5MB,  so I picked an arbitrary 4MB
     *  stack size.  This might not be sufficient for some other operations.
     */

    {
	struct  rlimit   rl;

#define  GWC_STACK_LIM (6 * 1024 *1024)

	if( getrlimit( RLIMIT_STACK, &rl ) == -1 )
	{
	    perror( "getrlimit for RLIMIT_STACK:" );
	    exit( 1 );
	}

	printf("Current stack limit: %d bytes\n", (int)rl.rlim_cur) ;

	/*   Only change the size if it's too small.   */
	if( rl.rlim_cur < GWC_STACK_LIM )
	{
	    rl.rlim_cur = GWC_STACK_LIM ;

	    if( setrlimit( RLIMIT_STACK, &rl ) == -1 )
	    {
		perror( "setrlimit for RLIMIT_STACK:" );
		exit( 1 );
	    }
	    printf("New stack limit: %d bytes\n", (int)rl.rlim_cur) ;
	}
    }

    #define PREFIX "."
    #define SYSCONFDIR "."

    load_preferences();
    
    /* load all encoding preferences on start */
    // Why load these but not the filter and reverb preferences?
    load_ogg_encoding_preferences();
    load_mp3_encoding_preferences();
    load_mp3_simple_encoding_preferences();
    
    /* load ~/.config/gtk-wave-cleaner/gtkrc if it exists */
    gtk_rc_add_default_file ( g_build_filename (g_get_user_config_dir (), APPNAME, "gtkrc", NULL) );

    /* This is called in all GTK applications. Arguments are parsed
     * from the command line and are returned to the application. */
    gtk_init(&argc, &argv);
    g_set_application_name("Gtk Wave Cleaner");
	
    register_stock_icons ();

    main_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    if (gtk_icon_theme_has_icon(gtk_icon_theme_get_default(), APPNAME) == FALSE)
	{
		GdkPixbuf *icon =  gtk_widget_render_icon(main_window, "gwc_icon", 6, NULL);
		gtk_icon_theme_add_builtin_icon(APPNAME, 48, icon);
    }
	gtk_window_set_default_icon_name(APPNAME);
	
    //gtk_window_set_default_icon_from_file(pixmapdir "/gwc-logo.png", NULL);
    gtk_window_set_title(GTK_WINDOW(main_window), "Gtk Wave Cleaner: Dehiss, declick audio files");

    // Remember the window geometry from the last time GWC was used, but check for sanity
    // Looks like we don't actually need to get this information - see comments below.
    GdkDisplay *display = gdk_display_get_default();
    GdkScreen *screen = gdk_display_get_default_screen(display);
    gint screen_width = gdk_screen_get_width(screen);
    gint screen_height = gdk_screen_get_height(screen);
    // Looks like we could just set it to window_width and window_height - perhaps because I have a helpful window manager?
    gtk_window_set_default_size(GTK_WINDOW(main_window), MIN( window_width, screen_width), 
				MIN(window_height, screen_height) );
    // Looks like we could just set it to window_x and window_y - perhaps because I have a helpful window manager?
    gtk_window_move(GTK_WINDOW(main_window), MIN( window_x, screen_width - 20), 
		    MIN( window_y, screen_height - 20 ) );
    if(window_maximised == TRUE)
    {
        gtk_window_maximize(GTK_WINDOW(main_window));
    }


	g_signal_connect(main_window, "drag-data-received", G_CALLBACK(drag_data_received), NULL);

	GtkTargetEntry targets[] = {
		{"text/uri-list", GTK_TARGET_OTHER_APP, 0},
	};
	gtk_drag_dest_set(main_window, GTK_DEST_DEFAULT_ALL, targets, 1, GDK_ACTION_COPY);

    /* When the window is given the "delete_event" signal (this is given
     * by the window manager, usually by the "close" option, or on the
     * titlebar), we ask it to call the delete_event () function
     * as defined above. The data passed to the callback
     * function is NULL and is ignored in the callback function. */
    gtk_signal_connect(GTK_OBJECT(main_window), "delete_event",
		       GTK_SIGNAL_FUNC(delete_event), NULL);

    g_signal_connect(GTK_OBJECT(main_window), "key_press_event",
		       GTK_SIGNAL_FUNC(key_press_cb), NULL);

    /* Sets the border width of the window. */
    gtk_container_set_border_width(GTK_CONTAINER(main_window), 1);

    /* Creates a new button with the label "Stop Recording". */

    /* When the button receives the "clicked" signal, it will call the
     * function stop() passing it NULL as its argument.  The stop()
     * function is defined above. */

    /*   g_signal_connect (GTK_OBJECT (stop_button), "clicked",  */
    /*  		     GTK_SIGNAL_FUNC (stop), NULL);  */

    main_vbox = gtk_vbox_new(FALSE, 0);
    track_times_vbox = gtk_vbox_new(FALSE, 1);
    times_vbox = gtk_vbox_new(FALSE, 1);
    led_vbox = gtk_vbox_new(FALSE, 1);
    bottom_hbox = gtk_hbox_new(FALSE, 1);
    gtk_container_add (GTK_CONTAINER (main_window), main_vbox);

    ui_manager = gtk_ui_manager_new ();

    // Set up the signals to do the menu hints in the statusbar
    g_signal_connect (ui_manager, "connect-proxy",
                      G_CALLBACK (ui_manager_connect_proxy), NULL);
    // Do we actually need this?
    g_signal_connect (ui_manager, "disconnect-proxy",
                      G_CALLBACK (ui_manager_disconnect_proxy), NULL);

    action_group = gtk_action_group_new ("MenuActions");
    gtk_action_group_add_actions (action_group, entries, 
				  G_N_ELEMENTS (entries), main_window);
    /*gtk_action_group_add_toggle_actions (action_group, toggle_entries, 
					 G_N_ELEMENTS (toggle_entries), main_window);
    gtk_action_group_add_radio_actions (action_group, radio_entries, 
					G_N_ELEMENTS (radio_entries), 0, 
					radio_action_callback, main_window);*/

    gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);

    gtk_accel_map_load( g_build_filename (g_get_user_config_dir (), APPNAME, ACCELERATORS_FILE, NULL) );
    gtk_window_add_accel_group (GTK_WINDOW (main_window), 
				gtk_ui_manager_get_accel_group (ui_manager));

    error = NULL;
    if (!gtk_ui_manager_add_ui_from_string (ui_manager, ui_description, -1, &error))
      {
        g_message ("building menus failed: %s", error->message);
        g_error_free (error);
        exit (EXIT_FAILURE);
      }

    menubar = gtk_ui_manager_get_widget (ui_manager, "/MainMenu");
    gtk_box_pack_start (GTK_BOX (main_vbox), menubar, FALSE, FALSE, 0);
    toolbar = gtk_ui_manager_get_widget (ui_manager, "/MainToolbar");
    gtk_toolbar_set_style (GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);
    gtk_box_pack_start (GTK_BOX (main_vbox), toolbar, FALSE, FALSE, 0);

/*// Need to do stuff like this if we want handles so we can rearrange things.
//  https://developer.gnome.org/gtk2/stable/GtkHandleBox.html
//  GtkHandleBox is deprecated since GTK+ 3.4
//  Maybe GDL is an alternative?
//  https://developer.gnome.org/gdl/
    edit_toolbar = gtk_ui_manager_get_widget (ui_manager, "/EditToolbar");
    gtk_toolbar_set_style (edit_toolbar, GTK_TOOLBAR_ICONS);
    GtkWidget* handlebox1;
    handlebox1 = gtk_handle_box_new ();
    gtk_container_add (handlebox1, edit_toolbar);
    gtk_box_pack_start (GTK_BOX (main_vbox), handlebox1, FALSE, FALSE, 0);
    transport_toolbar = gtk_ui_manager_get_widget (ui_manager, "/TransportToolbar");
    gtk_toolbar_set_style (transport_toolbar, GTK_TOOLBAR_ICONS);
    GtkWidget* handlebox2;
    handlebox2 = gtk_handle_box_new ();
    gtk_container_add (handlebox2, transport_toolbar);
    gtk_box_pack_start (GTK_BOX (main_vbox), handlebox2, FALSE, FALSE, 0);
*/

    {
	/* setup status bar at bottom of window including a progress display) */
	progress_bar = gtk_progress_bar_new ();
	status_bar = GTK_STATUSBAR(gtk_statusbar_new ());
	status_message = gtk_statusbar_get_context_id (
			status_bar, "status-message");
	push_message = gtk_statusbar_get_context_id (
			status_bar, "push-message");

	/* create a new canvas */
	audio_drawing_area = gtk_drawing_area_new();

	gtk_signal_connect(GTK_OBJECT(audio_drawing_area), "expose_event",
			   (GCallback) audio_expose_event, NULL);
	gtk_signal_connect(GTK_OBJECT(audio_drawing_area),
			   "configure_event",
			   (GCallback) audio_area_configure_event,
			   NULL);

	gtk_signal_connect(GTK_OBJECT(audio_drawing_area),
			   "button_press_event",
			   (GCallback) audio_area_button_event, NULL);
	gtk_signal_connect(GTK_OBJECT(audio_drawing_area),
			   "motion_notify_event",
			   (GCallback) audio_area_motion_event, NULL);
    /*          gtk_signal_connect (GTK_OBJECT(audio_drawing_area),"event",  */
    /*                             (GCallback) audio_area_event, NULL);  */

	gtk_widget_set_events(audio_drawing_area, GDK_EXPOSURE_MASK
			      | GDK_LEAVE_NOTIFY_MASK
			      | GDK_BUTTON_PRESS_MASK
			      | GDK_BUTTON_RELEASE_MASK
			      | GDK_POINTER_MOTION_MASK
			      | GDK_POINTER_MOTION_HINT_MASK);

	audio_view.canvas_width = 600;
	audio_view.canvas_height = 400;

	gtk_box_pack_start(GTK_BOX(main_vbox), audio_drawing_area, TRUE,
			   TRUE, 0);
    }

    scroll_pos = gtk_adjustment_new(1.0, 0.0, 100.0, 5.0, 5.0, 0.0);
    hscrollbar = gtk_hscrollbar_new(GTK_ADJUSTMENT(scroll_pos));

    gtk_signal_connect(GTK_OBJECT(GTK_ADJUSTMENT(scroll_pos)), "changed",
		       (GCallback) scroll_bar_changed, NULL);

    gtk_signal_connect(GTK_OBJECT(GTK_ADJUSTMENT(scroll_pos)),
		       "value_changed", (GCallback) scroll_bar_changed,
		       NULL);

    gtk_box_pack_start(GTK_BOX(main_vbox), hscrollbar, FALSE, TRUE, 0);

    l_file_channels =
    mk_label_and_pack(GTK_BOX(track_times_vbox), "Audio Channels: 0");
    l_file_rate =
    mk_label_and_pack(GTK_BOX(track_times_vbox), "Samplerate: 0");
    l_file_time =
    mk_label_and_pack(GTK_BOX(track_times_vbox), "Track Length 0:00:000");
    l_file_samples =
    mk_label_and_pack(GTK_BOX(track_times_vbox), "Track samples: 0");
    l_first_time =
    mk_label_and_pack(GTK_BOX(times_vbox), "First 0:00:000");
    l_last_time = mk_label_and_pack(GTK_BOX(times_vbox), "Last 0:00:000");
    l_selected_time = mk_label_and_pack(GTK_BOX(times_vbox), "Selected 0:00:000");
    l_samples = mk_label_and_pack(GTK_BOX(times_vbox), "Samples: 0");

    for (i = 0; i < 2; i++) {
	dial[i] = led_bar_new(20, 0);
	gtk_box_pack_start(GTK_BOX(led_vbox), dial[i], TRUE, TRUE, 0);
    }

    led_bar_light_percent(dial[0], (0.0), 0.0);
    led_bar_light_percent(dial[1], (0.0), 0.0);

    gtk_box_pack_start(GTK_BOX(bottom_hbox), led_vbox, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(bottom_hbox), track_times_vbox, TRUE, TRUE,
		       0);
    gtk_box_pack_start(GTK_BOX(bottom_hbox), times_vbox, TRUE, TRUE, 0);

    gtk_box_pack_start(GTK_BOX(main_vbox), bottom_hbox, FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(status_bar), GTK_WIDGET(progress_bar), FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(main_vbox), GTK_WIDGET(status_bar), FALSE, TRUE, 0);

    {
	detect_only_box = gtk_hbox_new(FALSE, 10);
	detect_only_widget =
	    gtk_check_button_new_with_label
	    ("Detect, do not repair clicks");
	GTK_WIDGET_UNSET_FLAGS(detect_only_widget, GTK_CAN_FOCUS);
	gtk_box_pack_start(GTK_BOX(detect_only_box), detect_only_widget,
			   FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(led_vbox), detect_only_box, FALSE,
			   FALSE, 0);

	leave_click_marks_box = gtk_hbox_new(FALSE, 11);
	leave_click_marks_widget =
	    gtk_check_button_new_with_label
	    ("Leave click marks after repairing");
	GTK_WIDGET_UNSET_FLAGS(leave_click_marks_widget, GTK_CAN_FOCUS);
	gtk_box_pack_start(GTK_BOX(leave_click_marks_box), leave_click_marks_widget,
			   FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(led_vbox), leave_click_marks_box, FALSE,
			   FALSE, 0);
    }

    gchar *newdir = g_build_filename (g_get_user_cache_dir (), "gwcXXXXXX", NULL) ; 
	gchar *_CLIPBOARD_FILE = "gwc_intclip.dat" ;
    // note that g_mkdtemp rather than mkdtemp is required for portability e.g. to Solaris (not OpenSolaris)
    // unfortunately it pushes the minimum GLIB required to 2.30, so is it really more or less portable...?
    if (!g_mkdtemp (newdir))
    {
	  // this is expected if $XDG_CACHE_HOME is not set
	  // therefore don't use g_warning, which will error if G_DEBUG environment variable is set to "fatal-warnings"
      warning ("Could not create directory for temporary files in $XDG_CACHE_HOME\nFalling back to current working directory.");
      // note: assume if we can't create a folder for our undo files we probably can't create a clipboard file there either.
      // It would be best to move this to the file open routine and if we can't use $XDG_CACHE_HOME to save the temp files 
      // in the same location as the file we are working on. https://github.com/AlisterH/gwc/issues/9 
	  //*newdir = g_build_filename (g_path_get_dirname ( TODO  filename), "gwcXXXXXX", NULL) ; 
	  // should be able to avoid this nonsense - need to study pointers again I think
	  newdir = g_build_filename (g_get_current_dir(), "gwcXXXXXX", NULL) ; 
	  // might not need to test this - is it actually possible for it to fail but us still be able to write to the audio file we are working on itself?
      if (!g_mkdtemp (newdir))
      {
      	warning ("Could not create directory for temporary files in current working directory.\n"
      	         "If you run more than one instance of GWC in this directory their undo files will conflict.\n");
        warning ("Is the current working directory read-only?\n"
               "If so we won't be able to save undo files!\n"
               "Strongly recommend you exit and create ~/.cache/, or restart in a writable directory!"); // we should really give them a button to exit gwc in this dialog and maybe others
      }
      else
        tmpdir = newdir;
	}
	else
	{
	  tmpdir = newdir;
	  // don't put the clipboard in tmpdir - this way we can share it between multiple instances of gwc
	  // note there are problems if we try to paste incompatible audio e.g. mono vs stereo
	  // and should we have some sort of locking so multiple instances don't try to write it at the same time?
	  // also note there is a problem with undo (perhaps only with mono?)
	  _CLIPBOARD_FILE = g_build_filename (g_get_user_cache_dir (), "gwc_intclip.dat", NULL) ;
	}
	CLIPBOARD_FILE = _CLIPBOARD_FILE ;
	//printf("CLIPBOARD_FILE: %s\n", CLIPBOARD_FILE) ;

    /* and the window */
    gtk_widget_show_all(main_window);
	#ifdef MAC_OS_X
	// Note that we only tested if we are building on OSX, and are assuming we are building with the GDK QUARZ backend.
	// We should really check that, as we could be building with the X11 backend.

	// Supposedly just doing this first line makes closing from the dock work, etc. (#1)
	GtkosxApplication *theApp = g_object_new(GTKOSX_TYPE_APPLICATION, NULL);
	//We actually need this otherwise it just quits without asking about saving!
	g_signal_connect (theApp, "NSApplicationBlockTermination", G_CALLBACK (delete_event), NULL );
	g_signal_connect (theApp, "NSApplicationOpenFile", G_CALLBACK(app_open_file_cb), NULL); 
	
	// integrate the menus into the osx menu bar
	// N.B. osx adds a "special characters" entry to the edit menu, which is bizarre
	// but I guess useful for inserting symbols into the mp3 and ogg export metadata dialogs
	gtk_widget_hide (menubar);
	gtkosx_application_set_menu_bar(theApp, GTK_MENU_SHELL(menubar));
	GtkWidget *about_item, *preferences_menu, *close_item, *help_menu;
	// move the about item to the main (program name) menu
	about_item = gtk_ui_manager_get_widget(ui_manager, "/MainMenu/HelpMenu/About");
	gtkosx_application_insert_app_menu_item (theApp, GTK_WIDGET (about_item), 0);
	// move the preferences item to the main menu
	preferences_menu = gtk_ui_manager_get_widget(ui_manager, "/MainMenu/SettingsMenu");
	gtkosx_application_insert_app_menu_item (theApp, GTK_WIDGET (preferences_menu), 1);
	//	hide the close item in the file menu
	close_item = gtk_ui_manager_get_widget(ui_manager, "/MainMenu/FileMenu/Quit");
	gtk_widget_hide (close_item);
	// only need to do this so that the "window" menu is positioned correctly!
	help_menu = gtk_ui_manager_get_widget(ui_manager, "/MainMenu/HelpMenu");
	gtkosx_application_set_help_menu (theApp, GTK_MENU_ITEM (help_menu));
	// this adds a "window" menu, which doesn't really seem useful in my test environment
	// except that it enables the ⌘M keyboard shortcut to minimise!
	gtkosx_application_set_window_menu (theApp, NULL);
	
	// N.B. Setting the icon like this is unnecessary when running as a .app application bundle
	// And we need to do that to get osx to use the icon when the application isn't running.
	// We need the test so we don't override it when we are in an application bundle!
	if (gtkosx_application_get_bundle_id() == NULL)
	{
		GdkPixbuf *icon =  gtk_widget_render_icon(main_window, "gwc_icon", 6, NULL);
		gtkosx_application_set_dock_icon_pixbuf(theApp, icon);
    }
	
	// Possible todo:
	// implement native file dialogs using nativefiledialog library or tinyfiledialogs or something
	#endif

    /* and the idle function */
    /*      gtk_idle_add(idle_func, NULL);  */

    /*   start_monitor("/dev/dsp") ;  */
    /*   config_audio_input(44100, 16, 1) ;  */
    /*   config_audio_input(prefs.rate, prefs.bits, prefs.stereo) ;  */

    /* All GTK applications must have a gtk_main(). Control ends here
     * and waits for an event to occur (like a key press or
     * mouse event). */

    push_status_text("Ready");

    signal(SIGSEGV, gwc_signal_handler);
    signal(SIGBUS, gwc_signal_handler);

    {
	char buf[100];
	int v1, v2, v3, i;
	sf_command(NULL, SFC_GET_LIB_VERSION, buf, sizeof(buf));
    /*  	printf("sfversion: %s \n", buf) ;  */
	for (i = 0; i < strlen(buf); i++) {
	    if (buf[i] == '-') {
		i++;
		break;
	    }
	}

	sscanf(&buf[i], "%d.%d.%d", &v1, &v2, &v3);

	printf("libsndfile Version: %s %d %d %d\n", buf, v1, v2, v3) ;
	// Is this test right?  The logic seems odd.  In any case it is probably not necessary these days.
	if (v1 < 1 || v2 < 0 || v3 < 0) {
	    warning("libsndfile 1.0.0 or greater is required");
	    exit(1);
	}
    }
    if ( argc > 1 ) {
		if ( access (argv[1], F_OK) == 0 ) {
			strcpy(wave_filename, argv[1]);
			open_wave_filename();
			if (argc > 2) {
				if ((!strcasecmp(argv[2], "batch")) || (!strcasecmp(argv[2], "batchs"))) {
					batch(argc, argv);
					return EXIT_SUCCESS; //this closes the gui, but it isn't necessarily true that the batch processing was successful
				}
			}
		}
		else if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-help") == 0  || strcmp(argv[1], "-h") == 0 ) {
			usage( argv[0] );
		}
		else {
			//warning(strcat(argv[1], ": File not found")); //doing it like this prevents exit for some reason
			warning(g_strconcat(argv[1], ": File not found", NULL));
			if (argc > 2) { //alister: it seems very messy just duplicating this code in here, but it is easy and works 
				if ((!strcasecmp(argv[2], "batch")) || (!strcasecmp(argv[2], "batchs"))) {
					return EXIT_FAILURE; //this closes the gui, but in this case we know the batch processing wasn't successful
				}}
		}
    }

    #ifdef MAC_OS_X    
    // Re #1 above - we actually need this, otherwise nothing happens at all.
    // It has has to be down here after e.g. open_wave_filename, otherwise running `gwc somefile` crashes on osx.
    // But the rest of the gtkosx_application stuff needs to be before that otherwise it freezes when 
    // clicking the icon after running `gwc somefile`
    gtkosx_application_ready (theApp);
    #endif

    gtk_main();

    return (0);
}
