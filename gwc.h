/*****************************************************************************
*   Gnome Wave Cleaner Version 0.21
*   Copyright (C) 2001, 2002, 2003, 2004, 2005, 2006 Jeffrey J. Welty
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

/* gwc.h */
#define _ISOC9X_SOURCE  1
#define _ISOC99_SOURCE  1
#define __USE_ISOC9X    1
#define __USE_ISOC99    1
#include <math.h>

#include <gnome.h>
#ifdef HAVE_FFTW3
#include <fftw3.h>
#if (FFTWPREC == 1)
    typedef float fftw_real ;
#    define FFTW(func) fftwf_ ## func
#else /* FFTWPREC == 1 */
    typedef double fftw_real ;
#    define FFTW(func) fftw_ ## func
#endif /* FFTWPREC == 0 */
#else
#ifdef HAVE_FFTW
#include <rfftw.h>
#else
#include <drfftw.h>
#endif
#endif

#include <limits.h>
#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

#define GWC_VERSION_MAJOR 0
#define GWC_VERSION_MINOR 21
#define VERSION "0.21-19"
#define GWC_POINT_HANDLE 0x01
#define SBW  128	/* Sample Block Width, the number of audio sammples summarized in one block  */
#define STATUS_UPDATE_INTERVAL 0.5	/* update status bar every 1/2  second on long edit operations */
#define MAX_AUDIO_BUFSIZE 32768

/* defs for declicking results */
#define SINGULAR_MATRIX 0
#define REPAIR_SUCCESS 1
#define REPAIR_CLIPPED 2
#define REPAIR_FAILURE 3
#define DETECT_ONLY 4
#define REPAIR_OOB 5

/* defs for declick detection method */
#define FFT_DETECT 0
#define HPF_DETECT 1

/* defs for denoise */
#define DENOISE_MAX_FFT 32768

#define DENOISE_WINDOW_BLACKMAN 0
#define DENOISE_WINDOW_BLACKMAN_HYBRID 1
#define DENOISE_WINDOW_HANNING_OVERLAP_ADD 2
#ifdef DENOISE_TRY_ONE_SAMPLE
#define DENOISE_WINDOW_ONE_SAMPLE 3
#define DENOISE_WINDOW_WELTY 4
#else
#define DENOISE_WINDOW_WELTY 3
#endif

#define DENOISE_WEINER 0
#define DENOISE_POWER_SPECTRAL_SUBTRACT 1
#define DENOISE_EM 2
#define DENOISE_LORBER 3
#define DENOISE_WOLFE_GODSILL 4
#define DENOISE_EXPERIMENTAL 5

//#define DENOISE_MAGNITUDE_SPECTRAL_SUBTRACT -1

/* defs for markers */
#define MAX_MARKERS 200
#define MARKER_RESET_VALUE -1000000000	/* (large negative long) markers to be completely
					    off the screen when they are reset */

/* defs for encoding */
#define GWC_OGG 1
#define GWC_MP3 2
#define GWC_MP3_SIMPLE 3


struct sound_prefs {
    int playback_bits ;
    int bits ;
    int rate ;
    int stereo ;
    long n_samples ;
    int n_channels ;
    int wavefile_fd ;
    double max_value ;
    long max_allowed ;
    int sample_buffer_exists ;
    long data_length ;

    size_t mmap_size ;
    void *mmap_file ;
    size_t data_offset ;
    unsigned long data_size ;
    void *data ;
    int successful_open ;
} ;

struct denoise_prefs {
    int noise_suppression_method ;
    int window_type ;
    int smoothness ;
    int FFT_SIZE ;
    int n_noise_samples ;
    double amount ;
    double dn_gamma ;
    double randomness ;
    double min_sample_freq ;
    double max_sample_freq ;
    int freq_filter ;
    int estimate_power_floor ;
    int rate ;
} ;

struct view {
    int canvas_width ;
    int canvas_height ;
    long first_sample ;
    long last_sample ;
    long selected_first_sample ;
    long selected_last_sample ;
    long cursor_position ;
    long prev_cursor_position ;
    int selection_region ;
    int channel_selection_mask ;
    long n_samples ;
#ifdef TRUNCATE_OLD
    long truncate_head, truncate_tail ;
#endif /* TRUNCATE_OLD */
} ;

struct sample_block {
    int n_samples ;
    double rms[2] ;
    double max_value[2] ;
} ;

struct sample_display_block {
    int n_samples ;
    double rms[2] ;
    double max_value[2] ;
} ;

#define MAX_CLICKS 1000

struct click_data {
    int n_clicks ;
    int max_clicks ;
    char channel[MAX_CLICKS] ;
    long start[MAX_CLICKS], end[MAX_CLICKS] ;
} ;

