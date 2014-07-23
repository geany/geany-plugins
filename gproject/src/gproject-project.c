/*
 * Copyright 2010 Jiri Techet <techet@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <sys/time.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gstdio.h>

#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif
#include <geanyplugin.h>

#include "gproject-utils.h"
#include "gproject-project.h"

extern GeanyPlugin *geany_plugin;
extern GeanyData *geany_data;
extern GeanyFunctions *geany_functions;

typedef struct
{
	GtkWidget *source_patterns;
	GtkWidget *header_patterns;
	GtkWidget *ignored_dirs_patterns;
	GtkWidget *generate_tags;
} PropertyDialogElements;

GPrj *g_prj = NULL;

static PropertyDialogElements *e;

typedef enum {DeferredTagOpAdd, DeferredTagOpRemove} DeferredTagOpType;

typedef struct
{
	gchar * filename;
	DeferredTagOpType type;
} DeferredTagOp;

static GSList *file_tag_deferred_op_queue = NULL;
static gboolean flush_queued = FALSE;


static void deferred_op_free(DeferredTagOp* op, G_GNUC_UNUSED gpointer user_data)
{
	g_free(op->filename);
	g_free(op);
}


static void deferred_op_queue_clean(void)
{
	g_slist_foreach(file_tag_deferred_op_queue, (GFunc)deferred_op_free, NULL);
	g_slist_free(file_tag_deferred_op_queue);
	file_tag_deferred_op_queue = NULL;
        flush_queued = FALSE;
}


static void workspace_add_tag(gchar *filename, TagObject *obj, gpointer foo)
{
	TMWorkObject *tm_obj = NULL;

	if (!document_find_by_filename(filename))
	{
		gchar *locale_filename;

		locale_filename = utils_get_locale_from_utf8(filename);
		tm_obj = tm_source_file_new(locale_filename, FALSE, filetypes_detect_from_file(filename)->name);
		g_free(locale_filename);

		if (tm_obj)
		{
			tm_workspace_add_object(tm_obj);
			tm_source_file_update(tm_obj, TRUE, FALSE, TRUE);
		}
	}

	if (obj->tag)
		tm_workspace_remove_object(obj->tag, TRUE, TRUE);

	obj->tag = tm_obj;
}


static void workspace_add_file_tag(gchar *filename)
{
	TagObject *obj;

	obj = g_hash_table_lookup(g_prj->file_tag_table, filename);
	if (obj)
		workspace_add_tag(filename, obj, NULL);
}


static void workspace_remove_tag(gchar *filename, TagObject *obj, gpointer foo)
{
	if (obj->tag)
	{
		tm_workspace_remove_object(obj->tag, TRUE, TRUE);
		obj->tag = NULL;
	}
}


static void workspace_remove_file_tag(gchar *filename)
{
	TagObject *obj;

	obj = g_hash_table_lookup(g_prj->file_tag_table, filename);
	if (obj)
		workspace_remove_tag(filename, obj, NULL);
}


static void deferred_op_queue_dispatch(DeferredTagOp* op, G_GNUC_UNUSED gpointer user_data)
{
	if (op->type == DeferredTagOpAdd)
		workspace_add_file_tag(op->filename);
	else if (op->type == DeferredTagOpRemove)
		workspace_remove_file_tag(op->filename);
}


static gboolean deferred_op_queue_flush(G_GNUC_UNUSED gpointer data)
{
	g_slist_foreach(file_tag_deferred_op_queue,
					(GFunc)deferred_op_queue_dispatch, NULL);
	deferred_op_queue_clean();
	flush_queued = FALSE;

	return FALSE; /* returning false removes this callback; it is a one-shot */
}


static void deferred_op_queue_enqueue(gchar* filename, DeferredTagOpType type)
{
	DeferredTagOp * op;

	op = (DeferredTagOp *) g_new0(DeferredTagOp, 1);
	op->type = type;
	op->filename = g_strdup(filename);

	file_tag_deferred_op_queue = g_slist_prepend(file_tag_deferred_op_queue,op);

	if (!flush_queued)
	{
		flush_queued = TRUE;
		plugin_idle_add(geany_plugin, (GSourceFunc)deferred_op_queue_flush, NULL);
	}
}


