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

#include "prjorg-utils.h"
#include "prjorg-project.h"

extern GeanyPlugin *geany_plugin;
extern GeanyData *geany_data;
extern GeanyFunctions *geany_functions;

typedef struct
{
	GtkWidget *source_patterns;
	GtkWidget *header_patterns;
	GtkWidget *ignored_dirs_patterns;
	GtkWidget *ignored_file_patterns;
	GtkWidget *generate_tag_prefs;
} PropertyDialogElements;

PrjOrg *prj_org = NULL;

static PropertyDialogElements *e;

static GSList *s_idle_add_funcs;
static GSList *s_idle_remove_funcs;


static void clear_idle_queue(GSList **queue)
{
	GSList *elem;

	foreach_slist(elem, *queue)
		g_free(elem->data);
	g_slist_free(*queue);
	*queue = NULL;
}


static void collect_source_files(gchar *filename, TMSourceFile *sf, gpointer user_data)
{
	GPtrArray *array = user_data;

	if (sf != NULL)
		g_ptr_array_add(array, sf);
}


/* path - absolute path in locale, returned list in locale */
static GSList *get_file_list(const gchar *utf8_path, GSList *patterns, GSList *ignored_dirs_patterns, GSList *ignored_file_patterns)
{
	GSList *list = NULL;
	GDir *dir;
	gchar *locale_path = utils_get_locale_from_utf8(utf8_path);

	dir = g_dir_open(locale_path, 0, NULL);
	if (!dir)
	{
		g_free(locale_path);
		return NULL;
	}

	while (TRUE)
	{
		const gchar *locale_name;
		gchar *locale_filename, *utf8_filename, *utf8_name;

		locale_name = g_dir_read_name(dir);
		if (!locale_name)
			break;

		utf8_name = utils_get_utf8_from_locale(locale_name);
		locale_filename = g_build_filename(locale_path, locale_name, NULL);
		utf8_filename = utils_get_utf8_from_locale(locale_filename);

		if (g_file_test(locale_filename, G_FILE_TEST_IS_DIR))
		{
			GSList *lst;
			gchar *relative, *locale_parent_realpath, *locale_child_realpath,
				*utf8_parent_realpath, *utf8_child_realpath;

			/* symlink cycle avoidance - test if directory within parent directory */
			locale_parent_realpath = tm_get_real_path(locale_path);
			locale_child_realpath = tm_get_real_path(locale_filename);
			utf8_parent_realpath = utils_get_utf8_from_locale(locale_parent_realpath);
			utf8_child_realpath = utils_get_utf8_from_locale(locale_child_realpath);

			relative = get_relative_path(utf8_parent_realpath, utf8_child_realpath);

			g_free(locale_parent_realpath);
			g_free(locale_child_realpath);
			g_free(utf8_parent_realpath);
			g_free(utf8_child_realpath);

			if (relative)
			{
				g_free(relative);

				if (!patterns_match(ignored_dirs_patterns, utf8_name))
				{
					lst = get_file_list(utf8_filename, patterns, ignored_dirs_patterns, ignored_file_patterns);
					if (lst)
						list = g_slist_concat(list, lst);
				}
			}
		}
		else if (g_file_test(locale_filename, G_FILE_TEST_IS_REGULAR))
		{
			if (patterns_match(patterns, utf8_name) && !patterns_match(ignored_file_patterns, utf8_name))
				list = g_slist_prepend(list, g_strdup(utf8_filename));
		}

		g_free(utf8_filename);
		g_free(locale_filename);
		g_free(utf8_name);
	}

	g_dir_close(dir);
	g_free(locale_path);

	return list;
}


