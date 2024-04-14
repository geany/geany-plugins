/*
 *	  latexenvironments.c
 *
 *	  Copyright 2009-2024 Frank Lanitz <frank(at)frank(dot)uvena(dot)de>
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

#include "latexenvironments.h"

CategoryName glatex_environment_cat_names[] = {
	{ ENVIRONMENT_CAT_DUMMY, N_("Environments"), TRUE},
	{ ENVIRONMENT_CAT_FORMAT, N_("Formatting"), TRUE},
	{ ENVIRONMENT_CAT_STRUCTURE, N_("Document Structure"), TRUE},
	{ ENVIRONMENT_CAT_LISTS, N_("Lists"), TRUE},
	{ ENVIRONMENT_CAT_MATH, N_("Math"), TRUE},
	{ 0, NULL, FALSE}
};


SubMenuTemplate glatex_environment_array[] = {
	/* General document structure environments */
	{ENVIRONMENT_CAT_STRUCTURE, "block", "block"},
	{ENVIRONMENT_CAT_STRUCTURE, "document", "document"},
	{ENVIRONMENT_CAT_STRUCTURE, "frame", "frame"},
	{ENVIRONMENT_CAT_STRUCTURE, "table", "table"},
	{ENVIRONMENT_CAT_STRUCTURE, "tabular", "tabular"},
	{ENVIRONMENT_CAT_STRUCTURE, "figure", "figure"},
	/* Lists */
	{ENVIRONMENT_CAT_LISTS, "itemize", "itemize"},
	{ENVIRONMENT_CAT_LISTS, "enumerate", "enumerate"},
	{ENVIRONMENT_CAT_LISTS, "description", "description"},
	/* Math stuff */
	{ENVIRONMENT_CAT_MATH, "displaymath", "displaymath"},
	{ENVIRONMENT_CAT_MATH, "equation", "equation"},
	{ENVIRONMENT_CAT_MATH, "eqnarray", "eqnarray"},

	{0, NULL, NULL},
};

const gchar *glatex_list_environments [] =
{
	"description",
	"enumerate",
	"itemize"
};


/* if type == -1 then we will try to autodetect the type */
void glatex_insert_environment(const gchar *environment, gint type)
{
	GeanyDocument *doc = NULL;

	doc = document_get_current();

	/* Only do anything if it is realy needed to */
	if (doc != NULL && environment != NULL)
	{
		/* Checking whether we do have a selection */
		if (sci_has_selection(doc->editor->sci))
		{
			gchar *selection  = NULL;
			gchar *replacement = NULL;
			selection = sci_get_selection_contents(doc->editor->sci);

			sci_start_undo_action(doc->editor->sci);
			if (utils_str_equal(environment, "block") == TRUE)
			{
				replacement = g_strconcat("\\begin{", environment, "}{}\n",
							  selection, "\n\\end{", environment, "}\n", NULL);
			}
			else
			{
				replacement = g_strconcat("\\begin{", environment, "}\n",
							  selection, "\n\\end{", environment, "}\n", NULL);
			}
			sci_replace_sel(doc->editor->sci, replacement);
			sci_end_undo_action(doc->editor->sci);
			g_free(selection);
			g_free(replacement);

		}
		else /* No selection found*/
		{
			gchar *tmpstring = NULL;

			if (type == -1)
			{
				gint i;

				/* First, we check whether we have a known list over here
				 * an reset type to fit new value*/
				for (i = 0; i < GLATEX_LIST_END; i++)
				{
					if (utils_str_equal(glatex_list_environments[i], environment) == TRUE)
					{
						type = GLATEX_ENVIRONMENT_TYPE_LIST;
						break;
					}
				}
			}
			sci_start_undo_action(doc->editor->sci);

			if (utils_str_equal(environment, "block") == TRUE)
			{
				tmpstring = g_strconcat(
					"\\begin{",
					environment,
					"}{}\n%cursor%\n\\end{",
					environment,
					"}");
			}
			else /* We don't have a block-like environment */
			{
				if (type == GLATEX_ENVIRONMENT_TYPE_LIST)
				{
					tmpstring = g_strconcat(
						"\\begin{",
						environment,
						"}\n\t\\item %cursor% \n\\end{",
						environment,
						"}");
				}
				else
				{
					tmpstring = g_strconcat(
						"\\begin{",
						environment,
						"}\n%cursor%\n\\end{",
						environment,
						"}");
				}
			}
			glatex_insert_snippet(tmpstring);
			g_free(tmpstring);
			sci_end_undo_action(doc->editor->sci);
		}
	}
}


