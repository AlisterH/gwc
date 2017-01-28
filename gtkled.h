/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * GtkLed: Emulate a simple LED (light emitting diode)
 * Copyright (C) 1997 Tim Janik
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#ifndef __GTK_LED_H__
#define __GTK_LED_H__


#include <gdk/gdk.h>
#include <gtk/gtkmisc.h>

G_BEGIN_DECLS

#define GTK_LED_TYPE (gtk_led_get_type ())
#define GTK_LED(obj)	      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_LED_TYPE, GtkLed))
#define GTK_LED_CLASS(klass)  (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_LED_TYPE, GtkLedClass))
#define GTK_IS_LED(obj)	      (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_LED_TYPE))


typedef struct _GtkLed	     GtkLed;
typedef struct _GtkLedClass  GtkLedClass;

struct _GtkLed
{
  GtkMisc  misc;
  GdkColor fg[2];
  GdkColor bg[2];

  GdkGC    *gc;

  guint is_on;
  gint width, height;
};

struct _GtkLedClass
{
  GtkMiscClass parent_class;
};


GType	   gtk_led_get_type    (void);
GtkWidget* gtk_led_new	       (void);
void	   gtk_led_set_state   (GtkLed			*led,
				GtkStateType		widget_state,
				gboolean		on_off);
void	   gtk_led_switch      (GtkLed			*led,
				gboolean		on_off);
gboolean   gtk_led_is_on       (GtkLed			*led);
void       gtk_led_set_colors  (GtkLed                  *led,
				GdkColor                *active,
				GdkColor                *inactive);

G_END_DECLS

#endif /* __GTK_LED_H__ */
