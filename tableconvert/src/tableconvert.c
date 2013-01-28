/*
 *	  tableconvert.c
 *
 *	  Copyright 2011-2013 Frank Lanitz <frank(at)frank(dot)uvena(dot)de>
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

#include "geanyplugin.h"

GeanyPlugin     *geany_plugin;
GeanyData       *geany_data;
GeanyFunctions  *geany_functions;

PLUGIN_VERSION_CHECK(200)

PLUGIN_SET_TRANSLATABLE_INFO(
    LOCALEDIR, GETTEXT_PACKAGE, _("Tableconvert"),
    _("A little plugin to convert lists into tables"),
    VERSION, "Frank Lanitz <frank@frank.uvena.de>")

enum
{
	KB_HTMLTABLE_CONVERT_TO_TABLE,
	COUNT_KB
};

typedef struct {
	const gchar *start;
	const gchar *header_start;
	const gchar *header_stop;
	const gchar *body_start;
	const gchar *body_end;
	const gchar *columnsplit;
	const gchar *linestart;
	const gchar *lineend;
	const gchar *linesplit;
	const gchar *end;
} TableConvertRule;

enum {
	TC_LATEX = 0,
	TC_HTML,
	TC_SQL
};

TableConvertRule tablerules[] = {
	/* LaTeX */
	{
		"\\begin{table}[h]\n\\begin{tabular}{}\n", 
		"", 
		"", 
		"", 
		"", 
		" & ", 
		"\t",
		"\\\\",
		"\n",
		"\\end{tabular}\n\\end{table}"
	},
	/* HTML */
	{
		"<table>\n", 
		"<thead>\n", 
		"</thead>\n", 
		"<tbody\n\t<tr>\n\t\t<td>", 
		"</td>\n</tr>\n</tbody>", 
		"</td>\n\t<td>",
		"<tr>\n\t<td>",
		"</td>\n</tr>",
		"\n",
		"\n</table>"
	},
	/* SQL */
	{
		"",   
		"",   
		"",   
		"",   
		"",   
		",",  
		"\t(",
		")",  
		",\n",	
		";"
	}
};

static GtkWidget *main_menu_item = NULL;

static GString* convert_to_table_worker(gchar **rows, gboolean header, gint type)
{
	guint i;
	guint j;
	GString *replacement_str = NULL;

	g_return_val_if_fail(rows != NULL, NULL);
	
	/* Adding start of table to replacement */
	replacement_str = g_string_new(tablerules[type].start);

	/* Adding special header if requested 
	 * e.g. <thead> */
	if (header == TRUE)
	{
		g_string_append(replacement_str, tablerules[type].header_start);
	}

	/* Iteration onto rows and building up lines of table for
	 * replacement */
	for (i = 0; rows[i] != NULL ; i++)
	{
		gchar **columns = NULL;
		columns = g_strsplit_set(rows[i], "\t", -1);
		
		if (i == 0 &&
			header == TRUE)
		{
			g_string_append(replacement_str, tablerules[type].header_stop);
		}

		g_string_append(replacement_str, tablerules[type].linestart);
		
		for (j = 0; columns[j] != NULL; j++)
		{
			if (j > 0)
			{
				g_string_append(replacement_str, tablerules[type].columnsplit);
			}
			g_string_append(replacement_str, columns[j]);
		}

		g_string_append(replacement_str, tablerules[type].lineend);
		
		if (rows[i+1] != NULL)
		{
			g_string_append(replacement_str, tablerules[type].linesplit);
		}
		g_free(columns);
	}
	
	/* Adding the footer of table */
	
	g_string_append(replacement_str, tablerules[type].end);
	return replacement_str;
}


static GString* convert_to_table_html(gchar **rows, gboolean header)
{
	guint i;
	guint j;
	GString *replacement_str = NULL;

	g_return_val_if_fail(rows != NULL, NULL);

	/* Adding header to replacement */
	replacement_str = g_string_new("<table>\n");

	/* Adding <thead> if requested */
	if (header == TRUE)
	{
		g_string_append(replacement_str, "<thead>\n");
	}

	/* Iteration onto rows and building up lines of table for
	 * replacement */
	for (i = 0; rows[i] != NULL ; i++)
	{
		gchar **columns = NULL;
		columns = g_strsplit_set(rows[i], "\t", -1);

		/* Adding <tbody> after first line if header and body
		 * is requested */
		if (i == 1 &&
			header == TRUE)
		{
			g_string_append(replacement_str, "<tbody>\n");
		}

		g_string_append(replacement_str, "\t<tr>\n");
		for (j = 0; columns[j] != NULL; j++)
		{
			g_string_append(replacement_str, "\t\t<td>");
			g_string_append(replacement_str, columns[j]);
			g_string_append(replacement_str, "</td>\n");
		}

		g_string_append(replacement_str, "\t</tr>\n");

		/* Adding closing </thead> after first row if header
		 * is requested */
		if (i == 0 &&
			header == TRUE)
		{
			g_string_append(replacement_str, "</thead>\n");
		}
		g_free(columns);
	}

	/* Adding the footer of table */
	/* Closing </tbody> if requested */
	if (header == TRUE)
	{
		g_string_append(replacement_str, "</tbody>\n");
	}

	g_string_append(replacement_str, "</table>\n");
	return replacement_str;
}