void
glatex_environment_insert_activated (G_GNUC_UNUSED GtkMenuItem *menuitem,
							  G_GNUC_UNUSED gpointer gdata)
{
	gint env = GPOINTER_TO_INT(gdata);

	if (glatex_environment_array[env].cat == ENVIRONMENT_CAT_LISTS)
		glatex_insert_environment(glatex_environment_array[env].latex,
			GLATEX_ENVIRONMENT_TYPE_LIST);
	else
		glatex_insert_environment(glatex_environment_array[env].latex,
			GLATEX_ENVIRONMENT_TYPE_NONE);
}


void
glatex_insert_environment_dialog(G_GNUC_UNUSED GtkMenuItem *menuitem,
								 G_GNUC_UNUSED gpointer gdata)
{
	GtkWidget *dialog = NULL;
	GtkWidget *vbox = NULL;
	GtkWidget *label_env = NULL;
	GtkWidget *textbox_env = NULL;
	GtkWidget *table = NULL;
	GtkWidget *tmp_entry = NULL;
	GtkTreeModel *model = NULL;
	gint i, max;

	dialog = gtk_dialog_new_with_buttons(_("Insert Environment"),
				GTK_WINDOW(geany->main_widgets->window),
				GTK_DIALOG_DESTROY_WITH_PARENT, GTK_STOCK_CANCEL,
				GTK_RESPONSE_CANCEL, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
				NULL);

	vbox = ui_dialog_vbox_new(GTK_DIALOG(dialog));
	gtk_widget_set_name(dialog, "GeanyDialog");
	gtk_box_set_spacing(GTK_BOX(vbox), 10);

	table = gtk_table_new(1, 2, FALSE);
	gtk_table_set_col_spacings(GTK_TABLE(table), 6);
	gtk_table_set_row_spacings(GTK_TABLE(table), 6);

	label_env = gtk_label_new(_("Environment:"));
	textbox_env = gtk_combo_box_text_new_with_entry();

	max = glatex_count_menu_entries(glatex_environment_array, -1);
	for (i = 0; i < max; i++)
	{
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(textbox_env),
								  glatex_environment_array[i].label);
	}

	model = gtk_combo_box_get_model(GTK_COMBO_BOX(textbox_env));
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(model),
		0, GTK_SORT_ASCENDING);

	gtk_misc_set_alignment(GTK_MISC(label_env), 0, 0.5);

	gtk_table_attach_defaults(GTK_TABLE(table), label_env, 0, 1, 0, 1);
	gtk_table_attach_defaults(GTK_TABLE(table), textbox_env, 1, 2, 0, 1);
	gtk_container_add(GTK_CONTAINER(vbox), table);

	tmp_entry =  gtk_bin_get_child(GTK_BIN(textbox_env));
	g_signal_connect(G_OBJECT(tmp_entry), "activate",
		G_CALLBACK(glatex_enter_key_pressed_in_entry), dialog);

	gtk_widget_show_all(vbox);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
	{
		gchar *env_string = NULL;

		env_string = g_strdup(gtk_combo_box_text_get_active_text(
			GTK_COMBO_BOX_TEXT(textbox_env)));

		if (env_string != NULL)
		{
			glatex_insert_environment(env_string, -1);
			g_free(env_string);
		}
	}

	gtk_widget_destroy(dialog);
}

void glatex_insert_list_environment(gint type)
{
	glatex_insert_environment(glatex_list_environments[type], GLATEX_ENVIRONMENT_TYPE_LIST);
}
