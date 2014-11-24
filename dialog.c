/*****************************************************************************
*   Gnome Wave Cleaner Version 0.19
*   Copyright (C) 2001 Jeffrey J. Welty
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

/* dialog.c */
/* utility routines to help with gtk boxes */

#include <stdlib.h>
#include "gwc.h"


int gwc_dialog_run(GtkDialog *dlg)
{
    int dres ;
    //we could do this and remove a bunch of other uses of gtk_widget_show()
    //gtk_widget_show_all(GTK_WIDGET(dlg));
    dres = gtk_dialog_run(GTK_DIALOG(dlg));

    if (dres == GTK_RESPONSE_OK)
	return 0 ;

    return 1 ;
}

GtkWidget *add_number_entry_with_label(char *entry_text, char *label_text, GtkWidget *table,
                                       int row)
{
    GtkWidget *entry, *label ;

    entry = gtk_entry_new ();
    gtk_entry_set_text(GTK_ENTRY(entry), entry_text) ;
    // enables the enter key to press the OK button
    gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
    gtk_widget_show (entry);
    gtk_table_attach_defaults(GTK_TABLE(table), entry,  0, 1, row, row+1) ;

    label = gtk_label_new (label_text);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_widget_show (label);
    gtk_table_attach_defaults(GTK_TABLE(table), label,  1, 2, row, row+1) ;
//  Alternative way to enable the enter key if we pass dlg to this function
//    g_signal_connect_swapped (entry, "activate",
//       G_CALLBACK (gtk_window_activate_default), dlg);

    return entry ;
    
}

GtkWidget *add_number_entry_with_label_int(int value, char *label_text, GtkWidget *table, int row)
{
    char buf[100] ;
    sprintf(buf, "%d", value) ;
    return add_number_entry_with_label(buf, label_text, table, row) ;
}

GtkWidget *add_number_entry_with_label_double(double value, char *label_text, GtkWidget *table, int row)
{
    char buf[100] ;
    sprintf(buf, "%lg", value) ;
    return add_number_entry_with_label(buf, label_text, table, row) ;
}
