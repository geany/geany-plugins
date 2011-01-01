/*
 *      bibtex.c
 *
 *      Copyright 2008-2011 Frank Lanitz <frank(at)frank(dot)uvena(dot)de>
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

#include "bibtex.h"
#include "reftex.h"


void glatex_insert_bibtex_entry(G_GNUC_UNUSED GtkMenuItem * menuitem,
						 gpointer gdata)
{
	gint i;
	gint doctype = GPOINTER_TO_INT(gdata);
	GPtrArray *entry = glatex_bibtex_init_empty_entry();

	switch(doctype) {
	case GLATEX_BIBTEX_ARTICLE:
		g_ptr_array_index(entry, GLATEX_BIBTEX_AUTHOR) = g_strdup("");
		g_ptr_array_index(entry, GLATEX_BIBTEX_TITLE) = g_strdup("");
		g_ptr_array_index(entry, GLATEX_BIBTEX_JOURNAL) = g_strdup("");
		g_ptr_array_index(entry, GLATEX_BIBTEX_YEAR) = g_strdup("");
		break;
	case GLATEX_BIBTEX_BOOK:
		g_ptr_array_index(entry, GLATEX_BIBTEX_AUTHOR) = g_strdup("");
		g_ptr_array_index(entry, GLATEX_BIBTEX_EDITOR) = g_strdup("");
		g_ptr_array_index(entry, GLATEX_BIBTEX_TITLE) = g_strdup("");
		g_ptr_array_index(entry, GLATEX_BIBTEX_PUBLISHER) = g_strdup("");
		g_ptr_array_index(entry, GLATEX_BIBTEX_YEAR) = g_strdup("");
		break;
	case GLATEX_BIBTEX_BOOKLET:
		g_ptr_array_index(entry, GLATEX_BIBTEX_TITLE) = g_strdup("");
		break;
	case GLATEX_BIBTEX_CONFERENCE:
	case GLATEX_BIBTEX_INCOLLECTION:
	case GLATEX_BIBTEX_INPROCEEDINGS:
		g_ptr_array_index(entry, GLATEX_BIBTEX_AUTHOR) = g_strdup("");
		g_ptr_array_index(entry, GLATEX_BIBTEX_TITLE) = g_strdup("");
		g_ptr_array_index(entry, GLATEX_BIBTEX_BOOKTITLE) = g_strdup("");
		g_ptr_array_index(entry, GLATEX_BIBTEX_YEAR) = g_strdup("");
		break;
	case GLATEX_BIBTEX_INBOOK:
		g_ptr_array_index(entry, GLATEX_BIBTEX_AUTHOR) = g_strdup("");
		g_ptr_array_index(entry, GLATEX_BIBTEX_EDITOR) = g_strdup("");
		g_ptr_array_index(entry, GLATEX_BIBTEX_TITLE) = g_strdup("");
		g_ptr_array_index(entry, GLATEX_BIBTEX_CHAPTER) = g_strdup("");
		g_ptr_array_index(entry, GLATEX_BIBTEX_PAGES) = g_strdup("");
		g_ptr_array_index(entry, GLATEX_BIBTEX_PUBLISHER) = g_strdup("");
		g_ptr_array_index(entry, GLATEX_BIBTEX_YEAR) = g_strdup("");
		break;
	case GLATEX_BIBTEX_MANUAL:
		g_ptr_array_index(entry, GLATEX_BIBTEX_TITLE) = g_strdup("");
		break;
	case GLATEX_BIBTEX_MASTERSTHESIS:
	case GLATEX_BIBTEX_PHDTHESIS:
		g_ptr_array_index(entry, GLATEX_BIBTEX_AUTHOR) = g_strdup("");
		g_ptr_array_index(entry, GLATEX_BIBTEX_TITLE) = g_strdup("");
		g_ptr_array_index(entry, GLATEX_BIBTEX_SCHOOL) = g_strdup("");
		g_ptr_array_index(entry, GLATEX_BIBTEX_YEAR) = g_strdup("");
		break;
	case GLATEX_BIBTEX_MISC:
		for (i = 0; i < GLATEX_BIBTEX_N_ENTRIES; i++)
		{
			g_ptr_array_index(entry, i) = g_strdup("");
		}
	case GLATEX_BIBTEX_TECHREPORT:
		g_ptr_array_index(entry, GLATEX_BIBTEX_AUTHOR) = g_strdup("");
		g_ptr_array_index(entry, GLATEX_BIBTEX_TITLE) = g_strdup("");
		g_ptr_array_index(entry, GLATEX_BIBTEX_INSTITUTION) = g_strdup("");
		g_ptr_array_index(entry, GLATEX_BIBTEX_YEAR) = g_strdup("");
		break;
	case GLATEX_BIBTEX_UNPUBLISHED:
		g_ptr_array_index(entry, GLATEX_BIBTEX_AUTHOR) = g_strdup("");
		g_ptr_array_index(entry, GLATEX_BIBTEX_TITLE) = g_strdup("");
		g_ptr_array_index(entry, GLATEX_BIBTEX_NOTE) = g_strdup("");
		break;
	case GLATEX_BIBTEX_PROCEEDINGS:
		g_ptr_array_index(entry, GLATEX_BIBTEX_TITLE) = g_strdup("");
		g_ptr_array_index(entry, GLATEX_BIBTEX_YEAR) = g_strdup("");
		break;
	default:
		for (i = 0; i < GLATEX_BIBTEX_N_ENTRIES; i++)
		{
			g_ptr_array_index(entry, i) = g_strdup("");
		}
	}

	glatex_bibtex_write_entry(entry, doctype);

	g_ptr_array_free(entry, TRUE);
}

/* Creating and initialising a array for a bibTeX entry with NULL pointers*/
GPtrArray *glatex_bibtex_init_empty_entry()
{
	GPtrArray *entry = g_ptr_array_new();
	g_ptr_array_set_size(entry, GLATEX_BIBTEX_N_ENTRIES);
	return entry;
}


