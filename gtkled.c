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
/*  #include <config.h>  */
#include <gtk/gtk.h>
#include "gtkled.h"

#define	LED_WIDTH	(10)
#define	LED_HEIGHT	(5)
#define	BOTTOM_SPACE	(2)

static void gtk_led_class_init	 (GtkLedClass	 *klass);
static void gtk_led_init	 (GtkLed	 *led);
static void gtk_led_destroy	 (GtkObject	 *object);
static void gtk_led_size_request (GtkWidget	 *widget,
				  GtkRequisition *requisition);
static gint gtk_led_expose	 (GtkWidget	 *widget,
				  GdkEventExpose *event);
static void gtk_led_realize      (GtkWidget      *widget);


static GtkMiscClass *parent_class = NULL;

enum {
  LED_COLOR_ON,
  LED_COLOR_OFF
};


GtkType
gtk_led_get_type ()
{
  static GtkType led_type = 0;
  
  if (!led_type)
  {
    GtkTypeInfo led_info =
    {
      "GtkLed",
      sizeof (GtkLed),
      sizeof (GtkLedClass),
      (GtkClassInitFunc) gtk_led_class_init,
      (GtkObjectInitFunc) gtk_led_init,
      /* reserved */ NULL,
      /* reserved */ NULL,
      (GtkClassInitFunc) NULL,
    };
    
    led_type = gtk_type_unique (gtk_misc_get_type (), &led_info);
  }
  
  return led_type;
}

void
gtk_led_class_init (GtkLedClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  
  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  
  parent_class = gtk_type_class (gtk_misc_get_type ());
  
  object_class->destroy = gtk_led_destroy;
  
  widget_class->size_request  = gtk_led_size_request;
  widget_class->expose_event  = gtk_led_expose;
  widget_class->realize       = gtk_led_realize;
}

void
gtk_led_init (GtkLed *led)
{
  GtkMisc *misc;
  
  misc = GTK_MISC (led);

  GTK_WIDGET_SET_FLAGS (led, GTK_NO_WINDOW);
  
  led->is_on             = FALSE;
  led->gc                = NULL;

}

GtkWidget*
gtk_led_new ()
{
  GtkLed *led;
  
  led = gtk_type_new (gtk_led_get_type ());

  return GTK_WIDGET (led);
}

void
gtk_led_set_colors (GtkLed *led, GdkColor *active, GdkColor *inactive)
{

  g_return_if_fail (led != NULL);
  g_return_if_fail (GTK_IS_LED (led));

  led->fg[LED_COLOR_ON] = *(active);
  led->fg[LED_COLOR_OFF] = *(inactive);
}

void
gtk_led_set_state (GtkLed	*led,
		   GtkStateType widget_state,
		   gboolean	on_off)
{
  g_return_if_fail (led != NULL);
  g_return_if_fail (GTK_IS_LED (led));

  gtk_widget_set_state (GTK_WIDGET (led), widget_state);
  gtk_led_switch (led, on_off);
}

void
gtk_led_switch (GtkLed	     *led,
		gboolean     on_off)
{
  g_return_if_fail (led != NULL);
  g_return_if_fail (GTK_IS_LED (led));

  led->is_on = on_off != FALSE;
  gtk_widget_draw (GTK_WIDGET (led), NULL);
}

gboolean
gtk_led_is_on (GtkLed  *led)
{
  g_return_val_if_fail (led != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_LED (led), FALSE);
  
  return led->is_on;
}

static void
gtk_led_destroy (GtkObject *object)
{
  GtkLed *led;
  
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_LED (object));
  
  led = GTK_LED (object);
  
  if (GTK_WIDGET (object)->parent &&
      GTK_WIDGET_MAPPED (object))
    gtk_widget_unmap (GTK_WIDGET (object));
  
  if (GTK_OBJECT_CLASS (parent_class)->destroy)
    (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
gtk_led_size_request (GtkWidget	     *widget,
		      GtkRequisition *requisition)
{
  GtkLed *led;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_LED (widget));
  g_return_if_fail (requisition != NULL);
  
  led = GTK_LED (widget);
  
  requisition->width = LED_WIDTH + led->misc.xpad * 2;
  requisition->height = LED_HEIGHT + led->misc.ypad * 2 + BOTTOM_SPACE;
}

static void 
gtk_led_realize (GtkWidget *widget)
{
  GtkLed       *led;
  GdkColormap  *cmap;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_LED (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
  led = GTK_LED (widget);
  widget->window = gtk_widget_get_parent_window (widget);
  gdk_window_ref (widget->window);
  widget->style = gtk_style_attach (widget->style, widget->window);
  if (!led->gc)
    {
      cmap = gtk_widget_get_colormap (widget);
      
      if (!(&(led->fg[LED_COLOR_ON])))
	gdk_color_parse ("#00F100", &(led->fg[LED_COLOR_ON]));
      gdk_color_alloc (cmap, &(led->fg[LED_COLOR_ON]));
      if (!(&(led->fg[LED_COLOR_OFF])))
	gdk_color_parse ("#008C00", &(led->fg[LED_COLOR_OFF]));
      gdk_color_alloc (cmap, &(led->fg[LED_COLOR_OFF]));
      led->gc = gdk_gc_new (widget->window);
      gdk_gc_copy (led->gc, widget->style->white_gc);
    }
	

}

static gint
gtk_led_expose (GtkWidget      *widget,
		GdkEventExpose *event)
{
  GtkLed   *led;
  GtkMisc  *misc;
  GdkColor *win_bg;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_LED (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
  
  led = GTK_LED (widget);
  misc = GTK_MISC (widget);

  if (GTK_WIDGET_DRAWABLE (widget))
  {
    if ((widget->allocation.width >= widget->requisition.width) &&
	(widget->allocation.height >= widget->requisition.height))
    {
      guint x, y;
      win_bg = (led->is_on) ? &(led->fg[LED_COLOR_ON]) : 
	&(led->fg[LED_COLOR_OFF]);
      gdk_gc_set_foreground (led->gc, win_bg);
      x = widget->allocation.x + misc->xpad +
	  (widget->allocation.width - widget->requisition.width) * 
	misc->xalign + 0.5;
      y = widget->allocation.y + misc->ypad + LED_HEIGHT +
	  (widget->allocation.height - widget->requisition.height) * 
	misc->xalign + 0.5 - BOTTOM_SPACE;

      gtk_draw_shadow (widget->style, widget->window,
		       GTK_STATE_NORMAL, GTK_SHADOW_IN,
		       x, y,
		       LED_WIDTH,
		       LED_HEIGHT);

      gdk_draw_rectangle (widget->window,
			  led->gc,
			  TRUE,
			  x + 1, y + 1,
			  LED_WIDTH - 2,
			  LED_HEIGHT - 2);
    }
  }
  
  return TRUE;
}
/* EOF */

