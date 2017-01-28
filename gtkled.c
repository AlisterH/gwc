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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib-object.h>

#include "gtkled.h"

/* these define initial width and height of the LED */
#define	LED_WIDTH	(10)
#define	LED_HEIGHT	(5)
#define	BOTTOM_SPACE	(2)

static void gtk_led_finalize (GObject *object);
static void gtk_led_size_request (GtkWidget *widget,
				  GtkRequisition *requisition);
static void gtk_led_size_allocate (GtkWidget *widget,
				  GtkAllocation *allocate);
static gint gtk_led_expose (GtkWidget *widget,
			    GdkEventExpose *event);
static void gtk_led_realize (GtkWidget *widget);


enum {
	LED_COLOR_ON,
	LED_COLOR_OFF
};

G_DEFINE_TYPE(GtkLed, gtk_led, GTK_TYPE_MISC);

static void
gtk_led_class_init (GtkLedClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	
	object_class = (GObjectClass*) class;
	widget_class = (GtkWidgetClass*) class;
	
	object_class->finalize = gtk_led_finalize;
	
	widget_class->size_request = gtk_led_size_request;
	widget_class->expose_event = gtk_led_expose;
	widget_class->realize = gtk_led_realize;
}

static void
gtk_led_init (GtkLed *led)
{
	GtkMisc *misc;
	
	misc = GTK_MISC (led);
	
	GTK_WIDGET_SET_FLAGS (led, GTK_NO_WINDOW);
	
	led->is_on             = FALSE;
	led->gc                = NULL;
	led->width             = LED_WIDTH;
	led->height            = LED_HEIGHT;

	g_signal_connect (G_OBJECT(led), "size-allocate", G_CALLBACK (gtk_led_size_allocate), NULL);
}

GtkWidget*
gtk_led_new (void)
{
	GtkLed *led;
	
	led = gtk_type_new (gtk_led_get_type ());
	
	return GTK_WIDGET (led);
}

void
gtk_led_set_colors (GtkLed *led, GdkColor *active, GdkColor *inactive)
{

	g_return_if_fail (GTK_IS_LED (led));
	
	led->fg[LED_COLOR_ON] = *(active);
	led->fg[LED_COLOR_OFF] = *(inactive);
}

void
gtk_led_set_state (GtkLed	*led,
		   GtkStateType widget_state,
		   gboolean	on_off)
{
	g_return_if_fail (GTK_IS_LED (led));
	
	gtk_widget_set_state (GTK_WIDGET (led), widget_state);
	gtk_led_switch (led, on_off);
}

void
gtk_led_switch (GtkLed	     *led,
		gboolean     on_off)
{
	g_return_if_fail (GTK_IS_LED (led));
	
	led->is_on = on_off != FALSE;
	gtk_widget_draw (GTK_WIDGET (led), NULL);
}

gboolean
gtk_led_is_on (GtkLed  *led)
{
	g_return_val_if_fail (GTK_IS_LED (led), FALSE);
	
	return led->is_on;
}

static void
gtk_led_finalize (GObject *object)
{
	GtkLed *led;
	
	g_return_if_fail (GTK_IS_LED (object));
	
	led = GTK_LED (object);
	
	if (GTK_WIDGET (object)->parent &&
	    GTK_WIDGET_MAPPED (object)) {
		gtk_widget_unmap (GTK_WIDGET (object));
	}

	G_OBJECT_CLASS (gtk_led_parent_class)->finalize (object);
}

static void
gtk_led_size_request (GtkWidget	     *widget,
		      GtkRequisition *requisition)
{
	GtkLed *led;
	
	g_return_if_fail (GTK_IS_LED (widget));
	g_return_if_fail (requisition != NULL);
	
	led = GTK_LED (widget);
	
	requisition->width = led->width + led->misc.xpad * 2;
	requisition->height = led->height + led->misc.ypad * 2 + BOTTOM_SPACE;
}

static void
gtk_led_size_allocate (GtkWidget     *widget,
		       GtkAllocation *allocate)
{
	GtkLed *led;
	
	g_return_if_fail (GTK_IS_LED (widget));
	g_return_if_fail (allocate != NULL);

	led = GTK_LED (widget);
	
	led->width = allocate->width;
	led->height = allocate->height - BOTTOM_SPACE;
}

static void 
gtk_led_realize (GtkWidget *widget)
{
	GtkLed       *led;
	GdkColormap  *cmap;
	
	g_return_if_fail (GTK_IS_LED (widget));
	
	GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

	led = GTK_LED (widget);

	widget->window = gtk_widget_get_parent_window (widget);
	gdk_window_ref (widget->window);
	widget->style = gtk_style_attach (widget->style, widget->window);
	if (!led->gc) {
		cmap = gtk_widget_get_colormap (widget);
		
		if (!(&(led->fg[LED_COLOR_ON]))) {
			gdk_color_parse ("#00F100", &(led->fg[LED_COLOR_ON]));
		}

		gdk_color_alloc (cmap, &(led->fg[LED_COLOR_ON]));

		if (!(&(led->fg[LED_COLOR_OFF]))) {
			gdk_color_parse ("#008C00", &(led->fg[LED_COLOR_OFF]));
		}

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
	
	g_return_val_if_fail (GTK_IS_LED (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);
	
	led = GTK_LED (widget);
	misc = GTK_MISC (widget);
	
	if (GTK_WIDGET_DRAWABLE (widget)) {
		if ((widget->allocation.width >= widget->requisition.width) &&
		    (widget->allocation.height >= widget->requisition.height)) {
			guint x, y;
			win_bg = (led->is_on) ? &(led->fg[LED_COLOR_ON]) : 
				&(led->fg[LED_COLOR_OFF]);

			gdk_gc_set_foreground (led->gc, win_bg);
			x = widget->allocation.x + misc->xpad * misc->xalign + 0.5;

			y = widget->allocation.y + misc->ypad * misc->xalign + 0.5 - BOTTOM_SPACE;
			
			gtk_draw_shadow (widget->style, widget->window,
					 GTK_STATE_NORMAL, GTK_SHADOW_IN,
					 x, y,
					 led->width,
					 led->height);
			
			gdk_draw_rectangle (widget->window,
					    led->gc,
					    TRUE,
					    x + 1, y + 1,
					    led->width - 2,
					    led->height - 2);
		
			
		}
	}
	
	return TRUE;
}
/* EOF */

