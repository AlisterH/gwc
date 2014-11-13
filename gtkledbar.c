/*
 * $Id: gtkledbar.c,v 1.1.1.1 2002/09/08 04:03:51 welty Exp $
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

#include <gtk/gtk.h>
#include "gtkledbar.h"

static void     led_bar_class_init        (LedBarClass *klass);
static void     led_bar_init              (LedBar      *led_bar);

guint
led_bar_get_type ()
{
  static guint led_bar_type = 0;

  if (!led_bar_type)
    {
      GtkTypeInfo led_bar_info = {
	"LedBar",
	sizeof (LedBar),
	sizeof (LedBarClass),
	(GtkClassInitFunc) led_bar_class_init,
	(GtkObjectInitFunc) led_bar_init,
	/* reserved_1 */ NULL,
	/* reserved_1 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      led_bar_type = gtk_type_unique (gtk_vbox_get_type (), &led_bar_info);
    }
  return led_bar_type;
}

static void
led_bar_class_init (LedBarClass *class)
{
  GtkObjectClass   *object_class;

  object_class = (GtkObjectClass *) class;

}

static void
led_bar_init (LedBar *led_bar)
{
  led_bar->num_segments = 0;
  led_bar->lit_segments = 0;
  led_bar->seq_segment  = 0;
  led_bar->seq_dir      = 1;
}

GtkWidget *
led_bar_new (gint segments, gint orientation )
{
  LedBar    *led_bar;
  GtkWidget *table;
  gint      i;
  GdkColor  active;
  GdkColor  inactive;
  gint      half, full;

  led_bar = gtk_type_new (led_bar_get_type ());
  if (segments > MAX_SEGMENTS)
    segments = MAX_SEGMENTS;
  led_bar->num_segments = segments;
  led_bar->orientation = orientation;
  if ( !orientation ) /* horiz */
      table = gtk_table_new (1, segments, FALSE);
  else /* vert */
      table = gtk_table_new (segments, 1, FALSE);
  gtk_container_add (GTK_CONTAINER (led_bar), table);
  gtk_widget_show (table);
  half = .50 * segments;
  full = .75 * segments;
  gdk_color_parse ("#00F100", &active);
  gdk_color_parse ("#008C00", &inactive);
  for (i = 0; i < segments; i++) 
    {
      if (i >= half && i <= full)
	{
	  gdk_color_parse ("#F1EE00", &active);
	  gdk_color_parse ("#8CAA00", &inactive);
	}
      else
	if (i >= full)
	  {
	    gdk_color_parse ("#F10000", &active);
	    gdk_color_parse ("#8C0000", &inactive);
	  }
      led_bar->segments[i] = gtk_led_new ();
      gtk_led_set_colors (GTK_LED (led_bar->segments[i]), &active, &inactive);

      if ( !orientation ) /* horiz */
	  gtk_table_attach (GTK_TABLE (table), led_bar->segments[i],
				     i, (i + 1), 0, 1, 0, 0, 0, 0);
      else /* vert */
	  gtk_table_attach (GTK_TABLE (table), led_bar->segments[i],
				     0, 1, (segments - i - 1), (segments - i),
                                     0, 0, 0, 0 );
      gtk_widget_show (led_bar->segments[i]);
    }

  return GTK_WIDGET (led_bar);
}

gint
led_bar_get_num_segments (GtkWidget *bar)
{
  g_return_val_if_fail (bar != NULL, 0);
  g_return_val_if_fail (IS_LEDBAR (bar), 0);

  return (LEDBAR (bar)->num_segments);
}

void
led_bar_light_segments (GtkWidget *bar, gint num)
{
  LedBar    *led_bar;
  int       i;

  g_return_if_fail (bar != NULL);
  g_return_if_fail (IS_LEDBAR (bar));

  led_bar = LEDBAR (bar);
  if (num == 0 && led_bar->lit_segments == 0)
    return;
  if (num < led_bar->lit_segments)
    {
      for (i = 0; i < num; i++) 
	{
	  gtk_led_set_state (GTK_LED (led_bar->segments[i]), 
			     GTK_STATE_SELECTED,
			     TRUE);
	}
    }
  else
    {
      for (i = led_bar->lit_segments; i < num; i++)
	gtk_led_set_state (GTK_LED (led_bar->segments[i]),
			   GTK_STATE_SELECTED,
			   TRUE);
    }
  led_bar->lit_segments = i;
}

