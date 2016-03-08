/*
 *	  tableconvert.c
 *
 *	  Copyright 2011-2015 Frank Lanitz <frank(at)frank(dot)uvena(dot)de>
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
 *	  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
	#include "config.h" /* for the gettext domain */
#endif

#include "tableconvert.h"
#include "tableconvert_ui.h"

PLUGIN_VERSION_CHECK(224)

PLUGIN_SET_TRANSLATABLE_INFO(
    LOCALEDIR, GETTEXT_PACKAGE, _("Tableconvert"),
    _("Converts lists into tables for different filetypes"),
    VERSION, "Frank Lanitz <frank@frank.uvena.de>")

GeanyPlugin	 	*geany_plugin;
GeanyData	   	*geany_data;

TableConvertRule tablerules[] = {
	/* LaTeX */
	{
		N_("LaTeX"),								/* type               */
		"\\begin{table}[h]\n\\begin{tabular}{}\n",  /* start        	  */
		"\t",                                         /* header_start       */
		"",                                         /* header_stop        */
		" & ",										/* header_columnsplit */
		"",											/* header_linestart   */
		"\\\\ \\hline",								/* header_lineend	  */
		"",                                         /* body_start         */
		"",                                         /* body_end           */
		" & ",                                      /* columnsplit        */
		"\t",                                       /* linestart          */
		"\\\\",                                     /* lineend            */
		"",                                         /* linesplit          */
		"\\end{tabular}\n\\end{table}"              /* end                */
	},
	/* HTML */
	{
		N_("HTML"),			/* type               */
		"<table>\n",        /* start        	  */
		"<thead>\n",        /* header_start       */
		"</thead>\n",       /* header_stop        */
		"</th>\n\t<th>",      /* header_columnsplit */
		"<tr>\n\t<th>",		/* header_linestart   */
		"</th>\n</tr>\n",	/* header_lineend	  */
		"<tbody>\n",        /* body_start         */
		"\n</tbody>",       /* body_end           */
		"</td>\n\t<td>",    /* columnsplit        */
		"<tr>\n\t<td>",     /* linestart          */
		"</td>\n</tr>",     /* lineend            */
		"",                 /* linesplit          */
		"\n</table>"        /* end                */
	},
	/* SQL */
	/* Header not yet supported, so treated as normal
	 * "Body"-SQL. This is handled on function logic
	 * */
	{
		N_("SQL"), 			/* type               */
		"",                 /* start        	  */
		"",                 /* header_start       */
		"",                 /* header_stop        */
		"",                 /* header_columnsplit */
		"",					/* header_linestart   */
		"",					/* header_lineend	  */
		"",                 /* body_start         */
		"",                 /* body_end           */
		"','",              /* columnsplit        */
		"\t('",             /* linestart          */
		"')",               /* lineend            */
		",",                /* linesplit          */
		";"                 /* end                */
	},
	/* DokuWiki */
	{
		N_("DokuWiki"),		/* type               */
        "",                 /* start        	  */
        "",                 /* header_start       */
        "",                 /* header_stop        */
        " ^ ",              /* header_columnsplit */
        "^ ",				/* header_linestart   */
        " ^",				/* header_lineend	  */
        "",                 /* body_start         */
        "",                 /* body_end           */
        " | ",              /* columnsplit        */
        "| ",               /* linestart          */
        " |",               /* lineend            */
        "",                 /* linesplit          */
        "",                 /* end                */
     }
};


static gchar* convert_to_table_worker(gchar **rows, gboolean header,
	gint file_type)
{
	guint i;
	guint j;
	GString *replacement_str = NULL;
	GeanyDocument *doc = NULL;
	TableConvertRule *rule = &tablerules[file_type];
	doc = document_get_current();

	g_return_val_if_fail(rows != NULL, NULL);
	g_return_val_if_fail(rule != NULL, NULL);

	/* Adding start of table to replacement */
	replacement_str = g_string_new(rule->start);

	/* Iteration onto rows and building up lines of table for
	 * replacement */
	for (i = 0; rows[i] != NULL; i++)
	{
		gchar **columns = NULL;
		columns = g_strsplit_set(rows[i], "\t", -1);

		/* Putting in some header if needed */
		if (i == 0 &&
			header == TRUE)
		{
			/* header_start */
			g_string_append(replacement_str, rule->header_start);

			/* header_linestart */
			g_string_append(replacement_str, rule->header_linestart);

			/* header_columnsplit + columncontent */
			for (j = 0; columns[j] != NULL; j++)
			{
				if (j > 0)
				{
					g_string_append(replacement_str, rule->header_columnsplit);
				}
				g_string_append(replacement_str, columns[j]);
			}

			/* header_lineend */
			g_string_append(replacement_str, rule->header_lineend);

			/* header_stop */
			g_string_append(replacement_str, rule->header_stop);

			/* Adding a new line of header assuming it's something
			 * relevant for every typ */
			g_string_append(replacement_str, editor_get_eol_char(doc->editor));

			/* body_start */
			/* Only insert explicit body start if there is a header */
			g_string_append(replacement_str, rule->body_start);
		}
		if (i > 0 ||
			header == FALSE)
		{
			/* linestart */
			g_string_append(replacement_str, rule->linestart);

			/* columnsplit + column */
			for (j = 0; columns[j] != NULL; j++)
			{
				if (j > 0)
				{
					g_string_append(replacement_str, rule->columnsplit);
				}
				g_string_append(replacement_str, columns[j]);
			}

			/* lineend */
			g_string_append(replacement_str, rule->lineend);

			/* linesplit */
			if (rows[i+1] != NULL && !utils_str_equal(rule->linesplit, ""))
			{
				g_string_append(replacement_str, rule->linesplit);
			}
			g_string_append(replacement_str, editor_get_eol_char(doc->editor));
			g_strfreev(columns);
		}
	}

	if (header == TRUE)
	{
		g_string_append(replacement_str, rule->body_end);
	}

	/* Adding the footer of table */
	g_string_append(replacement_str, rule->end);

	return g_string_free(replacement_str, FALSE);
}