typedef struct {
    long noise_start ;
    long noise_end ;
    long denoise_start ;
    long denoise_end ;
    int ready ;
} DENOISE_DATA ;


void print_denoise(char *header, struct denoise_prefs *pDnprefs) ;

GtkWidget *add_number_entry_with_label(char *entry_text, char *label_text, GtkWidget *table, int row) ;
GtkWidget *add_number_entry_with_label_int(int value, char *label_text, GtkWidget *table, int row) ;
GtkWidget *add_number_entry_with_label_double(double value, char *label_text, GtkWidget *table, int row) ;
void add_song_marker(void) ;
void add_song_marker_pair(void) ;
void amplify_audio(struct sound_prefs *p, long first, long last, int channel_mask) ;
int  amplify_dialog(struct sound_prefs current, struct view *) ;
int  audio_area_button_event(GtkWidget *c, GdkEventButton *event, gpointer data) ;
int  audio_area_motion_event(GtkWidget *c, GdkEventMotion *event) ;
double blackman(int k, int N) ;
double blackman_hybrid(int k, int n_flat, int N) ;
int close_undo(void) ;
int close_wavefile(struct view *v) ;
void config_audio_device(int speed, int bits, int stereo) ;
void d_print(char *, ...) ;
int declick_a_click(struct sound_prefs *p, long first_sample, long last_sample, int channel_mask) ;
void declick_set_preferences(GtkWidget * widget, gpointer data) ;
void decrackle_set_preferences(GtkWidget * widget, gpointer data) ;
void denoise_set_preferences(GtkWidget * widget, gpointer data) ;
int denoise(struct sound_prefs *pPrefs, struct denoise_prefs *pDnprefs, long noise_start, long noise_end,
            long first_sample, long last_sample, int channel_mask) ;
int  denoise_dialog(struct sound_prefs current, struct view *) ;
int dethunk(struct sound_prefs *pPrefs, long first_sample, long last_sample, int channel_mask) ;
void audio_normalize(int flag);
void set_scroll_bar(long n, long first, long last);
void display_times(void) ;

char *do_declick(struct sound_prefs *p, long noise_start, long noise_end, int channel_selection_mask, double sensitivity, int repair,
               struct click_data *, int iterate_flag, int leave_click_marks) ;
char *do_declick_fft(struct sound_prefs *p, long noise_start, long noise_end, int channel_selection_mask, double sensitivity, int repair,
               struct click_data *, int iterate_flag, int leave_click_marks) ;
char *do_declick_hpf(struct sound_prefs *p, long noise_start, long noise_end, int channel_selection_mask, double sensitivity, int repair,
               struct click_data *, int iterate_flag, int leave_click_marks) ;

int do_decrackle(struct sound_prefs *p, long noise_start, long noise_end, int channel_selection_mask, double level, gint nmax, gint width) ;
void estimate_region(fftw_real data[], int firstbad, int lastbad, int siglen) ;
#ifndef TRUNCATE_OLD
void resize_sample_buffer(struct sound_prefs *p);
#endif
void fill_sample_buffer(struct sound_prefs *p) ;
void filter_audio(struct sound_prefs *p, long first, long last, int channel_mask) ;
int  filter_dialog(struct sound_prefs current, struct view *) ;
void flush_wavefile_data(void) ;
void get_region_of_interest(long *first, long *last, struct view *v) ;
int get_sample_buffer(struct sample_block **result) ;
void get_sample_stats(struct sample_display_block *result, long first, long last, double blocks_per_pixel) ;
char *get_undo_msg(void) ;
int get_undo_levels(void) ;
int gwc_dialog_run(GtkDialog *);
void gwc_window_set_title(char *title) ;
double high_pass_filter(fftw_real x[], int N) ;
void info(char *msg) ;
int is_valid_audio_file(char *filename) ;
void load_denoise_preferences(void) ;
void load_mp3_encoding_preferences(void);
void load_mp3_simple_encoding_preferences(void);
void load_ogg_encoding_preferences(void);
int load_sample_block_data(struct sound_prefs *p) ;
void main_redraw(int cursor_flag, int redraw_data) ;
void mark_songs(GtkWidget * widget, gpointer data) ;
/* void move_delete_song_marker(int delete) ; */
void adjust_song_marker_positions(long start, long delta);
void adjust_marker_positions(long start, long delta);
void move_song_marker(void) ;
void delete_song_marker(void) ;
void select_song_marker(void) ;
struct sound_prefs open_wavefile(char *filename, struct view *v) ;
void pinknoise(struct sound_prefs *p, long first, long last, int channel_mask) ;
int  pinknoise_dialog(struct sound_prefs current, struct view *) ;
int  play_wavefile_data(long first, long last) ;
void pop_status_text(void) ;
int print_noise_sample(struct sound_prefs *pPrefs, struct denoise_prefs *pDnprefs, long noise_start, long noise_end) ;
int  process_audio(gfloat *pL, gfloat *pR) ;
void push_status_text(gchar *msg) ;
int read_fft_real_wavefile_data(fftw_real left[], fftw_real right[], long first, long last) ;
int read_float_wavefile_data(float left[], float right[], long first, long last) ;
int  read_raw_wavefile_data(char buf[], long first, long last) ;
void read_sample_block(struct sample_block *sb, struct sound_prefs *p, long block_number) ;
int  read_wavefile_data(long left[], long right[], long first, long last) ;
void redraw(struct view *v, struct sound_prefs *p, GtkWidget *canvas, int cursor_flag, int redraw_data, int sonogram_flag) ;
void rescan_sample_buffer(struct sound_prefs *p) ;
void reverb_audio(struct sound_prefs *p, long first, long last, int channel_mask) ;
int  reverb_dialog(struct sound_prefs current, struct view *) ;
int  sample_to_pixel(struct view *v, long sample) ;
void save_cdrdao_tocs(GtkWidget * widget, gpointer data) ;
void save_cdrdao_tocp(GtkWidget * widget, gpointer data) ;
void save_denoise_preferences(void) ;
void save_ogg_encoding_preferences(void);
void save_mp3_encoding_preferences(void);
void save_mp3_simple_encoding_preferences(void);
int partial_save_encoding_mp3(char *filename,char *filename_new,long first_sample,long total_samples);
int save_encoding_ogg(char *filename,char *filename_new);
void save_preferences(void);
void save_sample_block_data(struct sound_prefs *p) ;
void save_as_wavfile(char *filename_new, long first_sample, long last_sample) ;
void save_selection_as_wavfile(char *filename_new, struct view *v) ;
void save_selection_as_encoded(int fmt,char *oldname,char *filename_new, struct view *v, char *trackname) ;
int  encode(int fmt,char *origname, char *newname,long start,long length, char *trackname);
int start_encode( int mode,char *newfilename,long start,long length, char *origfilename);