void 
led_bar_unlight_segments (GtkWidget *bar, gint num)
{
  LedBar    *led_bar;
  int       i;

  g_return_if_fail (bar != NULL);
  g_return_if_fail (IS_LEDBAR (bar));

  led_bar = LEDBAR (bar);
  if (led_bar->lit_segments == 0)
    return;

  for (i = 0; i < num; i++) {
    gtk_led_set_state (GTK_LED (led_bar->segments[i]),
		       GTK_STATE_SELECTED,
		       FALSE);
  }
  led_bar->lit_segments -= num;
  if (led_bar->lit_segments < 0)
    led_bar->lit_segments = 0;
}

void 
led_bar_light_segment (GtkWidget *bar, gint segment)
{
  LedBar     *led_bar;

  g_return_if_fail (bar != NULL);
  g_return_if_fail (IS_LEDBAR (bar));

  led_bar = LEDBAR (bar);
  gtk_led_set_state (GTK_LED (led_bar->segments[segment]),
		     GTK_STATE_SELECTED,
		     TRUE);
}

void
led_bar_unlight_segment (GtkWidget *bar, gint segment)
{
  LedBar     *led_bar;

  g_return_if_fail (bar != NULL);
  g_return_if_fail (IS_LEDBAR (bar));

  led_bar = LEDBAR (bar);
  gtk_led_set_state (GTK_LED (led_bar->segments[segment]),
		     GTK_STATE_SELECTED,
		     FALSE);
}

void
led_bar_light_percent (GtkWidget *bar, gfloat percent)
{
  LedBar     *led_bar;
  gint       num, i;

  g_return_if_fail (bar != NULL);
  g_return_if_fail (IS_LEDBAR (bar));

  led_bar = LEDBAR (bar);
  num = percent * led_bar->num_segments;
  led_bar->lit_segments = num;
  for (i = 0; i < led_bar->num_segments; i++) 
    {
      if (num > 0 ) 
	{
	  if (! (GTK_LED (led_bar->segments[i])->is_on))
	    gtk_led_set_state (GTK_LED (led_bar->segments[i]),
			       GTK_STATE_SELECTED,
			       TRUE);
	  num--;
	} else {
	  if (GTK_LED (led_bar->segments[i])->is_on)
	    gtk_led_set_state (GTK_LED (led_bar->segments[i]),
			       GTK_STATE_SELECTED,
			       FALSE);
    }
  }
}

void
led_bar_clear (GtkWidget *bar)
{
  LedBar     *led_bar;
  int        i;

  g_return_if_fail (bar != NULL);
  g_return_if_fail (IS_LEDBAR (bar));

  led_bar = LEDBAR (bar);
  for (i = 0; i < led_bar->num_segments; i++) 
    {
      if (GTK_LED (led_bar->segments[i])->is_on)
	gtk_led_set_state (GTK_LED (led_bar->segments[i]),
			   GTK_STATE_SELECTED,
			   FALSE);
    }
}

void
led_bar_sequence_step (GtkWidget *bar)
{
  LedBar    *led_bar;

  g_return_if_fail (bar != NULL);
  g_return_if_fail (IS_LEDBAR (bar));

  led_bar = LEDBAR (bar);
  if (led_bar->seq_segment >= (led_bar->num_segments - 1))
    led_bar->seq_dir = -1;
  else if (led_bar->seq_segment <= 0)
    led_bar->seq_dir = 1;

  led_bar_unlight_segment (GTK_WIDGET(led_bar), led_bar->seq_segment);
  led_bar->seq_segment += led_bar->seq_dir;
  led_bar_light_segment (GTK_WIDGET(led_bar), led_bar->seq_segment);
}

/* EOF */



