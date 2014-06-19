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

    $Id: file_io.h,v 1.3 2004/06/11 22:28:57 tszilagyi Exp $
*/


#ifndef _tap_reverb_file_io_h
#define _tap_reverb_file_io_h


typedef struct t_revtype {
        char name[MAXLEN];
        unsigned long num_combs;
        unsigned long num_allps;
        float combs_data[3 * MAX_COMBS];
        float allps_data[2 * MAX_ALLPS];
        float bandps_lo;
        float bandps_hi;
        struct t_revtype * next;
} REVTYPE;


REVTYPE * parse_reverb_input_file(void);
void list_revtypes(REVTYPE * root);
REVTYPE * get_revtype_by_name(REVTYPE * root, const char * name);
REVTYPE * get_next_revtype(REVTYPE * root);
REVTYPE * get_revroot(void);


#endif /* _tap_reverb_file_io_h */