static gint prjorg_project_rescan_root(PrjOrgRoot *root)
{
	GPtrArray *source_files;
	GSList *pattern_list = NULL;
	GSList *ignored_dirs_list = NULL;
	GSList *ignored_file_list = NULL;
	GSList *lst;
	GSList *elem;
	gint filenum = 0;

	source_files = g_ptr_array_new();
	g_hash_table_foreach(root->file_table, (GHFunc)collect_source_files, source_files);
	tm_workspace_remove_source_files(source_files);
	g_ptr_array_free(source_files, TRUE);
	g_hash_table_remove_all(root->file_table);

	if (!geany_data->app->project->file_patterns || !geany_data->app->project->file_patterns[0])
	{
		gchar **all_pattern = g_strsplit ("*", " ", -1);
		pattern_list = get_precompiled_patterns(all_pattern);
		g_strfreev(all_pattern);
	}
	else
		pattern_list = get_precompiled_patterns(geany_data->app->project->file_patterns);

	ignored_dirs_list = get_precompiled_patterns(prj_org->ignored_dirs_patterns);
	ignored_file_list = get_precompiled_patterns(prj_org->ignored_file_patterns);

	lst = get_file_list(root->base_dir, pattern_list, ignored_dirs_list, ignored_file_list);

	foreach_slist(elem, lst)
	{
		char *path = elem->data;

		if (path)
		{
			g_hash_table_insert(root->file_table, g_strdup(path), NULL);
			filenum++;
		}
	}

	g_slist_foreach(lst, (GFunc) g_free, NULL);
	g_slist_free(lst);

	g_slist_foreach(pattern_list, (GFunc) g_pattern_spec_free, NULL);
	g_slist_free(pattern_list);

	g_slist_foreach(ignored_dirs_list, (GFunc) g_pattern_spec_free, NULL);
	g_slist_free(ignored_dirs_list);

	return filenum;
}


static gboolean match_basename(gconstpointer pft, gconstpointer user_data)
{
	const GeanyFiletype *ft = pft;
	const gchar *utf8_base_filename = user_data;
	gint j;
	gboolean ret = FALSE;

	if (G_UNLIKELY(ft->id == GEANY_FILETYPES_NONE))
		return FALSE;

	for (j = 0; ft->pattern[j] != NULL; j++)
	{
		GPatternSpec *pattern = g_pattern_spec_new(ft->pattern[j]);

		if (g_pattern_match_string(pattern, utf8_base_filename))
		{
			ret = TRUE;
			g_pattern_spec_free(pattern);
			break;
		}
		g_pattern_spec_free(pattern);
	}
	return ret;
}


/* Stolen and modified version from Geany. The only difference is that Geany
 * first looks at shebang inside the file and then, if it fails, checks the
 * file extension. Opening every file is too expensive so instead check just
 * extension and only if this fails, look at the shebang */
static GeanyFiletype *filetypes_detect(const gchar *utf8_filename)
{
	struct stat s;
	GeanyFiletype *ft = NULL;
	gchar *locale_filename;

	locale_filename = utils_get_locale_from_utf8(utf8_filename);
	if (g_stat(locale_filename, &s) != 0 || s.st_size > 10*1024*1024)
		ft = filetypes[GEANY_FILETYPES_NONE];
	else
	{
		guint i;
		gchar *utf8_base_filename;

		/* to match against the basename of the file (because of Makefile*) */
		utf8_base_filename = g_path_get_basename(utf8_filename);
#ifdef G_OS_WIN32
		/* use lower case basename */
		SETPTR(utf8_base_filename, g_utf8_strdown(utf8_base_filename, -1));
#endif

		for (i = 0; i < geany_data->filetypes_array->len; i++)
		{
			GeanyFiletype *ftype = filetypes[i];

			if (match_basename(ftype, utf8_base_filename))
			{
				ft = ftype;
				break;
			}
		}

		if (ft == NULL)
			ft = filetypes_detect_from_file(utf8_filename);

		g_free(utf8_base_filename);
	}

	g_free(locale_filename);

	return ft;
}