/* path - absolute path in locale, returned list in locale */
static GSList *get_file_list(const gchar * path, GSList *patterns, GSList *ignored_dirs_patterns)
{
	GSList *list = NULL;
	GDir *dir;

	dir = g_dir_open(path, 0, NULL);
	if (!dir)
		return NULL;

	while (TRUE)
	{
		const gchar *name;
		gchar *filename;

		name = g_dir_read_name(dir);
		if (!name)
			break;

		filename = g_build_filename(path, name, NULL);

		if (g_file_test(filename, G_FILE_TEST_IS_DIR))
		{
			GSList *lst;

			if (patterns_match(ignored_dirs_patterns, name))
			{
				g_free(filename);
				continue;
			}

			lst = get_file_list(filename, patterns, ignored_dirs_patterns);
			if (lst)
				list = g_slist_concat(list, lst);
			g_free(filename);
		}
		else if (g_file_test(filename, G_FILE_TEST_IS_REGULAR))
		{
			if (patterns_match(patterns, name))
				list = g_slist_prepend(list, filename);
			else
				g_free(filename);
		}
	}

	g_dir_close(dir);

	return list;
}


void gprj_project_rescan(void)
{
	GSList *pattern_list = NULL;
	GSList *ignored_dirs_list = NULL;
	GSList *lst;
	GSList *elem;

	if (!g_prj)
		return;

	if (g_prj->generate_tags)
		g_hash_table_foreach(g_prj->file_tag_table, (GHFunc)workspace_remove_tag, NULL);
	g_hash_table_destroy(g_prj->file_tag_table);
	g_prj->file_tag_table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	deferred_op_queue_clean();

	pattern_list = get_precompiled_patterns(geany_data->app->project->file_patterns);

	ignored_dirs_list = get_precompiled_patterns(g_prj->ignored_dirs_patterns);

	lst = get_file_list(geany_data->app->project->base_path, pattern_list, ignored_dirs_list);

	for (elem = lst; elem != NULL; elem = g_slist_next(elem))
	{
		char *path;
		TagObject *obj;

		obj = g_new0(TagObject, 1);
		obj->tag = NULL;

		path = tm_get_real_path(elem->data);
		if (path)
		{
			setptr(path, utils_get_utf8_from_locale(path));
			g_hash_table_insert(g_prj->file_tag_table, path, obj);
		}
	}

	if (g_prj->generate_tags)
		g_hash_table_foreach(g_prj->file_tag_table, (GHFunc)workspace_add_tag, NULL);

	g_slist_foreach(lst, (GFunc) g_free, NULL);
	g_slist_free(lst);

	g_slist_foreach(pattern_list, (GFunc) g_pattern_spec_free, NULL);
	g_slist_free(pattern_list);

	g_slist_foreach(ignored_dirs_list, (GFunc) g_pattern_spec_free, NULL);
	g_slist_free(ignored_dirs_list);
}


static void update_project(
	gchar **source_patterns,
	gchar **header_patterns,
	gchar **ignored_dirs_patterns,
	gboolean generate_tags)
{
	if (g_prj->source_patterns)
		g_strfreev(g_prj->source_patterns);
	g_prj->source_patterns = g_strdupv(source_patterns);

	if (g_prj->header_patterns)
		g_strfreev(g_prj->header_patterns);
	g_prj->header_patterns = g_strdupv(header_patterns);

	if (g_prj->ignored_dirs_patterns)
		g_strfreev(g_prj->ignored_dirs_patterns);
	g_prj->ignored_dirs_patterns = g_strdupv(ignored_dirs_patterns);

	g_prj->generate_tags = generate_tags;

	gprj_project_rescan();
}


