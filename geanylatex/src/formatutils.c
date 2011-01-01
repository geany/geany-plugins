/*
 *	  formatutils.c
 *
 *	  Copyright 2009-2011 Frank Lanitz <frank(at)frank(dot)uvena(dot)de>
 *
 *	  This program is free software; you can redistribute it and/or modify
 *	  it under the terms of the GNU General Public License as published by
 *	  the Free Software Foundation; either version 2 of the License, or
 *	  (at your option) any later version.
 *
 *	  This program is distributed in the hope that it will be useful,
 *	  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	  GNU General Public License for more details.
 *
 *	  You should have received a copy of the GNU General Public License
 *	  along with this program; if not, write to the Free Software
 *	  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *	  MA 02110-1301, USA.
 */

#include "formatutils.h"


void glatex_insert_latex_format(G_GNUC_UNUSED GtkMenuItem * menuitem,
						 G_GNUC_UNUSED gpointer gdata)
{
	gint format = GPOINTER_TO_INT(gdata);
	GeanyDocument *doc = NULL;
	doc = document_get_current();

	if (doc != NULL)
	{
		if (sci_has_selection(doc->editor->sci))
		{
			gchar *selection;
			gchar *replacement = NULL;

			selection = sci_get_selection_contents(doc->editor->sci);

			replacement = g_strconcat(glatex_format_pattern[format],"{",
				selection, "}", NULL);

			sci_replace_sel(doc->editor->sci, replacement);
			g_free(selection);
			g_free(replacement);
		}
		else
		{
			sci_start_undo_action(doc->editor->sci);
			glatex_insert_string(glatex_format_pattern[format], TRUE);
			glatex_insert_string("{", TRUE);
			glatex_insert_string("}", FALSE);
			sci_end_undo_action(doc->editor->sci);
		}
	}
}


void glatex_insert_latex_fontsize(G_GNUC_UNUSED GtkMenuItem * menuitem,
						 G_GNUC_UNUSED gpointer gdata)
{
	gint size = GPOINTER_TO_INT(gdata);
	GeanyDocument *doc = NULL;
	doc = document_get_current();

	if (doc != NULL)
	{
		if (sci_has_selection(doc->editor->sci))
		{
			gchar *selection;
			gchar *replacement = NULL;

			selection = sci_get_selection_contents(doc->editor->sci);

			replacement = g_strconcat("{",glatex_fontsize_pattern[size],
				" ", selection, "}", NULL);

			sci_replace_sel(doc->editor->sci, replacement);
			g_free(selection);
			g_free(replacement);
		}
		else
		{
			sci_start_undo_action(doc->editor->sci);
			glatex_insert_string(glatex_fontsize_pattern[size], TRUE);
			glatex_insert_string(" ", TRUE);
			sci_end_undo_action(doc->editor->sci);
		}
	}
}