static void regenerate_tags(PrjOrgRoot *root, gpointer user_data)
{
	GHashTableIter iter;
	gpointer key, value;
	GPtrArray *source_files;
	GHashTable *file_table;

	source_files = g_ptr_array_new();
	file_table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GFreeFunc)tm_source_file_free);
	g_hash_table_iter_init(&iter, root->file_table);
	while (g_hash_table_iter_next(&iter, &key, &value))
	{
		TMSourceFile *sf;
		gchar *utf8_path = key;
		gchar *locale_path = utils_get_locale_from_utf8(utf8_path);

		sf = tm_source_file_new(locale_path, filetypes_detect(utf8_path)->name);
		if (sf && !document_find_by_filename(utf8_path))
			g_ptr_array_add(source_files, sf);

		g_hash_table_insert(file_table, g_strdup(utf8_path), sf);
		g_free(locale_path);
	}
	g_hash_table_destroy(root->file_table);
	root->file_table = file_table;

	tm_workspace_add_source_files(source_files);
	g_ptr_array_free(source_files, TRUE);
}


void prjorg_project_rescan(void)
{
	GSList *elem;
	gint filenum = 0;

	if (!prj_org)
		return;

	clear_idle_queue(&s_idle_add_funcs);
	clear_idle_queue(&s_idle_remove_funcs);

	foreach_slist(elem, prj_org->roots)
		filenum += prjorg_project_rescan_root(elem->data);

	if (prj_org->generate_tag_prefs == PrjOrgTagYes || (prj_org->generate_tag_prefs == PrjOrgTagAuto && filenum < 300))
		g_slist_foreach(prj_org->roots, (GFunc)regenerate_tags, NULL);
}


static void update_project(
	gchar **source_patterns,
	gchar **header_patterns,
	gchar **ignored_dirs_patterns,
	gchar **ignored_file_patterns,
	PrjOrgTagPrefs generate_tag_prefs)
{
	if (prj_org->source_patterns)
		g_strfreev(prj_org->source_patterns);
	prj_org->source_patterns = g_strdupv(source_patterns);

	if (prj_org->header_patterns)
		g_strfreev(prj_org->header_patterns);
	prj_org->header_patterns = g_strdupv(header_patterns);

	if (prj_org->ignored_dirs_patterns)
		g_strfreev(prj_org->ignored_dirs_patterns);
	prj_org->ignored_dirs_patterns = g_strdupv(ignored_dirs_patterns);

	if (prj_org->ignored_file_patterns)
		g_strfreev(prj_org->ignored_file_patterns);
	prj_org->ignored_file_patterns = g_strdupv(ignored_file_patterns);

	prj_org->generate_tag_prefs = generate_tag_prefs;

	prjorg_project_rescan();
}


void prjorg_project_save(GKeyFile * key_file)
{
	GPtrArray *array;
	GSList *elem, *lst;

	if (!prj_org)
		return;

	g_key_file_set_string_list(key_file, "prjorg", "source_patterns",
		(const gchar**) prj_org->source_patterns, g_strv_length(prj_org->source_patterns));
	g_key_file_set_string_list(key_file, "prjorg", "header_patterns",
		(const gchar**) prj_org->header_patterns, g_strv_length(prj_org->header_patterns));
	g_key_file_set_string_list(key_file, "prjorg", "ignored_dirs_patterns",
		(const gchar**) prj_org->ignored_dirs_patterns, g_strv_length(prj_org->ignored_dirs_patterns));
	g_key_file_set_string_list(key_file, "prjorg", "ignored_file_patterns",
		(const gchar**) prj_org->ignored_file_patterns, g_strv_length(prj_org->ignored_file_patterns));
	g_key_file_set_integer(key_file, "prjorg", "generate_tag_prefs", prj_org->generate_tag_prefs);

	array = g_ptr_array_new();
	lst = prj_org->roots->next;
	foreach_slist (elem, lst)
	{
		PrjOrgRoot *root = elem->data;
		g_ptr_array_add(array, root->base_dir);
	}
	g_key_file_set_string_list(key_file, "prjorg", "external_dirs", (const gchar * const *)array->pdata, array->len);
	g_ptr_array_free(array, TRUE);
}


