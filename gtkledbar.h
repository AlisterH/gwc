/*
 * $Id: gtkledbar.h,v 1.1.1.1 2002/09/08 04:03:51 welty Exp $
 * GTKEXT - Extensions to The GIMP Toolkit
 * Copyright (C) 1998 Gregory McLean
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Cambridge, MA 
 * 02139, USA.
 *
 * Eye candy!
 */
#ifndef __GTKLEDBAR_H__
#define __GTKLEDBAR_H__

#include <gdk/gdk.h>
#include <gtk/gtkvbox.h>
#include "gtkled.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LEDBAR(obj)          GTK_CHECK_CAST (obj, led_bar_get_type (), LedBar)
#define LEDBAR_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, led_bar_get_type (), LedBarClass)
#define IS_LEDBAR(obj)       GTK_CHECK_TYPE (obj, led_bar_get_type ())

#define MAX_SEGMENTS         40

typedef struct _LedBar       LedBar;
typedef struct _LedBarClass  LedBarClass;

struct _LedBar
{
  GtkVBox   vbox;

  GtkWidget *segments[MAX_SEGMENTS];
  gint      num_segments;         /* How many segmanets in this bar */
  gint      lit_segments;         /* last segment that is lit */
  gint      seq_segment;          /* which led in the sequence we are at */
  gint      seq_dir;              /* direction */
  gint      orientation;          /* horizontal (0), or vertical (1) */
};

struct _LedBarClass
{
  GtkVBoxClass   parent_class;
};

guint         led_bar_get_type            (void);
GtkWidget*    led_bar_new                 (gint       segments,
					   gint       orientation);
gint          led_bar_get_num_segments    (GtkWidget  *bar);
void          led_bar_light_segments      (GtkWidget  *bar,
					   gint       num);
void          led_bar_unlight_segments    (GtkWidget  *bar,
					   gint       num);
void          led_bar_light_segment       (GtkWidget  *bar,
					   gint       segment);
void          led_bar_unlight_segment     (GtkWidget  *bar,
					   gint       segment);
void          led_bar_light_percent       (GtkWidget  *bar,
					   gfloat     percent);
void          led_bar_sequence_step       (GtkWidget  *bar);
void          led_bar_clear               (GtkWidget  *bar);

#ifdef __cplusplus
}
#endif

#endif

/* EOF */

