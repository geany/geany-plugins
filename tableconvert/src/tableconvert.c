/*
 *	  tableconvert.c
 *
 *	  Copyright 2011 Frank Lanitz <frank(at)frank(dot)uvena(dot)de>
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

#include "geanyplugin.h"

GeanyPlugin     *geany_plugin;
GeanyData       *geany_data;
GeanyFunctions  *geany_functions;

PLUGIN_VERSION_CHECK(200)

PLUGIN_SET_TRANSLATABLE_INFO(
    LOCALEDIR, GETTEXT_PACKAGE, _("Tableconvert"),
    _("A little plugin to convert lists into tables"),
    VERSION, "Frank Lanitz <frank@frank.uvena.de>");

enum
{
	KB_HTMLTABLE_CONVERT_TO_TABLE,
	COUNT_KB
};


static GtkWidget *main_menu_item = NULL;

void convert_to_table(gboolean header)
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
			guint i;
			guint j;

			if (doc->file_type->id == GEANY_FILETYPES_HTML)
			{
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
			}

			else if (doc->file_type->id == GEANY_FILETYPES_LATEX)
			{

				/* Adding header to replacement */
				replacement_str = g_string_new("\\begin{tabular}{}\n");

				/* Iteration onto rows and building up lines of table for
				* replacement */
				for (i = 0; rows[i] != NULL ; i++)
				{
					gchar **columns = NULL;
					columns = g_strsplit_set(rows[i], "\t", -1);

					for (j = 0; columns[j] != NULL; j++)
					{
						if (j > 0)
						{
							g_string_append(replacement_str, "  &  ");
						}
						g_string_append(replacement_str, columns[j]);
					}

					g_string_append(replacement_str, "\\\\\n");

					g_free(columns);
				}
				/* Adding the footer of table */

				g_string_append(replacement_str, "\\end{tabular}\n");
			}
		}
		else
		{
			/* OK. Something went not as expected.
			* We did have a selection but cannot parse it into rows.
			* Aborting */
			g_warning(_("Something went went wrong on parsing selection. Aborting"));
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

void kb_convert_to_table(G_GNUC_UNUSED guint key_id)
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


void plugin_init(GeanyData *data)
{
	init_keybindings();

	main_locale_init(LOCALEDIR, GETTEXT_PACKAGE);

	/* Build up menu entry */
	main_menu_item = gtk_menu_item_new_with_mnemonic(_("_Convert to table"));
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu), main_menu_item);
	ui_widget_set_tooltip_text(main_menu_item,
		_("Converts current marked list to a table."));
	g_signal_connect(G_OBJECT(main_menu_item), "activate", G_CALLBACK(convert_to_table), TRUE);
	gtk_widget_show_all(main_menu_item);
	ui_add_document_sensitive(main_menu_item);

}


void plugin_cleanup(void)
{
	gtk_widget_destroy(main_menu_item);
}