static PrjOrgRoot *create_root(const gchar *utf8_base_dir)
{
	PrjOrgRoot *root = (PrjOrgRoot *) g_new0(PrjOrgRoot, 1);
	root->base_dir = g_strdup(utf8_base_dir);
	root->file_table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GFreeFunc)tm_source_file_free);
	return root;
}


static void close_root(PrjOrgRoot *root, gpointer user_data)
{
	GPtrArray *source_files;

	source_files = g_ptr_array_new();
	g_hash_table_foreach(root->file_table, (GHFunc)collect_source_files, source_files);
	tm_workspace_remove_source_files(source_files);
	g_ptr_array_free(source_files, TRUE);

	g_hash_table_destroy(root->file_table);
	g_free(root->base_dir);
	g_free(root);
}


static gint root_comparator(PrjOrgRoot *a, PrjOrgRoot *b)
{
	gchar *a_realpath, *b_realpath, *a_locale_base_dir, *b_locale_base_dir;
	gint res;

	a_locale_base_dir = utils_get_locale_from_utf8(a->base_dir);
	b_locale_base_dir = utils_get_locale_from_utf8(b->base_dir);
	a_realpath = tm_get_real_path(a_locale_base_dir);
	b_realpath = tm_get_real_path(b_locale_base_dir);

	res = g_strcmp0(a_realpath, b_realpath);

	g_free(a_realpath);
	g_free(b_realpath);
	g_free(a_locale_base_dir);
	g_free(b_locale_base_dir);

	return res;
}


void prjorg_project_add_external_dir(const gchar *utf8_dirname)
{
	PrjOrgRoot *new_root = create_root(utf8_dirname);
	if (g_slist_find_custom (prj_org->roots, new_root, (GCompareFunc)root_comparator) != NULL)
	{
		close_root(new_root, NULL);
		return;
	}

	GSList *lst = prj_org->roots->next;
	lst = g_slist_prepend(lst, new_root);
	lst = g_slist_sort(lst, (GCompareFunc)root_comparator);
	prj_org->roots->next = lst;

	prjorg_project_rescan();
}


void prjorg_project_remove_external_dir(const gchar *utf8_dirname)
{
	PrjOrgRoot *test_root = create_root(utf8_dirname);
	GSList *found = g_slist_find_custom (prj_org->roots, test_root, (GCompareFunc)root_comparator);
	if (found != NULL)
	{
		PrjOrgRoot *found_root = found->data;

		prj_org->roots = g_slist_remove(prj_org->roots, found_root);
		close_root(found_root, NULL);
		prjorg_project_rescan();
	}
	close_root(test_root, NULL);
}


