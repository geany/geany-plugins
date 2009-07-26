/*
 *      reftex.c
 *
 *      Copyright 2009 Frank Lanitz <frank(at)frank(dot)uvena(dot)de>
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

void glatex_add_Labels(GtkWidget *combobox, const gchar *file)
{
	gchar **aux_entries = NULL;
	int i = 0;
	LaTeXLabel *tmp;
	if (file != NULL)
	{
		aux_entries = geanylatex_read_file_in_array(file);
		if (aux_entries != NULL)
		{
			for (i = 0; aux_entries[i] != NULL ; i++)
			{
				if  (g_str_has_prefix(aux_entries[i], "\\newlabel"))
				{
					tmp = glatex_parseLine(aux_entries[i]);
					gtk_combo_box_append_text(GTK_COMBO_BOX(combobox), 
						tmp->label_name);
					g_free(tmp);
				}
			}
		}
	}
}

LaTeXLabel* glatex_parseLine(const gchar *line)
{
	LaTeXLabel *label;
	label = g_new0(LaTeXLabel, 1);

	gchar *t = NULL;
	const gchar *x = NULL;
	gint l = 0;

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
