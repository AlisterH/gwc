/*                                                     -*- linux-c -*-
    Copyright (C) 2004 Tom Szilagyi

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    $Id: sound.c,v 1.5 2004/06/16 09:52:18 tszilagyi Exp $
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

#include "tap_reverb_common.h"
#include "tap_reverb_file_io.h"
#include "tap_reverb.h"


/* ***** VERY IMPORTANT! *****
 *
 * If you enable this, the program will use float arithmetics in DSP
 * calculations.  This usually yields lower average CPU usage, but
 * occasionaly may result in high CPU peaks which cause trouble to you
 * and your JACK server.  The default is to use fixpoint arithmetics
 * (with the following #define commented out).  But (depending on the
 * processor on which you run the code) you may find floating point
 * mode usable.
 */
/* #define REVERBED_CALC_FLOAT */


REVTYPE * curr = NULL ;
REVTYPE * reverb_root = NULL ;

/* magic numbers */
#define BANDPASS_BWIDTH   1.5f
#define FREQ_RESP_BWIDTH  3.0f
#define ENH_STEREO_RATIO  0.998f
/* compensation ratio of freq_resp in fb_gain calc */
#define FR_R_COMP         0.75f


#ifndef M_PI
#define M_PI 3.14159265358979323846264338327
#endif


#define db2lin(x) ((x) > -90.0f ? powf(10.0f, (x) * 0.05f) : 0.0f)
#define LN_2_2 0.34657359f
#define LIMIT(v,l,u) ((v)<(l)?(l):((v)>(u)?(u):(v)))

/*  #define REVERB_INPUT_IS_LONGS  */


#ifdef REVERBED_CALC_FLOAT
    /* ultra-aggressive denormalization */
    #define DENORM(x) (((unsigned char)(((*(unsigned int*)&(x))&0x7f800000)>>23))<103)?0.0f:(x)
    typedef float rev_t;
#else
#ifdef REVERB_INPUT_IS_LONGS
    /* coefficient for float to sample (signed int) conversion */
    /* this allows for about 60 dB headroom above 0dB, if 0 dB is equivalent to 1.0f */
    /* As 2^31 equals more than 180 dB, about 120 dB dynamics remains below 0 dB */
    #define F2S 65
#else
    /* coefficient for float to sample (signed int) conversion */
    /* this allows for about 60 dB headroom above 0dB, if 0 dB is equivalent to 1.0f */
    /* As 2^31 equals more than 180 dB, about 120 dB dynamics remains below 0 dB */
    #define F2S 2147483
#endif
typedef signed int rev_t;
#endif


typedef struct {
        float a1;
        float a2;
        float b0;
        float b1;
        float b2;
        rev_t x1;
        rev_t x2;
        rev_t y1;
        rev_t y2;
} biquad;

typedef struct {
	float feedback;
	float fb_gain;
	float freq_resp;
	rev_t ringbuffer[(int)MAX_COMB_DELAY * MAX_SAMPLERATE / 1000];
	unsigned long buflen;
	unsigned long buffer_pos;
	biquad filter;
	rev_t last_out;
} COMB_FILTER;

typedef struct {
	float feedback;
	float fb_gain;
	float in_gain;
	rev_t ringbuffer[(int)MAX_ALLP_DELAY * MAX_SAMPLERATE / 1000];
	unsigned long buflen;
	unsigned long buffer_pos;
	rev_t last_out;
} ALLP_FILTER;

/* data of the running instance */
unsigned long num_combs; /* total number of comb filters */
unsigned long num_allps; /* total number of allpass filters */
COMB_FILTER combs[2 * MAX_COMBS];
ALLP_FILTER allps[2 * MAX_ALLPS];
biquad low_pass[2];
biquad high_pass[2];
float tap_decay = 2500.0f;
float drylevel = 0.0f;
float wetlevel = 0.0f;
int combs_en = 1; /* on/off */
int allps_en = 1; /* on/off */
int bandps_en = 1; /* on/off */
int stereo_en = 1; /* on/off */
int bypass = 0; /* on/off */

int changed_settings = 0;

/* additional data for the IR calculating instance */
COMB_FILTER combs_IR[MAX_COMBS];
ALLP_FILTER allps_IR[MAX_ALLPS];
biquad low_pass_IR;
biquad high_pass_IR;
unsigned long sample_rate;