static GString* convert_to_table_sql(gchar** rows)
{
	guint i;
	guint j;
	GString *replacement_str = NULL;

	g_return_val_if_fail(rows != NULL, NULL);

	/* Adding start */
	replacement_str = g_string_new("");

	/* Iteration onto rows and building up lines for replacement */
	for (i = 0; rows[i] != NULL ; i++)
	{
		gchar **columns = NULL;

		g_string_append(replacement_str, "\t('");
		columns = g_strsplit_set(rows[i], "\t", -1);

		for (j = 0; columns[j] != NULL; j++)
		{
			if (j > 0)
			{
				g_string_append(replacement_str, "','");
			}
			g_string_append(replacement_str, columns[j]);
		}

		if (rows[i+1] != NULL)
		{
			g_string_append(replacement_str, "'),\n");
		}
		else
		{
			g_string_append(replacement_str, "')\n");
		}

		g_free(columns);
	}
	return replacement_str;
}

static void convert_to_table(gboolean header)
{
	GeanyDocument *doc = NULL;
	doc = document_get_current();

	g_return_if_fail(doc != NULL);

	if (sci_has_selection(doc->editor->sci))
	{
		gchar *selection = NULL;
		gchar **rows = NULL;
		GString *replacement_str = NULL;
		gchar *replacement = NULL;

		/* Actually grabbing selection and splitting it into single
		 * lines we will work on later */
		selection = sci_get_selection_contents(doc->editor->sci);
		rows = g_strsplit_set(selection, "\r\n", -1);
		g_free(selection);

		/* Checking whether we do have something we can work on - Returning if not */
		if (rows != NULL)
		{
			switch (doc->file_type->id)
			{
				case GEANY_FILETYPES_NONE:
				{
					g_free(rows);
					g_free(replacement);
					return;
				}
				case GEANY_FILETYPES_HTML:
				{
					replacement_str = convert_to_table_worker(rows, header, TC_HTML);
					break;
				}
				case GEANY_FILETYPES_LATEX:
				{
					replacement_str = convert_to_table_worker(rows, header, TC_LATEX);
					break;
				}
				case GEANY_FILETYPES_SQL:
				{
					replacement_str = convert_to_table_worker(rows, header, TC_SQL);
					break;
				}
				default:
				{
					replacement_str = NULL;
				}
			} /* filetype switch */
		}
		else
		{
			/* OK. Something went not as expected.
			* We did have a selection but cannot parse it into rows.
			* Aborting */
			g_warning(_("Something went wrong on parsing selection. Aborting"));
			return;
		}

		/* The replacement should have been prepared at this point. Let's go
		* on and put it into document and replace selection with it. */
		if (replacement_str != NULL)
		{
			replacement = g_string_free(replacement_str, FALSE);
			sci_replace_sel(doc->editor->sci, replacement);
		}
		g_free(rows);
		g_free(replacement);
	}
	   /* in case of there was no selection we are just doing nothing */
	return;
}

static void kb_convert_to_table(G_GNUC_UNUSED guint key_id)
{
	g_return_if_fail(document_get_current() != NULL);
	convert_to_table(TRUE);
}


static void init_keybindings(void)
{
	GeanyKeyGroup *key_group;
	key_group = plugin_set_key_group(geany_plugin, "htmltable", COUNT_KB, NULL);
	keybindings_set_item(key_group, KB_HTMLTABLE_CONVERT_TO_TABLE,
		kb_convert_to_table, 0, 0, "convert_to_table",
		_("Convert selection to table"), NULL);
}

static void cb_table_convert(G_GNUC_UNUSED GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer gdata)
{
	convert_to_table(TRUE);
}

void plugin_init(GeanyData *data)
{
	init_keybindings();

	/* Build up menu entry */
	main_menu_item = gtk_menu_item_new_with_mnemonic(_("_Convert to table"));
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu), main_menu_item);
	ui_widget_set_tooltip_text(main_menu_item,
		_("Converts current marked list to a table."));
	g_signal_connect(G_OBJECT(main_menu_item), "activate", G_CALLBACK(convert_to_table), NULL);
	gtk_widget_show_all(main_menu_item);
	ui_add_document_sensitive(main_menu_item);
}


void plugin_cleanup(void)
{
	gtk_widget_destroy(main_menu_item);
}
