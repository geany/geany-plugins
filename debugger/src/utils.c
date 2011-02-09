/*
 *		utils.c
 *      
 *      Copyright 2010 Alexander Petukhov <Alexander(dot)Petukhov(at)mail(dot)ru>
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
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

/*
 *		Miscellaneous functions.
 */

#include <stdlib.h>
#include <memory.h>
#include <stdio.h>
#include <gtk/gtk.h>

#include "breakpoint.h"
#include "debug_module.h"

#include "geanyplugin.h"
extern GeanyFunctions	*geany_functions;

/* character to calculate width */
#define CHAR_TESTED "W"

/* left / right paddings from the header text to the columns border */
#define RENDERER_X_PADDING 5
#define RENDERER_Y_PADDING 2

/*
 * get character width in a widget
 */
int get_char_width(GtkWidget *widget)
{
	PangoLayout *playout = pango_layout_new(gtk_widget_get_pango_context(widget));

	int width, height;

	pango_layout_set_text(playout, CHAR_TESTED, -1);
	pango_layout_get_pixel_size (playout, &width, &height);

	g_object_unref(playout);

	return width;
}

/*
 * get string width if string length is bigger than minwidth or minwidth * char_width 
 */
int get_header_string_width(gchar *header, int minchars, int char_width)
{
	return strlen(header) > minchars ? strlen(header) : minchars * char_width;
}

/*
 * create tree view column 
 */
GtkTreeViewColumn *create_column(gchar *name, GtkCellRenderer *renderer, gboolean expandable, gint minwidth, gchar *arg, int value)
{
	gtk_cell_renderer_set_padding(renderer, RENDERER_X_PADDING, RENDERER_Y_PADDING);
	
	GtkTreeViewColumn *column =
		gtk_tree_view_column_new_with_attributes (name, renderer, arg, value, NULL);

	if (expandable)
		gtk_tree_view_column_set_expand(column, expandable);
	else
		gtk_tree_view_column_set_min_width(column, minwidth);
	
	return column;
}

/*
 * opens position in a editor 
 */
void editor_open_position(char* file, int line)
{
	GeanyDocument* doc = NULL;
	gboolean already_open = (doc = document_get_current()) && !strcmp(DOC_FILENAME(doc), file);

	if (!already_open)
		doc = document_open_file(file, FALSE, NULL, NULL);

	if (doc)
	{
		sci_goto_line(doc->editor->sci, line - 1, TRUE);
		scintilla_send_message(doc->editor->sci, SCI_SETFOCUS, TRUE, 0);
	}
}

/*
 * GTree iteratin functions that sets marker for each breakpoint in the tree 
 */
gboolean tree_foreach_set_marker(gpointer key, gpointer value, gpointer data)
{
	breakpoint *bp = (breakpoint*)value;
	markers_add_breakpoint(bp);
	return FALSE;
}

/*
 * get word at "position" in Scintilla document
 */
GString* get_word_at_position(ScintillaObject *sci, int position)
{
	GString *word = g_string_new("");

	gchar gc;

	/* first, move to the beginning of a word */
	do
	{
		gc = sci_get_char_at(sci, position - 1);
		if (isalpha(gc) || '.' == gc || '_' == gc)
		{
			position--;
			continue;
		}
		else if ('>' == gc)
		{
			if('-' == sci_get_char_at(sci, position - 2))
			{
				position -= 2;
				continue;
			}
		}
		break;		
	}
	while(TRUE);

	/* move to the end of a word */
	do
	{
		gc = sci_get_char_at(sci, position);
		if (isalpha(gc) || '.' == gc || '_' == gc)
		{
			word = g_string_append_c(word, gc);
			position++;
			continue;
		}
		else if ('-' == gc)
		{
			gchar next = sci_get_char_at(sci, position + 1);
			if('>' == next)
			{
				word = g_string_append(word, "->");
				position += 2;
				continue;
			}
		}
		break;		
	}
	while (TRUE);
	
	return word;
}
