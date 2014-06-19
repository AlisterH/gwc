
Mp3_File mp3file ;
struct Mp3_File {
    int rate ;
    int channels ;
    long position ; /* current, or rather last output sample number */
}

void mp3_clear(Mp3File *) ; /* closes everything related to an open mp3file */
int mp3_open(FILE *fp_mp3, Mp3_File *mp3file) ; /* opens and mp3 file, returns 1 on success, -1 on failure, detects rate and #channels */
long mp3_pcm_total(Mp3File *) ; 
int mp3_pcm_seek(Mp3File *, int sample_number) ;
long mp3_read(Mp3_File *, char *buf, int bufsize) ;