void prjorg_project_open(GKeyFile * key_file)
{
	gchar **source_patterns, **header_patterns, **ignored_dirs_patterns, **ignored_file_patterns, **external_dirs, **dir_ptr, *last_name;
	gint generate_tag_prefs;
	GSList *elem, *ext_list = NULL;
	gchar *utf8_base_path;

	if (prj_org != NULL)
		prjorg_project_close();

	prj_org = (PrjOrg *) g_new0(PrjOrg, 1);

	prj_org->source_patterns = NULL;
	prj_org->header_patterns = NULL;
	prj_org->ignored_dirs_patterns = NULL;
	prj_org->ignored_file_patterns = NULL;
	prj_org->generate_tag_prefs = PrjOrgTagAuto;

	source_patterns = g_key_file_get_string_list(key_file, "prjorg", "source_patterns", NULL, NULL);
	if (!source_patterns)
		source_patterns = g_strsplit("*.c *.C *.cpp *.cxx *.c++ *.cc *.m", " ", -1);
	header_patterns = g_key_file_get_string_list(key_file, "prjorg", "header_patterns", NULL, NULL);
	if (!header_patterns)
		header_patterns = g_strsplit("*.h *.H *.hpp *.hxx *.h++ *.hh", " ", -1);
	ignored_dirs_patterns = g_key_file_get_string_list(key_file, "prjorg", "ignored_dirs_patterns", NULL, NULL);
	if (!ignored_dirs_patterns)
		ignored_dirs_patterns = g_strsplit(".* CVS", " ", -1);
	ignored_file_patterns = g_key_file_get_string_list(key_file, "prjorg", "ignored_file_patterns", NULL, NULL);
	if (!ignored_file_patterns)
		ignored_file_patterns = g_strsplit("*.o *.obj *.a *.lib *.so *.dll *.lo *.la *.class *.jar *.pyc *.mo *.gmo", " ", -1);
	generate_tag_prefs = utils_get_setting_integer(key_file, "prjorg", "generate_tag_prefs", PrjOrgTagAuto);

	external_dirs = g_key_file_get_string_list(key_file, "prjorg", "external_dirs", NULL, NULL);
	foreach_strv (dir_ptr, external_dirs)
		ext_list = g_slist_prepend(ext_list, *dir_ptr);
	ext_list = g_slist_sort(ext_list, (GCompareFunc)g_strcmp0);
	last_name = NULL;
	foreach_slist (elem, ext_list)
	{
		if (g_strcmp0(last_name, elem->data) != 0)
			prj_org->roots = g_slist_append(prj_org->roots, create_root(elem->data));
		last_name = elem->data;
	}
	g_slist_free(ext_list);

	/* the project directory is always first */
	utf8_base_path = get_project_base_path();
	prj_org->roots = g_slist_prepend(prj_org->roots, create_root(utf8_base_path));
	g_free(utf8_base_path);

	update_project(
		source_patterns,
		header_patterns,
		ignored_dirs_patterns,
		ignored_file_patterns,
		generate_tag_prefs);

	g_strfreev(source_patterns);
	g_strfreev(header_patterns);
	g_strfreev(ignored_dirs_patterns);
	g_strfreev(ignored_file_patterns);
	g_strfreev(external_dirs);
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


void prjorg_project_read_properties_tab(void)
{
	gchar **source_patterns, **header_patterns, **ignored_dirs_patterns, **ignored_file_patterns;

	source_patterns = split_patterns(gtk_entry_get_text(GTK_ENTRY(e->source_patterns)));
	header_patterns = split_patterns(gtk_entry_get_text(GTK_ENTRY(e->header_patterns)));
	ignored_dirs_patterns = split_patterns(gtk_entry_get_text(GTK_ENTRY(e->ignored_dirs_patterns)));
	ignored_file_patterns = split_patterns(gtk_entry_get_text(GTK_ENTRY(e->ignored_file_patterns)));

	update_project(
		source_patterns, header_patterns, ignored_dirs_patterns, ignored_file_patterns,
		gtk_combo_box_get_active(GTK_COMBO_BOX(e->generate_tag_prefs)));

	g_strfreev(source_patterns);
	g_strfreev(header_patterns);
	g_strfreev(ignored_dirs_patterns);
	g_strfreev(ignored_file_patterns);
}


gint prjorg_project_add_properties_tab(GtkWidget *notebook)
{
	GtkWidget *vbox, *hbox, *hbox1;
	GtkWidget *table;
	GtkWidget *label;
	gchar *str;
	gint page_index;

	e = g_new0(PropertyDialogElements, 1);

	vbox = gtk_vbox_new(FALSE, 0);

	table = gtk_table_new(5, 2, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 5);
	gtk_table_set_col_spacings(GTK_TABLE(table), 10);

	label = gtk_label_new(_("Source patterns:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	e->source_patterns = gtk_entry_new();
	ui_table_add_row(GTK_TABLE(table), 0, label, e->source_patterns, NULL);
	ui_entry_add_clear_icon(GTK_ENTRY(e->source_patterns));
	ui_widget_set_tooltip_text(e->source_patterns,
		_("Space separated list of patterns that are used to identify source files. "
		  "Used for header/source swapping."));
	str = g_strjoinv(" ", prj_org->source_patterns);
	gtk_entry_set_text(GTK_ENTRY(e->source_patterns), str);
	g_free(str);

	label = gtk_label_new(_("Header patterns:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	e->header_patterns = gtk_entry_new();
	ui_entry_add_clear_icon(GTK_ENTRY(e->header_patterns));
	ui_table_add_row(GTK_TABLE(table), 1, label, e->header_patterns, NULL);
	ui_widget_set_tooltip_text(e->header_patterns,
		_("Space separated list of patterns that are used to identify headers. "
		  "Used for header/source swapping."));
	str = g_strjoinv(" ", prj_org->header_patterns);
	gtk_entry_set_text(GTK_ENTRY(e->header_patterns), str);
	g_free(str);

	label = gtk_label_new(_("Ignored file patterns:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	e->ignored_file_patterns = gtk_entry_new();
	ui_entry_add_clear_icon(GTK_ENTRY(e->ignored_file_patterns));
	ui_table_add_row(GTK_TABLE(table), 2, label, e->ignored_file_patterns, NULL);
	ui_widget_set_tooltip_text(e->ignored_file_patterns,
		_("Space separated list of patterns that are used to identify files "
		  "that are not displayed in the project tree."));
	str = g_strjoinv(" ", prj_org->ignored_file_patterns);
	gtk_entry_set_text(GTK_ENTRY(e->ignored_file_patterns), str);
	g_free(str);

	label = gtk_label_new(_("Ignored directory patterns:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	e->ignored_dirs_patterns = gtk_entry_new();
	ui_entry_add_clear_icon(GTK_ENTRY(e->ignored_dirs_patterns));
	ui_table_add_row(GTK_TABLE(table), 3, label, e->ignored_dirs_patterns, NULL);
	ui_widget_set_tooltip_text(e->ignored_dirs_patterns,
		_("Space separated list of patterns that are used to identify directories "
		  "that are not scanned for source files."));
	str = g_strjoinv(" ", prj_org->ignored_dirs_patterns);
	gtk_entry_set_text(GTK_ENTRY(e->ignored_dirs_patterns), str);
	g_free(str);

	label = gtk_label_new(_("Generate tags for all project files:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	e->generate_tag_prefs = gtk_combo_box_text_new();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(e->generate_tag_prefs), _("Auto (generate if less than 300 files)"));
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(e->generate_tag_prefs), _("Yes"));
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(e->generate_tag_prefs), _("No"));
	gtk_combo_box_set_active(GTK_COMBO_BOX(e->generate_tag_prefs), prj_org->generate_tag_prefs);
	ui_table_add_row(GTK_TABLE(table), 4, label, e->generate_tag_prefs, NULL);
	ui_widget_set_tooltip_text(e->generate_tag_prefs,
		_("Generate tag list for all project files instead of only for the currently opened files. "
		  "Might be slow for big projects."));

	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 6);

	hbox1 = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Note: the patterns above affect only the sidebar and are not used in the Find in Files\n"
	"dialog. You can further restrict the files belonging to the project by setting the\n"
	"File Patterns under the Project tab (these are also used for the Find in Files dialog)."));
	gtk_box_pack_start(GTK_BOX(hbox1), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox1, FALSE, FALSE, 6);

	label = gtk_label_new("Project Organizer");

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 6);

	page_index = gtk_notebook_append_page(GTK_NOTEBOOK(notebook), hbox, label);
	gtk_widget_show_all(notebook);

	return page_index;
}


void prjorg_project_close(void)
{
	if (!prj_org)
		return;  /* can happen on plugin reload */

	clear_idle_queue(&s_idle_add_funcs);
	clear_idle_queue(&s_idle_remove_funcs);

	g_slist_foreach(prj_org->roots, (GFunc)close_root, NULL);
	g_slist_free(prj_org->roots);

	g_strfreev(prj_org->source_patterns);
	g_strfreev(prj_org->header_patterns);
	g_strfreev(prj_org->ignored_dirs_patterns);
	g_strfreev(prj_org->ignored_file_patterns);

	g_free(prj_org);
	prj_org = NULL;
}


gboolean prjorg_project_is_in_project(const gchar *utf8_filename)
{
	GSList *elem;

	if (!utf8_filename || !prj_org || !geany_data->app->project || !prj_org->roots)
		return FALSE;

	foreach_slist (elem, prj_org->roots)
	{
		PrjOrgRoot *root = elem->data;
		if (g_hash_table_lookup_extended (root->file_table, utf8_filename, NULL, NULL))
			return TRUE;
	}

	return FALSE;
}


static gboolean add_tm_idle(gpointer foo)
{
	GSList *elem2;

	if (!prj_org || !s_idle_add_funcs)
		return FALSE;

	foreach_slist (elem2, s_idle_add_funcs)
	{
		GSList *elem;
		gchar *utf8_fname = elem2->data;

		foreach_slist (elem, prj_org->roots)
		{
			PrjOrgRoot *root = elem->data;
			TMSourceFile *sf = g_hash_table_lookup(root->file_table, utf8_fname);

			if (sf != NULL && !document_find_by_filename(utf8_fname))
			{
				tm_workspace_add_source_file(sf);
				break;  /* single file representation in TM is enough */
			}
		}
	}

	clear_idle_queue(&s_idle_add_funcs);

	return FALSE;
}


/* This function gets called when document is being closed by Geany and we need
 * to add the TMSourceFile from the tag manager because Geany removes it on
 * document close.
 *
 * Additional problem: The tag removal in Geany happens after this function is called.
 * To be sure, perform on idle after this happens (even though from my knowledge of TM
 * this shouldn't probably matter). */
void prjorg_project_add_single_tm_file(gchar *utf8_filename)
{
	if (s_idle_add_funcs == NULL)
		plugin_idle_add(geany_plugin, (GSourceFunc)add_tm_idle, NULL);

	s_idle_add_funcs = g_slist_prepend(s_idle_add_funcs, g_strdup(utf8_filename));
}


static gboolean remove_tm_idle(gpointer foo)
{
	GSList *elem2;

	if (!prj_org || !s_idle_remove_funcs)
		return FALSE;

	foreach_slist (elem2, s_idle_remove_funcs)
	{
		GSList *elem;
		gchar *utf8_fname = elem2->data;

		foreach_slist (elem, prj_org->roots)
		{
			PrjOrgRoot *root = elem->data;
			TMSourceFile *sf = g_hash_table_lookup(root->file_table, utf8_fname);

			if (sf != NULL)
				tm_workspace_remove_source_file(sf);
		}
	}

	clear_idle_queue(&s_idle_remove_funcs);

	return FALSE;
}


/* This function gets called when document is being opened by Geany and we need
 * to remove the TMSourceFile from the tag manager because Geany inserts
 * it for the newly open tab. Even though tag manager would handle two identical
 * files, the file inserted by the plugin isn't updated automatically in TM
 * so any changes wouldn't be reflected in the tags array (e.g. removed function
 * from source file would still be found in TM)
 *
 * Additional problem: The document being opened may be caused
 * by going to tag definition/declaration - tag processing is in progress
 * when this function is called and if we remove the TmSourceFile now, line
 * number for the searched tag won't be found. For this reason delay the tag
 * TmSourceFile removal until idle */
void prjorg_project_remove_single_tm_file(gchar *utf8_filename)
{
	if (s_idle_remove_funcs == NULL)
		plugin_idle_add(geany_plugin, (GSourceFunc)remove_tm_idle, NULL);

	s_idle_remove_funcs = g_slist_prepend(s_idle_remove_funcs, g_strdup(utf8_filename));
}