void gprj_project_save(GKeyFile * key_file)
{
	if (!g_prj)
		return;

	g_key_file_set_string_list(key_file, "gproject", "source_patterns",
		(const gchar**) g_prj->source_patterns, g_strv_length(g_prj->source_patterns));
	g_key_file_set_string_list(key_file, "gproject", "header_patterns",
		(const gchar**) g_prj->header_patterns, g_strv_length(g_prj->header_patterns));
	g_key_file_set_string_list(key_file, "gproject", "ignored_dirs_patterns",
		(const gchar**) g_prj->ignored_dirs_patterns, g_strv_length(g_prj->ignored_dirs_patterns));
	g_key_file_set_boolean(key_file, "gproject", "generate_tags", g_prj->generate_tags);
}


void gprj_project_open(GKeyFile * key_file)
{
	gchar **source_patterns, **header_patterns, **ignored_dirs_patterns;
	gboolean generate_tags;

	if (g_prj != NULL)
		gprj_project_close();

	g_prj = (GPrj *) g_new0(GPrj, 1);

	g_prj->source_patterns = NULL;
	g_prj->header_patterns = NULL;
	g_prj->ignored_dirs_patterns = NULL;
	g_prj->generate_tags = FALSE;

	g_prj->file_tag_table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	deferred_op_queue_clean();

	source_patterns = g_key_file_get_string_list(key_file, "gproject", "source_patterns", NULL, NULL);
	if (!source_patterns)
		source_patterns = g_strsplit("*.c *.C *.cpp *.cxx *.c++ *.cc", " ", -1);
	header_patterns = g_key_file_get_string_list(key_file, "gproject", "header_patterns", NULL, NULL);
	if (!header_patterns)
		header_patterns = g_strsplit("*.h *.H *.hpp *.hxx *.h++ *.hh *.m", " ", -1);
	ignored_dirs_patterns = g_key_file_get_string_list(key_file, "gproject", "ignored_dirs_patterns", NULL, NULL);
	if (!ignored_dirs_patterns)
		ignored_dirs_patterns = g_strsplit(".* CVS", " ", -1);
	generate_tags = utils_get_setting_boolean(key_file, "gproject", "generate_tags", FALSE);

	update_project(
		source_patterns,
		header_patterns,
		ignored_dirs_patterns,
		generate_tags);

	g_strfreev(source_patterns);
	g_strfreev(header_patterns);
	g_strfreev(ignored_dirs_patterns);
}


static gchar **split_patterns(const gchar *str)
{
	GString *tmp;
	gchar **ret;
	gchar *input;

	input = g_strdup(str);

	g_strstrip(input);
	tmp = g_string_new(input);
	g_free(input);
	do {} while (utils_string_replace_all(tmp, "  ", " "));
	ret = g_strsplit(tmp->str, " ", -1);
	g_string_free(tmp, TRUE);
	return ret;
}


void gprj_project_read_properties_tab(void)
{
	gchar **source_patterns, **header_patterns, **ignored_dirs_patterns;

	source_patterns = split_patterns(gtk_entry_get_text(GTK_ENTRY(e->source_patterns)));
	header_patterns = split_patterns(gtk_entry_get_text(GTK_ENTRY(e->header_patterns)));
	ignored_dirs_patterns = split_patterns(gtk_entry_get_text(GTK_ENTRY(e->ignored_dirs_patterns)));

	update_project(
		source_patterns, header_patterns, ignored_dirs_patterns,
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(e->generate_tags)));

	g_strfreev(source_patterns);
	g_strfreev(header_patterns);
	g_strfreev(ignored_dirs_patterns);
}


