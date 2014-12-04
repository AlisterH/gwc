/*****************************************************************************
*   Gnome Wave Cleaner Version 0.19
*   Copyright (C) 2003 Jeffrey J. Welty
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

/* soundfile.c some functions to manipulate the sound file ...frank 4.10.03 */

#include <stdlib.h>
#include <stdio.h>
#include <sndfile.h>
#include "soundfile.h"
#include "gwc.h"

extern SNDFILE *sndfile;
extern SF_INFO sfinfo;

static void perr(char *text)
{
    int err = sf_error(sndfile);
    puts("##########################################################");
    puts(text);
    puts(sf_error_number(err));
}

long soundfile_count_samples_in_file(char *filename)
{
    sf_count_t sample_count = -1;
    SNDFILE *sndfile_new;
    SF_INFO sfinfo_new = sfinfo;

    sfinfo_new.format = 0;

    sndfile_new = sf_open(filename, SFM_READ, &sfinfo_new);
    if (sndfile_new != NULL) {
        sample_count = sf_seek(sndfile_new, 0, SEEK_END);
        sf_close(sndfile_new);
    }

/*     printf("soundfile_count_samples_in_file: %lld\n", sample_count); */

    return sample_count;
}

int soundfile_save_file(char *filename, long lpos, long lsample_count,
                        int status_info)
{
    sf_count_t pos = lpos;
    sf_count_t sample_count = lsample_count;
    int rc = -1;

    if (sf_seek(sndfile, pos, SEEK_SET|SFM_READ) == pos) {
        int channels = sfinfo.channels;
        sf_count_t buffer_size = SBW*1000;
        int *buffer = calloc(channels*buffer_size, sizeof(int));

        if (buffer != NULL) {
            SNDFILE *sndfile_new;
            SF_INFO sfinfo_new = sfinfo;

            sndfile_new = sf_open(filename, SFM_WRITE, &sfinfo_new);
            if (sndfile_new != NULL) {
                sf_count_t processed = 0;
                sf_count_t read_size;

                while (processed < sample_count) {
                    if (buffer_size > sample_count - processed)
                        buffer_size = sample_count - processed;

                    read_size = sf_readf_int(sndfile, buffer, buffer_size);
                    if (read_size > 0)
                        sf_writef_int(sndfile_new, buffer, read_size);

                    processed += buffer_size;
                    if (status_info)
                        update_progress_bar((gfloat)processed/sample_count,
                                          PROGRESS_UPDATE_INTERVAL, FALSE);
                }

                sf_close(sndfile_new);
                rc = 0;
            }
            free(buffer);
        }
    }
    return rc;
}

int soundfile_load_file(char *filename, long lpos, long lsample_count,
                        int status_info)
{
    sf_count_t pos = lpos;
    sf_count_t sample_count = lsample_count;
    int rc = -1;

    if (sf_seek(sndfile, pos, SEEK_SET|SFM_WRITE) == pos) {
        int channels = sfinfo.channels;
        sf_count_t buffer_size = SBW*1000;
        int *buffer = calloc(channels*buffer_size, sizeof(int));

        if (buffer != NULL) {
            SNDFILE *sndfile_new;
            SF_INFO sfinfo_new = sfinfo;

            sfinfo_new.format = 0;
            sndfile_new = sf_open(filename, SFM_READ, &sfinfo_new);
            if (sndfile_new != NULL) {
                sf_count_t processed = 0;
                sf_count_t read_size;

                if (sample_count > sfinfo_new.frames)
                    sample_count = sfinfo_new.frames;

                while (processed < sample_count) {
                    if (buffer_size > sample_count - processed)
                        buffer_size = sample_count - processed;

                    read_size = sf_readf_int(sndfile_new, buffer, buffer_size);
                    if (read_size > 0)
                        sf_writef_int(sndfile, buffer, read_size);

                    processed += buffer_size;
                    if (status_info)
                        update_progress_bar((gfloat)processed/sample_count,
                                          PROGRESS_UPDATE_INTERVAL, FALSE);
                }

                sf_close(sndfile_new);
                rc = 0;
/*                remove(filename); */
            }
            free(buffer);
        }
    }
    return rc;
}

static void adjust_all_marker_positions(long start, long delta)
{
    adjust_song_marker_positions(start, delta);
    adjust_marker_positions(start, delta);
}

long soundfile_count_samples(void)
{
    sf_count_t curr = sf_seek(sndfile, 0, SEEK_SET|SFM_READ);
    sf_count_t rc   = sf_seek(sndfile, 0, SEEK_END|SFM_READ);
    sf_seek(sndfile, curr, SEEK_SET|SFM_READ);
    return rc;
}

/* simply write 'sample_count' silence-samples at 'pos'
 * (overwrite existing samples)
 */
