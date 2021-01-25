#define NULL_FMT -1
#define OGG_FMT 1000
#define MP3_FMT 2000
#define MP3_SIMPLE_FMT 3000

struct encoding_prefs {
	/* MP3 */
        int mp3_br_mode;
        char mp3_bitrate[6];
        char mp3_quality_level[2];
        int mp3presets;

	/*  Lame is mmx enabled? */
	int mp3_lame_mmx_enabled;
	/* Advanced MP3 settings */
        int mp3_mmx;
        int mp3_sse;
        int mp3_threednow;
	int mp3_copyrighted;
	int mp3_add_crc;
	int mp3_strict_iso;
	int mp3_nofilters;
	int mp3_use_lowpass;
	int mp3_use_highpass;
	char mp3_lowpass_freq[6];
	char mp3_highpass_freq[6];
	char mp3loc[256]; /* Full path and executable for mp3 encoder */
	char artist[256];
	char album[256];

	/* OGG Vorbis */
	char ogg_quality_level[7]; /*  6 characters */
	char oggloc[256]; /* Full path and executable for ogg encoder */
	int ogg_downmix;
	char ogg_bitrate[6];
        char ogg_maxbitrate[6];
        char ogg_minbitrate[6];
	int ogg_encopt; /* use Managed, nominal Bitrate, or Quality Level , or none */
        char ogg_resample[6]; /*  Resample at new rate  */
	/* Advanced ogg options */
	char  ogg_lowpass_frequency[6];
	char  ogg_bitrate_average_window[6];
	int ogg_useresample;
	int ogg_useadvlowpass;
	int ogg_useadvbravgwindow;
	
};

