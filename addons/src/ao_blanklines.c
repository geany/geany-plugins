/*
 *			ao_blanklines.c - this file is part of Addons, a Geany plugin
 *
 *			Copyright 2009 Eugene Arshinov <earshinov@gmail.com>
 *
 *			This program is free software; you can redistribute it and/or modify
 *			it under the terms of the GNU General Public License as published by
 *			the Free Software Foundation; either version 2 of the License, or
 *			(at your option) any later version.
 *
 *			This program is distributed in the hope that it will be useful,
 *			but WITHOUT ANY WARRANTY; without even the implied warranty of
 *			MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 *			GNU General Public License for more details.
 *
 *			You should have received a copy of the GNU General Public License
 *			along with this program; if not, write to the Free Software
 *			Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *			MA 02110-1301, USA.
 */

#include "geanyplugin.h"

#include "addons.h"
#include "ao_blanklines.h"

static gboolean enabled = FALSE;

static void editor_strip_trailing_newlines(GeanyEditor *editor)
{
	const gint maxline = sci_get_line_count(editor->sci) - 1;
	const gint maxpos = sci_get_line_end_position(editor->sci, maxline);

	gint line, start, end, pos;
	gchar ch;

	/*
	 * Store index of the last non-empty line in `line' and position of the first
	 * of its trailing spaces in `pos'. If all lines are empty, `line' will
	 * contain -1, and `pos' will be undefined. If `line' does not contain
	 * trailing spaces, `pos' will be its end position.
	 */
	for (line = maxline; line >= 0; line--)
	{
		start = sci_get_position_from_line(editor->sci, line);
		end = sci_get_line_end_position(editor->sci, line);

		/*
		 * We can't be sure that `geany_data->file_prefs->strip_trailing_spaces'
		 * setting is set, so we should check for trailing spaces manually.
		 * Performance overhead of the for loop below is very small anyway,
		 * so it does not worth checking the setting manually and writing
		 * additional code.
		 */

		for (pos = end-1; pos >= start; pos--)
		{
			ch = sci_get_char_at(editor->sci, pos);
			if (ch != ' ' && ch != '\t')
				break;
		}
		++pos;
		if (pos > start)
			break;
	}

	if (line == -1 || geany_data->file_prefs->final_new_line)
	{
		/* leave one newline */
		line++;
		pos = sci_get_position_from_line(editor->sci, line);
	}

	if (pos < maxpos)
	{
		/* there are some lines to be removed */
		sci_set_target_start(editor->sci, pos);
		sci_set_target_end(editor->sci, maxpos);
		sci_replace_target(editor->sci, "", FALSE);
	}
}

void ao_blanklines_set_enable(gboolean enabled_)
{
	enabled = enabled_;
}

void ao_blanklines_on_document_before_save(GObject *object, GeanyDocument *doc, gpointer data)
{
	if (enabled)
		editor_strip_trailing_newlines(doc->editor);
}
