 /*
 *      templates.c
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

#include "templates.h"

GString *glatex_get_template_from_file(gchar *filepath)
{
	gchar *template = NULL;
	GString *return_value = NULL;

	if (filepath == NULL) return NULL;

	g_file_get_contents(filepath, &template, NULL, NULL);

	return_value = g_string_new(template);
	if (template != NULL)
		g_free(template);
	return return_value;
}


static void glatex_init_cutom_template_item(gchar *file, GPtrArray *array)
{
	TemplateEntry *template = g_new0(TemplateEntry, 1);
	gchar *tmp = NULL;

	/* Return if its not a searched file */
	if (g_str_has_suffix(file,".gtl") == FALSE)
		return;

	template->filepath = g_strdup(file);

	tmp = g_path_get_basename(file);
	template->label = utils_remove_ext_from_filename(tmp);
	g_free(tmp);

	/* Adding struct to array */
	template->template = glatex_get_template_from_file(file);
	g_ptr_array_add(array, template);
}


GPtrArray* glatex_init_custom_templates()
{
	gchar *tmp_basedir = NULL;
	GSList *file_list = NULL;
	GPtrArray *templates = NULL;

	/* Creating up config dir for checking for templates */
	tmp_basedir = g_strconcat(geany->app->configdir,
			G_DIR_SEPARATOR_S, "plugins", G_DIR_SEPARATOR_S,
			"geanyLaTeX", G_DIR_SEPARATOR_S, NULL);

	/* Putting all files in configdir to a file list */
	file_list = utils_get_file_list_full(tmp_basedir, TRUE, TRUE, NULL);

	/* Init GPtrArray */
	templates = g_ptr_array_new();

	/* Iterating on all list items */
	g_slist_foreach(file_list, (GFunc)glatex_init_cutom_template_item, templates);
 	g_slist_foreach(file_list, (GFunc) g_free, NULL);
	g_slist_free(file_list);
	return templates;
}

/* Frees all elelements of struct */
void glatex_free_template_entry(TemplateEntry *template, G_GNUC_UNUSED gpointer *data){
	if (template->label != NULL)
		g_free(template->filepath);
	if (template->label != NULL)
		g_string_free(template->template, TRUE);
}


void glatex_add_templates_to_combobox(GPtrArray *templates, GtkWidget *combobox)
{
	guint i;
	TemplateEntry *tmp;
	for (i = 0; i < templates->len; i++)
	{
		tmp = g_ptr_array_index(templates,i);
		gtk_combo_box_append_text(GTK_COMBO_BOX(combobox),
			tmp->label);
	}
}