gint gprj_project_add_properties_tab(GtkWidget *notebook)
{
	GtkWidget *vbox, *hbox, *hbox1;
	GtkWidget *table;
	GtkWidget *label;
	gchar *str;
	gint page_index;

	e = g_new0(PropertyDialogElements, 1);

	vbox = gtk_vbox_new(FALSE, 0);

	table = gtk_table_new(3, 2, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 5);
	gtk_table_set_col_spacings(GTK_TABLE(table), 10);

	label = gtk_label_new(_("Source patterns:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	e->source_patterns = gtk_entry_new();
	ui_table_add_row(GTK_TABLE(table), 0, label, e->source_patterns, NULL);
	ui_entry_add_clear_icon(GTK_ENTRY(e->source_patterns));
	ui_widget_set_tooltip_text(e->source_patterns,
		_("Space separated list of patterns that are used to identify source files."));
	str = g_strjoinv(" ", g_prj->source_patterns);
	gtk_entry_set_text(GTK_ENTRY(e->source_patterns), str);
	g_free(str);

	label = gtk_label_new(_("Header patterns:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	e->header_patterns = gtk_entry_new();
	ui_entry_add_clear_icon(GTK_ENTRY(e->header_patterns));
	ui_table_add_row(GTK_TABLE(table), 1, label, e->header_patterns, NULL);
	ui_widget_set_tooltip_text(e->header_patterns,
		_("Space separated list of patterns that are used to identify headers. "
		  "Used mainly for header/source swapping."));
	str = g_strjoinv(" ", g_prj->header_patterns);
	gtk_entry_set_text(GTK_ENTRY(e->header_patterns), str);
	g_free(str);

	label = gtk_label_new(_("Ignored dirs patterns:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	e->ignored_dirs_patterns = gtk_entry_new();
	ui_entry_add_clear_icon(GTK_ENTRY(e->ignored_dirs_patterns));
	ui_table_add_row(GTK_TABLE(table), 2, label, e->ignored_dirs_patterns, NULL);
	ui_widget_set_tooltip_text(e->ignored_dirs_patterns,
		_("Space separated list of patterns that are used to identify directories "
		  "that are not scanned for source files."));
	str = g_strjoinv(" ", g_prj->ignored_dirs_patterns);
	gtk_entry_set_text(GTK_ENTRY(e->ignored_dirs_patterns), str);
	g_free(str);

	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 6);

	e->generate_tags = gtk_check_button_new_with_label(_("Generate tags for all project files"));
	ui_widget_set_tooltip_text(e->generate_tags,
		_("Generate tag list for all project files instead of only for the currently opened files. "
		  "Too slow for big projects (>1000 files) and should be disabled in this case."));
	gtk_box_pack_start(GTK_BOX(vbox), e->generate_tags, FALSE, FALSE, 6);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(e->generate_tags), g_prj->generate_tags);

	hbox1 = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Note: set the patterns of files belonging to the project under the Project tab."));
	gtk_box_pack_start(GTK_BOX(hbox1), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox1, FALSE, FALSE, 6);

	label = gtk_label_new(_("GProject"));

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 6);

	page_index = gtk_notebook_append_page(GTK_NOTEBOOK(notebook), hbox, label);
	gtk_widget_show_all(notebook);

	return page_index;
}


void gprj_project_close(void)
{
	g_return_if_fail(g_prj);

	if (g_prj->generate_tags)
		g_hash_table_foreach(g_prj->file_tag_table, (GHFunc)workspace_remove_tag, NULL);

	deferred_op_queue_clean();

	g_strfreev(g_prj->source_patterns);
	g_strfreev(g_prj->header_patterns);
	g_strfreev(g_prj->ignored_dirs_patterns);

	g_hash_table_destroy(g_prj->file_tag_table);

	g_free(g_prj);
	g_prj = NULL;
}


void gprj_project_add_file_tag(gchar *filename)
{
	deferred_op_queue_enqueue(filename, DeferredTagOpAdd);
}


void gprj_project_remove_file_tag(gchar *filename)
{
	deferred_op_queue_enqueue(filename, DeferredTagOpRemove);
}


gboolean gprj_project_is_in_project(const gchar * filename)
{
	return filename && g_prj && geany_data->app->project &&
		g_hash_table_lookup(g_prj->file_tag_table, filename) != NULL;
}
