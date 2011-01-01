/*
 *      reftex.c
 *
 *      Copyright 2009-2011 Frank Lanitz <frank(at)frank(dot)uvena(dot)de>
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

#include <string.h>
#include "reftex.h"
#include "latexutils.h"

void glatex_parse_aux_file(gchar *file, gpointer combobox)
{
	gchar **aux_entries = NULL;
	int i = 0;
	LaTeXLabel *tmp;
	gchar *tmp_label_name = NULL;

	if (file != NULL)
	{
		/*  Return if its not an aux file */
		if (!g_str_has_suffix(file, ".aux"))
			return;

		aux_entries = glatex_read_file_in_array(file);

		if (aux_entries != NULL)
		{
			for (i = 0; aux_entries[i] != NULL ; i++)
			{
				if  (g_str_has_prefix(aux_entries[i], "\\newlabel"))
				{
					tmp = glatex_parseLine(aux_entries[i]);
					tmp_label_name = g_strdup(tmp->label_name);
					gtk_combo_box_append_text(GTK_COMBO_BOX(combobox), tmp_label_name);
					g_free(tmp);
					g_free(tmp_label_name);
				}
			}
			g_free(aux_entries);
		}
	}
}

void glatex_add_Labels(GtkWidget *combobox, GSList *dir)
{

	if (dir == NULL)
	{
		return;
	}

	g_slist_foreach(dir, (GFunc) glatex_parse_aux_file, combobox);
}


LaTeXLabel* glatex_parseLine(const gchar *line)
{
	LaTeXLabel *label;
	gchar *t = NULL;
	const gchar *x = NULL;
	gint l = 0;

	label = g_new0(LaTeXLabel, 1);

	line += 10;
	x = line;
	t = strchr(line, '}');
	if (t != NULL)
	{
		while (*x != '\0' && x < t && *x != '}')
		{
			l++;
			x++;
		}
	}
	label->label_name = g_strndup(line, l);

	return label;
}