void reverb_setup(long rate, double decay_d, double wet_d, double dry_d, char *name)
{
    tap_decay = decay_d ;
    wetlevel = wet_d ;
    drylevel = dry_d ;
    changed_settings = 1 ;
    sample_rate = rate ;

    if(reverb_root == NULL) {
	reverb_root = parse_reverb_input_file() ;
    }

    curr = get_revtype_by_name(reverb_root, name) ;
    reverb_init() ;
}


/* push a sample into a ringbuffer and return the sample falling out */
static inline
rev_t
push_buffer(rev_t insample, rev_t * buffer,
            unsigned long buflen, unsigned long * pos) {

        rev_t outsample;
	
        outsample = buffer[*pos];
        buffer[(*pos)++] = insample;
	
        if (*pos >= buflen)
                *pos = 0;

        return outsample;
}


/* read a value from a ringbuffer. */
static inline
rev_t
read_buffer(rev_t * buffer, unsigned long buflen,
            unsigned long pos, unsigned long n) {
	
        while (n + pos >= buflen)
                n -= buflen;
        return buffer[n + pos];
}


/* overwrites a value in a ringbuffer, but pos stays the same. */
static inline
void
write_buffer(rev_t insample, rev_t * buffer, unsigned long buflen,
             unsigned long pos, unsigned long n) {
	
        while (n + pos >= buflen)
                n -= buflen;
        buffer[n + pos] = insample;
}



static inline
void
biquad_init(biquad *f) {
	
        f->x1 = 0.0f;
        f->x2 = 0.0f;
        f->y1 = 0.0f;
        f->y2 = 0.0f;
}


static inline
void
lp_set_params(biquad *f, float fc, float bw, float fs) {

        float omega = 2.0 * M_PI * fc/fs;
        float sn = sin(omega);
        float cs = cos(omega);
        float alpha = sn * sinh(M_LN2 / 2.0 * bw * omega / sn);

        const float a0r = 1.0 / (1.0 + alpha);

        f->b0 = a0r * (1.0 - cs) * 0.5;
        f->b1 = a0r * (1.0 - cs);
        f->b2 = a0r * (1.0 - cs) * 0.5;
        f->a1 = a0r * (2.0 * cs);
        f->a2 = a0r * (alpha - 1.0);
}


static inline
void
hp_set_params(biquad *f, float fc, float bw, float fs) {

        float omega = 2.0 * M_PI * fc/fs;
        float sn = sin(omega);
        float cs = cos(omega);
        float alpha = sn * sinh(M_LN2 / 2.0 * bw * omega / sn);

        const float a0r = 1.0 / (1.0 + alpha);

        f->b0 = a0r * (1.0 + cs) * 0.5;
        f->b1 = a0r * -(1.0 + cs);
        f->b2 = a0r * (1.0 + cs) * 0.5;
        f->a1 = a0r * (2.0 * cs);
        f->a2 = a0r * (alpha - 1.0);
}


static inline
rev_t
biquad_run(biquad *f, rev_t x) {

        rev_t y;

        y = f->b0 * x + f->b1 * f->x1 + f->b2 * f->x2
		+ f->a1 * f->y1 + f->a2 * f->y2;
#ifdef REVERBED_CALC_FLOAT
	y = DENORM(y);
#endif
        f->x2 = f->x1;
        f->x1 = x;
        f->y2 = f->y1;
        f->y1 = y;
	
        return y;
}


/* push a sample into a comb filter and return the sample falling out */
rev_t
comb_run(rev_t insample, COMB_FILTER * comb) {
	
	rev_t outsample;
	rev_t pushin;

	pushin = comb->fb_gain * insample + biquad_run(&(comb->filter), comb->fb_gain * comb->last_out);
#ifdef REVERBED_CALC_FLOAT
	pushin = DENORM(pushin);
#endif
	outsample = push_buffer(pushin,	comb->ringbuffer, comb->buflen, &(comb->buffer_pos));

#ifdef REVERBED_CALC_FLOAT
	outsample = DENORM(outsample);
#endif
	comb->last_out = outsample;

	return outsample;
}