static int write_silence(sf_count_t pos, sf_count_t sample_count)
{
    int rc = 0;
    int channels = sfinfo.channels;
    sf_count_t buffer_size = SBW*100;
    int *buffer = calloc(channels*buffer_size, sizeof(int));

/*     printf("write_silence: pos=%lld, sample_count=%lld\n", */
/*            pos, sample_count); */

    if (buffer == NULL) {
        warning("Out of Memory");
        return -1;
    }

    /* go to position ... */
    if (sf_seek(sndfile, pos, SEEK_SET|SFM_WRITE) < 0) {
        perr("write_silence: sf_seek write pointer");
        warning("Libsndfile reports write pointer seek error in audio file");
        rc = -1;
    }
    else {
        /* ... and write sample_count silence */
        while (sample_count > 0) {
            if (sample_count - buffer_size < 0)
                buffer_size = sample_count;

            if (sf_writef_int(sndfile, buffer, buffer_size) != buffer_size) {
                perr("write_silence: sf_writef_int");
                warning("Libsndfile reports write error in audio file");
                rc = -1;
                break;
            }
            sample_count -= buffer_size;
        }
    }

    free(buffer);
/*     printf("write_silence: rc=%d\n", rc); */
    return rc;
}

/* append 'sample_count' samples and shift all samples from 'first_pos'
 * to end of file (this creates 'sample_count' samples at 'first_pos')
 */
int soundfile_shift_samples_right(long lfirst_pos, long lsample_count,
                                  int status_info)
{
    sf_count_t first_pos = lfirst_pos;
    sf_count_t sample_count = lsample_count;
    sf_count_t end_pos = sf_seek(sndfile, 0, SEEK_END|SFM_READ);

    if (sample_count <= 0) {
        puts("soundfile_shift_samples_right: no samples to move");
        return 0;
    }

/*     printf("soundfile_shift_samples_right: " */
/*            "first_pos=%lld, shift=%lld, samples in file=%lld\n", */
/*            first_pos, sample_count, end_pos); */

    if (first_pos < end_pos) {
        sf_count_t processed = 0;
        sf_count_t read_pos, write_pos;
        int channels = sfinfo.channels;
#define TMPBUFSIZE     (4*SBW*1000)
        int buffer[TMPBUFSIZE];
        sf_count_t buffer_size = TMPBUFSIZE/channels;

        /* go to end position and append enough space for the new data */
        if (write_silence(end_pos, sample_count) < 0) {
            return -1;
        }

        /* loop reading-writing from end */
        write_pos = end_pos + sample_count;
        read_pos  = end_pos;

        while (read_pos > first_pos) {
            if (buffer_size > read_pos - first_pos)
                buffer_size = read_pos - first_pos;

            write_pos -= buffer_size;
            read_pos  -= buffer_size;

/*             printf("soundfile_shift_samples_right: " */
/*                    "read_pos=%lld, write_pos=%lld, diff=%lld, buffer_size=%lld\n", */
/*                    read_pos, write_pos, write_pos-read_pos, buffer_size); */

            /* start position for reading */
            if (sf_seek(sndfile, read_pos, SEEK_SET|SFM_READ) < 0) {
                perr("soundfile_shift_samples_right: sf_seek read pointer");
                warning("Libsndfile reports read pointer seek error in audio file");
                return -1;
            }
            if (sf_readf_int(sndfile, buffer, buffer_size) != buffer_size) {
                perr("soundfile_shift_samples_right: sf_readf_int");
                warning("Libsndfile reports read error in audio file");
                return -1;
            }

            /* start position for writing */
            if (sf_seek(sndfile, write_pos, SEEK_SET|SFM_WRITE) < 0) {
                perr("soundfile_shift_samples_right: sf_seek write pointer");
                warning("Libsndfile reports write pointer seek error in audio file");
                return -1;
            }
            if (sf_writef_int(sndfile, buffer, buffer_size) != buffer_size) {
                perr("soundfile_shift_samples_right: sf_writef_int");
                warning("Libsndfile reports write error in audio file");
                return -1;
            }

            processed += buffer_size;
            if (status_info)
                update_progress_bar((gfloat)processed/(end_pos-first_pos),
                                  PROGRESS_UPDATE_INTERVAL, FALSE);

/*             printf("soundfile_shift_samples_right: processed=%lld (%f.2)\n", */
/*                    processed, (gfloat)processed/(end_pos-first_pos)); */
        }
    }

/*     printf("soundfile_shift_samples_right: samples in file=%lld\n", */
/*            sf_seek(sndfile, 0, SEEK_END|SFM_READ)); */

    adjust_all_marker_positions(first_pos, sample_count);
    return 0;
}


/* shift all samples from 'first_pos' to begin of file
 * (this removes 'sample_count' samples at end of file)
 */
