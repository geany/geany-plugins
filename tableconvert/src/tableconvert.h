/*
 * tableconvert.h
 *
 * Copyright 2014 Frank Lanitz <frank@frank.uvena.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 *
 */


#ifndef TABLECONVERT_H
#define TABLECONVERT_H

#include <geanyplugin.h>
#include <gtk/gtk.h>


extern GeanyPlugin	*geany_plugin;
extern GeanyData	*geany_data;


enum
{
	KB_HTMLTABLE_CONVERT_TO_TABLE,
	COUNT_KB
};

typedef struct {
	const gchar *type;
	const gchar *start;
	const gchar *header_start;
	const gchar *header_stop;
	const gchar *header_columnsplit;
	const gchar *header_linestart;
	const gchar *header_lineend;
	const gchar *body_start;
	const gchar *body_end;
	const gchar *columnsplit;
	const gchar *linestart;
	const gchar *lineend;
	/* linesplit should keep empty until you really need some special
	 * logic there. */
	const gchar *linesplit;
	const gchar *end;
} TableConvertRule;

enum {
	TC_LATEX = 0,
	TC_HTML,
	TC_SQL,
	TC_DOKUWIKI,
	TC_END
};

extern TableConvertRule tablerules[];

extern void cb_table_convert(G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer gdata);
extern void cb_table_convert_type(G_GNUC_UNUSED GtkMenuItem *menuitem, gpointer gdata);
extern void convert_to_table(gboolean header, gint file_type);

#endif
