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

    $Id: file_io.c,v 1.4 2004/06/12 13:57:24 tszilagyi Exp $
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "tap_reverb_common.h"
#include "tap_reverb_file_io.h"


extern REVTYPE * reverb_root;

REVTYPE *get_revroot() 
{

    if(reverb_root == NULL) {
	reverb_root = parse_reverb_input_file() ;
    }

    return reverb_root ;
}


/* 0: between two REVTYPE records
 * 1: read REVTYPE keyword, name follows on next line
 * 2: read name, before COMBS keyword
 * 3: read COMBS keyword, lines for COMBS follow
 * 4: read ALLPS keyword, lines for ALLPS follow
 * 5: read BANDPS_LO keyword, next line contains BANDPS_LO data
 * 6: read BANDPS_HI keyword, next line contains BANDPS_HI data
 */
int parser_state = 0;


float convf(char * s) {

	float val, pow;
	int i, sign;

	for (i = 0; s[i] == ' ' || s[i] == '\n' || s[i] == '\t'; i++);
	sign = 1;
	if (s[i] == '+' || s[i] == '-')
		sign = (s[i++] == '+') ? 1 : -1;
	for (val = 0; s[i] >= '0' && s[i] <= '9'; i++)
		val = 10 * val + s[i] - '0';
	if ((s[i] == '.') || (s[i] == ','))
		i++;
	for (pow = 1; s[i] >= '0' && s[i] <= '9'; i++) {
		val = 10 * val + s[i] - '0';
		pow *= 10;
	}
	return(sign * val / pow);
}

#include "reverb_settings.h"

int builtin_ptr = 0 ;

#define FROM_BUILTIN 0x01
#define FROM_FILE 0x02

int reverb_data_format = FROM_FILE ;

int reverb_fgetc(FILE *infile)
{
    if(reverb_data_format == FROM_BUILTIN) {
	int c ;

	if(builtin_ptr == sizeof(reverb_default_settings))
	    return EOF ;

	c = reverb_default_settings[builtin_ptr] ;

	builtin_ptr++ ;
	return c ;

    } else {
	return fgetc(infile) ;
    }
}



REVTYPE *
parse_reverb_input_file(void) {

	char * home;
	char path[MAXLEN];
	FILE * infile;
	char line[1024];
	int c, i = 0;
	char str1[MAXLEN];
	char str2[MAXLEN];
	char str3[MAXLEN];
	int num_combs = 0;
	int num_allps = 0;
	REVTYPE * root = NULL;
	REVTYPE * item = NULL;
	REVTYPE * prev = NULL;

	
	if (!(home = getenv("HOME")))
		home = ".";

	sprintf(path, "%s/%s", home, NAME_REVERBED);

	if ((infile = fopen(path,"rt")) == NULL) {
		reverb_data_format = FROM_BUILTIN ;
	}

	if ((root = malloc(sizeof(REVTYPE))) == NULL) {
		fprintf(stderr, "file_io.c: couldn't alloc mem for root item\n");
		return(NULL);
	}

	root->next = NULL;
	root->name[0] = '\0';

	/* Here we read the whole file and fill up our list. */

	while ((c = reverb_fgetc(infile)) != EOF) {
		if (c != '\n')
			line[i++] = c;
		else {
			line[i] = '\0';
			i = 0;
			
			switch(parser_state) {

			case 0:
				if (strcmp(line, "REVTYPE") == 0) {
					if ((item = malloc(sizeof(REVTYPE))) == NULL) {
						fprintf(stderr, "tap_reverb_file_io.c: malloc failed.\n");
						return(NULL);
					}
					item->next = NULL;

					if (root->next == NULL)
						root->next = item;
					else
						prev->next = item;
					prev = item;

					parser_state = 1;
					num_allps = 0;
				}
				break;

			case 1:
				strcpy(item->name, line);
				parser_state = 2;
				break;

			case 2:
				if (strcmp(line, "COMBS") == 0) {
					num_combs = 0;
					parser_state = 3;
				}
				break;
			case 3:
				if (strcmp(line, "ALLPS") == 0) {
					num_allps = 0;
					item->num_combs = num_combs;
					parser_state = 4;
				} else {
					sscanf(line, "%s %s %s", str1, str2, str3);
					item->combs_data[3 * num_combs] = 1000.0f * convf(str1);
					item->combs_data[3 * num_combs + 1] = convf(str2);
					item->combs_data[3 * num_combs + 2] = convf(str3);
					num_combs++;
				}
				break;
			case 4:
				if (strcmp(line, "BANDPS_LO") == 0) {
					item->num_allps = num_allps;
					parser_state = 5;
				} else {
					sscanf(line, "%s %s", str1, str2);
					item->allps_data[2 * num_allps] = 1000.0f * convf(str1);
					item->allps_data[2 * num_allps + 1] = convf(str2);
					num_allps++;
				}
				break;
			case 5: 
				if (strcmp(line, "BANDPS_HI") == 0) {
					parser_state = 6;
				} else {
					item->num_allps = num_allps;
					sscanf(line, "%s", str1);
					item->bandps_lo = convf(str1);
				}
				break;
			case 6:
				sscanf(line, "%s", str1);
				item->bandps_hi = convf(str1);
				parser_state = 0;
				break;
			}

		}
	}

	if(infile != NULL)
	    fclose(infile);

	return(root);
}



REVTYPE *
get_revtype_by_name(REVTYPE * root, const char * name) {

	REVTYPE * item = root;

	if (item->next != NULL)
		item = item->next;
	else
		return NULL;

	while (item != NULL) {
		if (strcmp(name, item->name) == 0)
			return item;
		item = item->next;
	}
	return NULL;
}

REVTYPE * get_next_revtype(REVTYPE *root)
{

	REVTYPE * item = root;

	if (item->next != NULL)
		item = item->next;
	else
		return NULL;

	return item ;
}