void convert_to_table(gboolean header, gint file_type)
{
	GeanyDocument *doc = NULL;
	doc = document_get_current();

	g_return_if_fail(doc != NULL);

	if (sci_has_selection(doc->editor->sci))
	{
		gchar *selection = NULL;
		gchar **rows = NULL;
		gchar *replacement = NULL;
		GString *selection_str = NULL;

		/* Actually grabbing selection and splitting it into single
		 * lines we will work on later */
		selection = sci_get_selection_contents(doc->editor->sci);

		selection_str = g_string_new(selection);
		utils_string_replace_all(selection_str, "\r\n", "\n");

		g_free(selection);
		selection = g_string_free(selection_str, FALSE);

		rows = g_strsplit_set(selection, "\n", -1);
		g_free(selection);

		/* Checking whether we do have something we can work on - Returning if not */
		if (rows != NULL)
		{
			if (file_type == -1)
			{
				switch (doc->file_type->id)
				{
					case GEANY_FILETYPES_NONE:
					{
						g_strfreev(rows);
						return;
					}
					case GEANY_FILETYPES_HTML:
					case GEANY_FILETYPES_MARKDOWN:
					{
						replacement = convert_to_table_worker(rows,
							header,
							TC_HTML);
						break;
					}
					case GEANY_FILETYPES_LATEX:
					{
						replacement = convert_to_table_worker(rows,
							header,
							TC_LATEX);
						break;
					}
					case GEANY_FILETYPES_SQL:
					{
						/* TODO: Check for INTEGER and other datatypes on SQL */
						replacement = convert_to_table_worker(rows,
							FALSE,
							TC_SQL);
						break;
					}
					default:
					{
						/* We just don't do anything */
					}
				} /* filetype switch */
			}
			else
			{
				if (file_type == TC_SQL)
				{
					header = FALSE;
				}
				replacement = convert_to_table_worker(rows,
							header,
							file_type);
			}

		}
		else
		{
			/* OK. Something went not as expected.
			 * We do have a selection but cannot parse it into rows.
			 * Aborting */
			g_warning(_("Something went wrong on parsing selection. Aborting"));
			return;
		}

		/* The replacement should have been prepared at this point. Let's go
		 * on and put it into document and replace selection with it. */
		if (replacement != NULL)
		{
			sci_replace_sel(doc->editor->sci, replacement);
		}
		g_strfreev(rows);
		g_free(replacement);
	}
	   /* in case of there was no selection we are just doing nothing */
	return;
}


static void kb_convert_to_table(G_GNUC_UNUSED guint key_id)
{
	g_return_if_fail(document_get_current() != NULL);

	convert_to_table(TRUE, -1);
}


static void init_keybindings(void)
{
	GeanyKeyGroup *key_group;
	key_group = plugin_set_key_group(geany_plugin, "htmltable", COUNT_KB, NULL);
	keybindings_set_item(key_group, KB_HTMLTABLE_CONVERT_TO_TABLE,
		kb_convert_to_table, 0, 0, "convert_to_table",
		_("Convert selection to table"), NULL);
}

void cb_table_convert(G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer gdata)
{
	convert_to_table(TRUE, -1);
}

void cb_table_convert_type(G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer gdata)
{
	convert_to_table(TRUE, GPOINTER_TO_INT(gdata));
}

void plugin_init(GeanyData *data)
{
	init_keybindings();
	init_menuentries();
}


void plugin_cleanup(void)
{
	gtk_widget_destroy(main_menu_item);
	gtk_widget_destroy(menu_tableconvert);
}