void glatex_bibtex_write_entry(GPtrArray *entry, gint doctype)
{
	gint i;
	GString *output = NULL;
	gchar *tmp = NULL;
	GeanyDocument *doc = NULL;
	const gchar *eol;
	
	doc = document_get_current();
	if (doc != NULL)
	{
		eol = editor_get_eol_char(doc->editor);
	}
	else
	{
		eol = "\n";
	}
	/* Adding the doctype to entry */
	output = g_string_new("@");
	g_string_append(output, glatex_bibtex_types[doctype].latex);
	g_string_append(output, "{");
	g_string_append(output, eol);

	/* Adding the keywords and values to entry */
	for (i = 0; i < GLATEX_BIBTEX_N_ENTRIES; i++)
	{
		/* Check whether a field has been marked for being used */
		if (g_ptr_array_index (entry, i) != NULL)
		{
			/* Check whether a field was only set for being used ... */
			if (utils_str_equal(g_ptr_array_index (entry, i), "\0"))
			{
				g_string_append(output, glatex_label_entry_keywords[i]);
				g_string_append(output," = {},");
				g_string_append(output, eol);
			}
			/* ... or has some real value inside. */
			else
			{
				g_string_append(output, glatex_label_entry_keywords[i]);
				g_string_append(output, " = {");
				g_string_append(output, g_ptr_array_index(entry, i));
				g_string_append(output, "},");
				g_string_append(output, eol);
			}
		}
	}

	g_string_append(output, "}");
	g_string_append(output, eol);
	tmp = g_string_free(output, FALSE);
	sci_start_undo_action(doc->editor->sci);
	glatex_insert_string(tmp, FALSE);
	sci_end_undo_action(doc->editor->sci);
	g_free(tmp);
}

void glatex_bibtex_insert_cite(gchar *reference_name, gchar *option)
{
	gchar *tmp;

	g_return_if_fail(reference_name != NULL);
	
	if (option != NULL)
	{
		tmp = g_strconcat("\\cite[", option, "]{", reference_name, "}", NULL);
	}
	else
	{
		tmp = g_strconcat("\\cite{", reference_name, "}", NULL);
	}
	glatex_insert_string(tmp, TRUE);
	g_free(tmp);
}

/* Parses a given bib file and inserting found references into a given comboboy */
void glatex_parse_bib_file(const gchar* file, gpointer combobox)
{
	gchar **bib_entries = NULL;
	int i = 0;
	LaTeXLabel *tmp;
	gchar *tmp_label_name = NULL;
	
	if (file != NULL)
	{
		/*  Return if its not an bib file */
		if (!g_str_has_suffix(file, ".bib"))
		{
			return;
		}

		bib_entries = glatex_read_file_in_array(file);

		if (bib_entries != NULL)
		{
			for (i = 0; bib_entries[i] != NULL ; i++)
			{
				if  (g_str_has_prefix(bib_entries[i], "@"))
				{
					tmp = glatex_parseLine_bib(bib_entries[i]);
					tmp_label_name = g_strdup(tmp->label_name);
					gtk_combo_box_append_text(GTK_COMBO_BOX(combobox), tmp_label_name);
					g_free(tmp);
					g_free(tmp_label_name);
				}
			}
			g_free(bib_entries);
		}
	}
}


LaTeXLabel* glatex_parseLine_bib(const gchar *line)
{
	LaTeXLabel *label;
	const gchar *tmp_string = NULL;
	const gchar *x = NULL;
	gint l = 0;

	label = g_new0(LaTeXLabel, 1);
	
	x = line;
	
	while (*x != '\0' && 
		   *x != '{')
	{
		x++;
	}
	tmp_string = x + 1;
	g_warning("%s", tmp_string);
	while (*x != '\0' && *x != ',')
	{
		l++;
		x++;
	}
	label->label_name = g_strndup(tmp_string, l - 1);
	return label;
}
