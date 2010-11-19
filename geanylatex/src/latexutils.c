/*
 *      latexutils.c
 *
 *      Copyright 2009-2010 Frank Lanitz <frank(at)frank(dot)uvena(dot)de>
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

#include "latexutils.h"
#include "geanylatex.h"

gchar **glatex_read_file_in_array(const gchar *filename)
{
	gchar **result = NULL;
	gchar *data;
	
	g_return_val_if_fail((filename != NULL), NULL);	
	g_return_val_if_fail(g_file_get_contents(filename, &data, NULL, NULL), NULL);
	
	if (data != NULL)
	{
		g_warning("Content eingelesen: \n %s", data);
		result = g_strsplit_set(data, "\r\n", -1);
		g_free(data);
		return result;
	}
	return NULL;
}

void glatex_usepackage(const gchar *pkg, const gchar *options)
{
	GeanyDocument *doc = NULL;
	gint i;
	gint document_number_of_lines;
	gchar *tmp_line;

	doc = document_get_current();

	/* Checking whether we have a document */
	g_return_if_fail(doc != NULL);

	/* Iterating through document to find \begin{document}
	 * Do nothing, if its not available at all */
	document_number_of_lines = sci_get_line_count(doc->editor->sci);
	for (i = 0; i < document_number_of_lines; i++)
	{
		tmp_line = sci_get_line(doc->editor->sci, i);
		if (utils_str_equal(tmp_line, "\\begin{document}\n"))
		{
			gint pos;
			gchar *packagestring;

			pos = sci_get_position_from_line(doc->editor->sci, i);
			/* Building up package string and inserting it */
			if (NZV(options))
			{
				packagestring = g_strconcat("\\usepackage[", options,
					"]{", pkg, "}\n", NULL);
			}
			else
			{
				packagestring = g_strconcat("\\usepackage{", pkg, "}\n", NULL);
			}
			sci_insert_text(doc->editor->sci, pos, packagestring);

			g_free(tmp_line);
			g_free(packagestring);

			return;
		}
		g_free(tmp_line);
	}

	dialogs_show_msgbox(GTK_MESSAGE_ERROR,
		_("Could not determine where to insert package: %s"
		  "\nPlease try insert package manually"), pkg);
	ui_set_statusbar(TRUE, _("Could not determine where to insert package: %s"), pkg );
}


void glatex_enter_key_pressed_in_entry(G_GNUC_UNUSED GtkWidget *widget, gpointer dialog)
{
	gtk_dialog_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
}


void
glatex_insert_string(const gchar *string, gboolean reset_position)
{
	GeanyDocument *doc = NULL;

	doc = document_get_current();

	if (doc != NULL && string != NULL)
	{
		gint pos = sci_get_current_position(doc->editor->sci);
		gint len = 0;

		if (reset_position == TRUE)
		{
			len = strlen(string);
		}

		editor_insert_text_block(doc->editor, string, pos, len, 0, TRUE);
	}
}


void glatex_replace_special_character()
{
	GeanyDocument *doc = NULL;
	doc = document_get_current();

	if (doc != NULL && sci_has_selection(doc->editor->sci))
	{
		guint selection_len;
		gchar *selection = NULL;
		GString *replacement = g_string_new(NULL);
		guint i;
		gchar *new = NULL;
		const gchar *entity = NULL;
		gchar buf[7];
		gint len;

		selection = sci_get_selection_contents(doc->editor->sci);

		selection_len = strlen(selection);

		for (i = 0; i < selection_len; i++)
		{
			len = g_unichar_to_utf8(g_utf8_get_char(selection + i), buf);
			i = len - 1 + i;
			buf[len] = '\0';
			entity = glatex_get_entity(buf);

			if (entity != NULL)
			{
			
				g_string_append(replacement, entity);
			}
			else
			{
				g_string_append(replacement, buf);
			}
		}
		new = g_string_free(replacement, FALSE);
		sci_replace_sel(doc->editor->sci, new);
		g_free(selection);
		g_free(new);
	}
}
