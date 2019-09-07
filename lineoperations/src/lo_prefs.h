/*
 *      lo_prefs.h - Line operations, remove duplicate lines, empty lines,
 *                   lines with only whitespace, sort lines.
 *
 *      Copyright 2015 Sylvan Mostert <smostert.dev@gmail.com>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License along
 *      with this program; if not, write to the Free Software Foundation, Inc.,
 *      51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef LO_PREFS_H
#define LO_PREFS_H

#include <geanyplugin.h>
#include "lo_prefs.h"


typedef struct
{
	/* general settings */
	gchar *config_file;
	gboolean use_collation_compare;
} LineOpsInfo;


extern LineOpsInfo *lo_info;


/* handle button presses in the preferences dialog box */
void
lo_configure_response_cb(GtkDialog *dialog, gint response, gpointer user_data);


/* Configure the preferences GUI and callbacks */
GtkWidget *
lo_configure(G_GNUC_UNUSED GeanyPlugin *plugin, GtkDialog *dialog, G_GNUC_UNUSED gpointer pdata);


/* Initialize preferences */
void
lo_init_prefs(GeanyPlugin *plugin);


/* Free config */
void
lo_free_info();

#endif