/* push a sample into an allpass filter and return the sample falling out */
rev_t
allp_run(rev_t insample, ALLP_FILTER * allp) {
	
	rev_t outsample;
	rev_t pushin;

	pushin = allp->in_gain * allp->fb_gain * insample + allp->fb_gain * allp->last_out;
#ifdef REVERBED_CALC_FLOAT
	pushin = DENORM(pushin);
#endif
        outsample = push_buffer(pushin, allp->ringbuffer, allp->buflen, &(allp->buffer_pos));

#ifdef REVERBED_CALC_FLOAT
	outsample = DENORM(outsample);
#endif
	allp->last_out = outsample;

	return outsample;
}


/* load data from REVTYPE*curr into the running instance */
void
load_revtype_data(void) {
	
	int i;
	
	/* load combs data */
	num_combs = 2 * curr->num_combs;
	for (i = 0; i < curr->num_combs; i++) {
		combs[2*i].buflen = curr->combs_data[3*i] * sample_rate / 1000.0f;
		combs[2*i].feedback = curr->combs_data[3*i+1];
		combs[2*i].freq_resp = LIMIT(curr->combs_data[3*i+2]
					     * powf(sample_rate / 44100.0f, 0.8f),
					     0.0f, 1.0f);
		
		combs[2*i+1].buflen = combs[2*i].buflen;
		combs[2*i+1].feedback = combs[2*i].feedback;
		combs[2*i+1].freq_resp = combs[2*i].freq_resp;

		lp_set_params(&(combs[2*i].filter),
			      2000.0f + 13000.0f * (1 - curr->combs_data[3*i+2])
			      * sample_rate / 44100.0f,
			      BANDPASS_BWIDTH, sample_rate);
		lp_set_params(&(combs[2*i+1].filter),
			      2000.0f + 13000.0f * (1 - curr->combs_data[3*i+2])
			      * sample_rate / 44100.0f,
			      BANDPASS_BWIDTH, sample_rate);
	}
	
        /* load allps data */
        num_allps = 2 * curr->num_allps;
        for (i = 0; i < curr->num_allps; i++) {
                allps[2*i].buflen = curr->allps_data[2*i] * sample_rate / 1000.0f;
                allps[2*i].feedback = curr->allps_data[2*i+1];
		
                allps[2*i+1].buflen = allps[2*i].buflen;
                allps[2*i+1].feedback = allps[2*i].feedback;
	}

        /* init bandpass filters */
        lp_set_params(&(low_pass[0]), curr->bandps_hi, BANDPASS_BWIDTH, sample_rate);
        hp_set_params(&(high_pass[0]), curr->bandps_lo, BANDPASS_BWIDTH, sample_rate);
        lp_set_params(&(low_pass[1]), curr->bandps_hi, BANDPASS_BWIDTH, sample_rate);
        hp_set_params(&(high_pass[1]), curr->bandps_lo, BANDPASS_BWIDTH, sample_rate);
}


/* compute user-input-dependent reverberator coefficients */
void
comp_coeffs(void) {
	
	int i;
	
	for (i = 0; i < num_combs / 2; i++) {
		combs[2*i].fb_gain = powf(0.001f, 1000.0f * combs[2*i].buflen
					  * (1 + FR_R_COMP * combs[2*i].freq_resp)
					  / powf(combs[2*i].feedback / 100.0f, 0.89f)
					  / tap_decay / sample_rate);
		
		combs[2*i+1].fb_gain = combs[2*i].fb_gain;
		
		if (stereo_en) {
			if (i % 2 == 0)
                                combs[2*i+1].buflen = ENH_STEREO_RATIO * combs[2*i].buflen;
                        else
                                combs[2*i].buflen = ENH_STEREO_RATIO * combs[2*i+1].buflen;
                } else {
                        if (i % 2 == 0)
                                combs[2*i+1].buflen = combs[2*i].buflen;
                        else
                                combs[2*i].buflen = combs[2*i+1].buflen;
                }
	}
	for (i = 0; i < num_allps / 2; i++) {
		allps[2*i].fb_gain = powf(0.001f, 11000.0f * allps[2*i].buflen
					  / powf(allps[2*i].feedback / 100.0f, 0.88f)
					  / tap_decay / sample_rate);
		
		allps[2*i+1].fb_gain = allps[2*i].fb_gain;
		
		allps[2*i].in_gain = -0.06f 
			/ (allps[2*i].feedback / 100.0f)
			/ powf((tap_decay + 3500.0f) / 10000.0f, 1.5f);
		
                allps[2*i+1].in_gain = allps[2*i].in_gain;
		
                if (stereo_en) {
                        if (i % 2 == 0)
                                allps[2*i+1].buflen =
                                        ENH_STEREO_RATIO * ENH_STEREO_RATIO * allps[2*i].buflen;
                        else
                                allps[2*i].buflen =
                                        ENH_STEREO_RATIO * ENH_STEREO_RATIO * allps[2*i+1].buflen;
                } else {
                        if (i % 2 == 0)
                                allps[2*i+1].buflen = allps[2*i].buflen;
                        else
				allps[2*i].buflen = allps[2*i+1].buflen;
                }
        }
}