int save_undo_data(long first_sample, long last_sample, struct sound_prefs *p, int status_update_flag) ;
#ifndef TRUNCATE_OLD
int save_undo_data_remove(long first_sample, long last_sample, int status_update_flag);
int save_undo_data_insert(long first_sample, long last_sample, int status_update_flag);
#endif
void seek_to_audio_position(long playback_position) ;
void set_misc_preferences(GtkWidget * widget, gpointer data) ;
void set_mp3_simple_encoding_preferences(GtkWidget * widget, gpointer data);
void set_mp3_encoding_preferences(GtkWidget * widget, gpointer data);
void set_ogg_encoding_preferences(GtkWidget * widget, gpointer data);
long set_playback_cursor_position(struct view *v, long msec_per_visual_frame) ;
void set_status_text(gchar *msg) ;
void simple_amplify_audio(struct sound_prefs *p, long first, long last, int channel_mask, double amount) ;
void sndfile_truncate(long total_samples) ;
struct sound_prefs sound_pref_dialog(struct sound_prefs current) ;
void stats(double x[], int n, double *pMean, double *pStderr, double *pVar, double *pCv, double *pStddev) ;
void resample_audio_data(struct sound_prefs *p, long first, long last) ;
int  start_recording(char *input_device, char *filename) ;
long start_playback(char *output_device, struct view *v, struct sound_prefs *p, double seconds_per_block, double seconds_to_preload) ;
int  start_monitor(char *input_device) ;
void stop_playback(int force) ;
void stop_recording(void) ;
int start_save_undo(char *undo_msg, struct view *v) ;
/* void sum_sample_block(struct sample_block *sb, double left[], double right[], long n) ; */
void sum_sample_block(struct sample_block *sb, fftw_real left[], fftw_real right[], long n) ;
int undo(struct view *v, struct sound_prefs *p) ;
void undo_purge(void) ;
void update_status_bar(gfloat percentage, gfloat min_delta, gboolean init_flag) ;
int yesno(char *) ;
int yesnocancel(char *) ;
void warning(char *) ;
int write_fft_real_wavefile_data(fftw_real left[], fftw_real right[], long first, long last) ;
int write_float_wavefile_data(float left[], float right[], long first, long last) ;
int  write_raw_wavefile_data(char buf[], long first, long last) ;
int  write_wavefile_data(long left[], long right[], long first, long last) ;
int write_wavefile_data_to_fd(long left[], long right[], long first, long last, int fd) ;

/* bj 9/6/03 added */
#ifdef TRUNCATE_OLD
void truncate_wavfile(struct view *v);
#endif
void start_timer(void);
void stop_timer(char *message);
void batch_normalize(struct sound_prefs *p, long first , long last, int channel_mask);
