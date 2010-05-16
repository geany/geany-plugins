/*
 *      latexstructure.c
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

#include "latexstructure.h"

const gchar *glatex_structure_values[] = {
	("\\part"),
	("\\chapter"),
	("\\section"),
	("\\subsection"),
	("\\subsubsection"),
	("\\paragraph"),
	("\\subparagraph"),
	("\\subsubparagraph"),
	( NULL )
};

/* Takes over a direktion into which the structur element should
 * be changed.
 * TRUE -> rotate down (\section -> \subsection)
 * FALSE -> rotate up (\section -> \chapter)
 * Also it takes over the start structure level as a gint enum */
gint glatex_structure_rotate(gboolean direction, gint start)
{
	gint new_value;

	if (direction == TRUE)
	{
		if (start == GLATEX_STRUCTURE_SUBSUBPARAGRAPH)
			new_value = GLATEX_STRUCTURE_PART;
		else
			new_value = start + 1;
	}
	else
	{
		if (start == GLATEX_STRUCTURE_PART)
			new_value = GLATEX_STRUCTURE_SUBSUBPARAGRAPH;
		else
			new_value = start - 1;
	}
	return new_value;
}


void glatex_structure_lvlup()
{
	gint i;
	GeanyDocument *doc = NULL;
	gchar *tmp = NULL;
	GString *haystack = NULL;

	doc = document_get_current();

	if (doc == NULL)
		return;

	if (! sci_has_selection(doc->editor->sci))
		return;

	sci_start_undo_action(doc->editor->sci);
	tmp = sci_get_selection_contents(doc->editor->sci);
	haystack = g_string_new(tmp);

	g_free(tmp);
	tmp = NULL;

	for (i = 0; i < GLATEX_STRUCTURE_N_LEVEL; i++)
	{
		if (utils_string_replace_all
			(haystack,
			glatex_structure_values[i],
			glatex_structure_values[glatex_structure_rotate(FALSE, i)]
			) > 0)
		{
			tmp = g_string_free(haystack, FALSE);
			haystack = NULL;
			sci_replace_sel(doc->editor->sci, tmp);
			g_free(tmp);
			sci_end_undo_action(doc->editor->sci);
			break;
		}
	}

	if (haystack != NULL)
		g_string_free(haystack, TRUE);
}

void glatex_structure_lvldown()
{
	gint i;
	GeanyDocument *doc = NULL;
	gchar *tmp = NULL;
	GString *haystack = NULL;

	doc = document_get_current();

	if (doc == NULL)
		return;

	if (! sci_has_selection(doc->editor->sci))
		return;

	tmp = sci_get_selection_contents(doc->editor->sci);
	haystack = g_string_new(tmp);

	g_free(tmp);
	tmp = NULL;

	for (i = 0; i < GLATEX_STRUCTURE_N_LEVEL; i++)
	{
		if (utils_string_replace_all(haystack,
			glatex_structure_values[i],
			glatex_structure_values[glatex_structure_rotate(TRUE, i)])
			> 0)
		{
			tmp = g_string_free(haystack, FALSE);
			haystack = NULL;
			sci_replace_sel(doc->editor->sci, tmp);
			g_free(tmp);
			break;
		}
	}

	if (haystack != NULL)
		g_string_free(haystack, TRUE);
}