void reverb_init(void) {
	
	unsigned long i,j;
	
	for (i = 0; i < 2 * MAX_COMBS; i++) {
		for (j = 0; j < (unsigned long)MAX_COMB_DELAY * sample_rate / 1000; j++)
			combs[i].ringbuffer[j] = 0.0f;
		combs[i].buffer_pos = 0;
		combs[i].last_out = 0.0f;
		biquad_init(&(combs[i].filter));
	}
	
	for (i = 0; i < 2 * MAX_ALLPS; i++) {
		for (j = 0; j < (unsigned long)MAX_ALLP_DELAY * sample_rate / 1000; j++)
			allps[i].ringbuffer[j] = 0.0f;
		allps[i].buffer_pos = 0;
		allps[i].last_out = 0.0f;
	}
	
	biquad_init(&(low_pass[0]));
	biquad_init(&(low_pass[1]));
	biquad_init(&(high_pass[0]));
	biquad_init(&(high_pass[1]));
}





int
reverb_process(long nframes, reverb_audio_sample_t *output_L, reverb_audio_sample_t *input_L, reverb_audio_sample_t *output_R, reverb_audio_sample_t *input_R) {

        unsigned long sample_index;
        int i;



        rev_t out_L = 0;
	rev_t out_R = 0;
	rev_t in_L = 0;
	rev_t in_R = 0;
	rev_t combs_out_L = 0;
	rev_t combs_out_R = 0;

        float dry = db2lin(drylevel);
        float wet = db2lin(wetlevel);

        if (bypass) {

		memcpy(output_L, input_L, sizeof(reverb_audio_sample_t) * nframes);
		memcpy(output_R, input_R, sizeof(reverb_audio_sample_t) * nframes);

	} else {

		if (changed_settings) {
			load_revtype_data();
			comp_coeffs();
			changed_settings = 0;
		}
		
		for (sample_index = 0; sample_index < nframes; sample_index++) {
			
#ifdef REVERBED_CALC_FLOAT
			in_L = *(input_L++);
			in_R = *(input_R++);
#else
                        in_L = (float)F2S * *(input_L++);
                        in_R = (float)F2S * *(input_R++);
#endif
			combs_out_L = in_L;
			combs_out_R = in_R;

			/* process comb filters */
			if (combs_en) {
				for (i = 0; i < num_combs / 2; i++) {
					combs_out_L += comb_run(in_L, &(combs[2*i]));
					combs_out_R += comb_run(in_R, &(combs[2*i+1]));
				}
			}
			
			/* process allpass filters */
			if (allps_en) {
				for (i = 0; i < num_allps / 2; i++) {
					combs_out_L += allp_run(combs_out_L, &(allps[2*i]));
					combs_out_R += allp_run(combs_out_R, &(allps[2*i+1]));
				}
			}
			
			/* process bandpass filters */
			if (bandps_en) {
				combs_out_L = biquad_run(&(low_pass[0]), combs_out_L);
				combs_out_L = biquad_run(&(high_pass[0]), combs_out_L);
				combs_out_R = biquad_run(&(low_pass[1]), combs_out_R);
				combs_out_R = biquad_run(&(high_pass[1]), combs_out_R);
			}
			
#ifdef REVERBED_CALC_FLOAT
			out_L = in_L * dry + combs_out_L * wet;
			out_R = in_R * dry + combs_out_R * wet;
			*(output_L++) = out_L;
			*(output_R++) = out_R;
#else
                        out_L = (float)in_L * dry + (float)combs_out_L * wet;
                        out_R = (float)in_R * dry + (float)combs_out_R * wet;
                        *(output_L++) = (float)out_L / (float)F2S;
                        *(output_R++) = (float)out_R / (float)F2S;
#endif
		}
	}		
	return 0;
}