int soundfile_shift_samples_left(long lfirst_pos, long lsample_count,
                                 int status_info)
{
    /* if 'last_pos' is equal 'end_pos' simply truncate the end
     * else move sample i from 'last_pos' to the 'first_pos' position
     * (for all i between 'last_pos' and end of file)
     * and then truncate 'last_pos - first_pos' samples at the end.
     */
    sf_count_t first_pos = lfirst_pos;
    sf_count_t sample_count = lsample_count;
    sf_count_t end_pos = sf_seek(sndfile, 0, SEEK_END|SFM_READ);
    sf_count_t last_pos = first_pos + sample_count;

    if (sample_count <= 0) {
        puts("soundfile_shift_samples_left: no samples to move");
        return 0;
    }

/*     printf("soundfile_shift_samples_left: " */
/*            "first_pos=%lld, shift=%lld, samples in file=%lld\n", */
/*            first_pos, sample_count, end_pos); */

    if (last_pos < end_pos)
    {
        sf_count_t processed = 0;
        sf_count_t buffer_size;
        int channels = sfinfo.channels;
#define TMPBUFSIZE     (4*SBW*1000)
        int buffer[TMPBUFSIZE];

        /* start position for writing */
        if (sf_seek(sndfile, first_pos, SEEK_SET|SFM_WRITE) < 0)
        {
            perr("soundfile_shift_samples_left: sf_seek write pointer");
            warning("Libsndfile reports write pointer seek error in audio file");
            return -1;
        }

        /* start position for reading */
        if (sf_seek(sndfile, last_pos, SEEK_SET|SFM_READ) < 0)
        {
            perr("soundfile_shift_samples_left: sf_seek read pointer");
            warning("Libsndfile reports read pointer seek error in audio file");
            return -1;
        }

        /* loop to end of file */
        buffer_size = TMPBUFSIZE/channels;

        while ((buffer_size = sf_readf_int(sndfile, buffer, buffer_size)) > 0)
        {
            if (sf_writef_int(sndfile, buffer, buffer_size) != buffer_size) {
                perr("soundfile_shift_samples_left: sf_writef_int");
                warning("Libsndfile reports write error in audio file");
                return -1;
            }

            processed += buffer_size;

            if (status_info)
                update_progress_bar((gfloat)processed/(end_pos-first_pos-sample_count),
                                  PROGRESS_UPDATE_INTERVAL, FALSE);

/*             printf("soundfile_shift_samples_left: processed=%lld (%f.2)\n", */
/*                    processed, (gfloat)processed/(end_pos-first_pos-sample_count)); */
        }
    }

    /* truncate file size for '-sample_count' samples */
    end_pos -= sample_count;

    if (sf_command(sndfile, SFC_FILE_TRUNCATE, &end_pos, sizeof(end_pos)))
    {
        perr("soundfile_shift_samples_left: sf_command");
        warning("Libsndfile reports truncation of audio file failed");
        return -1;
    }

/*     printf("soundfile_shift_samples_left: samples in file=%lld\n", */
/*            sf_seek(sndfile, 0, SEEK_END|SFM_READ)); */

    adjust_all_marker_positions(first_pos, -sample_count);
    return 0;
}


/* insert 'sample_count' samples at sample 'insert_pos'
 * to the soundfile.
 * 'insert_pos' == -1 means append to end of file.
 */
int soundfile_insert_samples(long linsert_pos, long lsample_count,
                             int *sample_data, int status_info)
{
    sf_count_t insert_pos = linsert_pos;
    sf_count_t sample_count = lsample_count;
    sf_count_t end_pos = sf_seek(sndfile, 0, SEEK_END|SFM_READ);

    if (insert_pos < 0 || insert_pos > end_pos)
        insert_pos = end_pos;

    if (soundfile_shift_samples_right(insert_pos, sample_count, status_info) < 0) {
        return -1;
    }

    /* go to insert position 'insert_pos'... */
    if (sf_seek(sndfile, insert_pos, SEEK_SET|SFM_WRITE) < 0) {
        perr("soundfile_insert_samples: sf_seek write pointer");
        warning("Libsndfile reports write pointer seek error in audio file");
        return -1;
    }
    /* ...and insert new data */
    if (sf_writef_int(sndfile, sample_data, sample_count) != sample_count) {
        perr("soundfile_insert_samples: append with sf_writef_int");
        warning("Libsndfile reports write error in audio file");
        return -1;
    }

    return 0;
}

int soundfile_insert_silence(long linsert_pos, long lsample_count,
                             int status_info)
{
    sf_count_t insert_pos = linsert_pos;
    sf_count_t sample_count = lsample_count;
    sf_count_t end_pos = sf_seek(sndfile, 0, SEEK_END|SFM_READ);
    int rc = 0;

    if (insert_pos < 0 || insert_pos > end_pos)
        insert_pos = end_pos;

    if (soundfile_shift_samples_right(insert_pos, sample_count, status_info) < 0) {
        rc = -1;
    }
    else if (write_silence(insert_pos, sample_count) < 0) {
        rc = -1;
    }

    return rc;
}


/* remove 'sample_count' samples from 'first_pos' sample.
 * 'sample_count' == -1 means to end of file.
 */
int soundfile_remove_samples(long lfirst_pos, long lsample_count,
                             int status_info)
{
    sf_count_t first_pos = lfirst_pos;
    sf_count_t sample_count = lsample_count;
    sf_count_t end_pos = sf_seek(sndfile, 0, SEEK_END|SFM_READ);

    if (first_pos < 0 || first_pos > end_pos)
        first_pos = end_pos;

    if (sample_count < 0 || first_pos + sample_count - 1 > end_pos)
        sample_count = end_pos - first_pos + 1;

    return soundfile_shift_samples_left(first_pos, sample_count, status_info);
}