void
process_impresp(float * data, long int nframes) {

        unsigned long sample_index;

        rev_t out = 0;
        rev_t in = 0;
        rev_t combs_out = 0;

        float * output = data;
	float * input = data;

	unsigned long i,j;


	/* make sure the running instance has current data,
	 even if due to some accident JACK is not running */
	if (changed_settings) {
		load_revtype_data();
		comp_coeffs();
		changed_settings = 0;
	}

	/* init IR calc. instance */
	for (i = 0; i < MAX_COMBS; i++) {
		for (j = 0; j < (unsigned long)MAX_COMB_DELAY * sample_rate / 1000; j++)
			combs_IR[i].ringbuffer[j] = 0.0f;
		combs_IR[i].buffer_pos = 0;
		combs_IR[i].last_out = 0.0f;
		biquad_init(&(combs_IR[i].filter));
	}
	for (i = 0; i < MAX_ALLPS; i++) {
		for (j = 0; j < (unsigned long)MAX_ALLP_DELAY * sample_rate / 1000; j++)
			allps_IR[i].ringbuffer[j] = 0.0f;
		allps_IR[i].buffer_pos = 0;
		allps_IR[i].last_out = 0.0f;
	}
	biquad_init(&low_pass_IR);
	biquad_init(&high_pass_IR);
	
	/* load parameters */
	for (i = 0; i < curr->num_combs; i++) {
		combs_IR[i].buflen = combs[2*i].buflen;
		combs_IR[i].feedback = combs[2*i].feedback;
		combs_IR[i].fb_gain = combs[2*i].fb_gain;
		combs_IR[i].freq_resp = combs[2*i].freq_resp;
		combs_IR[i].filter.a1 = combs[2*i].filter.a1;
		combs_IR[i].filter.a2 = combs[2*i].filter.a2;
		combs_IR[i].filter.b0 = combs[2*i].filter.b0;
		combs_IR[i].filter.b1 = combs[2*i].filter.b1;
		combs_IR[i].filter.b2 = combs[2*i].filter.b2;
	}
        for (i = 0; i < curr->num_allps; i++) {
                allps_IR[i].buflen = allps[2*i].buflen;
                allps_IR[i].feedback = allps[2*i].feedback;
                allps_IR[i].fb_gain = allps[2*i].fb_gain;
                allps_IR[i].in_gain = allps[2*i].in_gain;
	}

	low_pass_IR.a1 = low_pass[0].a1;
	low_pass_IR.a2 = low_pass[0].a2;
	low_pass_IR.b0 = low_pass[0].b0;
	low_pass_IR.b1 = low_pass[0].b1;
	low_pass_IR.b2 = low_pass[0].b2;

	high_pass_IR.a1 = high_pass[0].a1;
	high_pass_IR.a2 = high_pass[0].a2;
	high_pass_IR.b0 = high_pass[0].b0;
	high_pass_IR.b1 = high_pass[0].b1;
	high_pass_IR.b2 = high_pass[0].b2;

	/* process */
	for (sample_index = 0; sample_index < nframes; sample_index++) {
		
#ifdef REVERBED_CALC_FLOAT
		in = *(input++);
#else
                in = (float)F2S * *(input++);
#endif
		combs_out = in;

		if (combs_en) {
			for (i = 0; i < curr->num_combs; i++) {
				combs_out += comb_run(in, &(combs_IR[i]));
			}
		}
		
		if (allps_en) {
			for (i = 0; i < curr->num_allps; i++) {
				combs_out += allp_run(combs_out, &(allps_IR[i]));
			}
		}
		
		if (bandps_en) {
			combs_out = biquad_run(&low_pass_IR, combs_out);
			combs_out = biquad_run(&high_pass_IR, combs_out);
		}
		
		out = combs_out;
#ifdef REVERBED_CALC_FLOAT
		*(output++) = out;
#else
                *(output++) = (float)out / (float)F2S;
#endif
	}

}		
