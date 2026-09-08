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
#include "prjorg-sidebar.h"
#include "prjorg-wraplabel.h"

extern GeanyPlugin *geany_plugin;
extern GeanyData *geany_data;

typedef struct
{
	GtkWidget *source_patterns;
	GtkWidget *header_patterns;
	GtkWidget *ignored_dirs_patterns;
	GtkWidget *ignored_file_patterns;
	GtkWidget *show_empty_dirs;
	GtkWidget *generate_tag_prefs;
} PropertyDialogElements;

PrjOrg *prj_org = NULL;

static PropertyDialogElements *e;

static GSList *s_idle_add_funcs;
static GSList *s_idle_remove_funcs;

static void close_root(PrjOrgRoot *root, gpointer user_data);


static void clear_idle_queue(GSList **queue)
{
	GSList *elem;

	foreach_slist(elem, *queue)
		g_free(elem->data);
	g_slist_free(*queue);
	*queue = NULL;
}


static void free_tree_node(PrjOrgTreeNode *node)
{
	g_ptr_array_free(node->subtrees, TRUE);
	g_free(node->name);
	g_free(node->full_path);
	g_free(node);
}


static PrjOrgTreeNode *create_tree_node(const gchar *name, const gchar *full_path, gboolean is_file)
{
	PrjOrgTreeNode *node = g_new0(PrjOrgTreeNode, 1);
	node->subtrees = g_ptr_array_new_full(1, (GDestroyNotify)free_tree_node);
	node->name = g_strdup(name);
	node->full_path = g_strdup(full_path);
	node->is_file = is_file;
	return node;
}


static gint sort_tree_nodes(gconstpointer a, gconstpointer b)
{
	PrjOrgTreeNode *node1 = *((PrjOrgTreeNode **)a);
	PrjOrgTreeNode *node2 = *((PrjOrgTreeNode **)b);

	if (!node1->is_file && node2->is_file)
		return -1;

	if (node1->is_file && !node2->is_file)
		return 1;

	return g_strcmp0(node1->name, node2->name);
}


/* fills tree_node and file_table with the contents of locale_path, returns
 * the number of files found */
static guint scan_directory(const gchar *locale_path, GHashTable *visited_paths,
	GHashTable *file_table, PrjOrgTreeNode *tree_node)
{
	gchar *real_path = utils_get_real_path(locale_path);
	const gchar *locale_name;
	GPtrArray *children;
	GDir *dir;
	guint i;
	guint num = 0;

	dir = g_dir_open(locale_path, 0, NULL);
	if (!dir || !real_path || g_hash_table_lookup(visited_paths, real_path))
	{
		g_free(real_path);
		if (dir)
			g_dir_close(dir);
		return 0;
	}

	g_hash_table_insert(visited_paths, real_path, GINT_TO_POINTER(1));

	children = g_ptr_array_new_full(10, g_free);
	while ((locale_name = g_dir_read_name(dir)))
		g_ptr_array_add(children, g_strdup(locale_name));

	g_dir_close(dir);

	foreach_ptr_array(locale_name, i, children)
	{
		gchar *locale_filename, *utf8_filename, *utf8_name;

		utf8_name = utils_get_utf8_from_locale(locale_name);
		locale_filename = g_build_filename(locale_path, locale_name, NULL);
		utf8_filename = utils_get_utf8_from_locale(locale_filename);

		if (g_file_test(locale_filename, G_FILE_TEST_IS_DIR))
		{
			if (!patterns_match(prj_org->ignored_dirs_patterns_prec, utf8_name))
			{
				PrjOrgTreeNode *dir_node = create_tree_node(utf8_name, NULL, FALSE);
				gint filenum;

				filenum = scan_directory(locale_filename, visited_paths, file_table, dir_node);
				if (filenum > 0 || prj_org->show_empty_dirs)
					g_ptr_array_add(tree_node->subtrees, dir_node);
				else
					free_tree_node(dir_node);

				num += filenum;
			}
		}
		else if (g_file_test(locale_filename, G_FILE_TEST_IS_REGULAR))
		{
			if (patterns_match(prj_org->file_patterns_prec, utf8_name) &&
				!patterns_match(prj_org->ignored_file_patterns_prec, utf8_name))
			{
				PrjOrgTreeNode *file_node = create_tree_node(utf8_name, utf8_filename, TRUE);

				g_hash_table_insert(file_table, g_strdup(utf8_filename), NULL);
				g_ptr_array_add(tree_node->subtrees, file_node);

				num++;
			}
		}

		g_free(utf8_filename);
		g_free(locale_filename);
		g_free(utf8_name);
	}

	g_ptr_array_sort(tree_node->subtrees, sort_tree_nodes);

	g_ptr_array_free(children, TRUE);

	return num;
}


static void collect_node_files(PrjOrgTreeNode *node, GPtrArray *files)
{
	PrjOrgTreeNode *child;
	guint i;

	foreach_ptr_array (child, i, node->subtrees)
	{
		if (child->is_file)
			g_ptr_array_add(files, child->full_path);
		else
			collect_node_files(child, files);
	}
}


/* removes all the files below node from the tag manager and the file table
 * and drops all the subtrees of node */
static void clear_node(PrjOrgRoot *root, PrjOrgTreeNode *node)
{
	GPtrArray *files = g_ptr_array_new();
	GPtrArray *source_files = g_ptr_array_new();
	gchar *utf8_path;
	guint i;

	collect_node_files(node, files);

	foreach_ptr_array (utf8_path, i, files)
	{
		TMSourceFile *sf = g_hash_table_lookup(root->file_table, utf8_path);

		if (sf)
			g_ptr_array_add(source_files, sf);
	}
	tm_workspace_remove_source_files(source_files);

	/* the file names are owned by the tree nodes - update the file table
	 * before the nodes get freed */
	foreach_ptr_array (utf8_path, i, files)
		g_hash_table_remove(root->file_table, utf8_path);

	g_ptr_array_set_size(node->subtrees, 0);

	g_ptr_array_free(files, TRUE);
	g_ptr_array_free(source_files, TRUE);
}


/* rescans the directory utf8_path represented by node, returns the number of
 * files found */
static guint rescan_node(PrjOrgRoot *root, PrjOrgTreeNode *node, const gchar *utf8_path)
{
	GHashTable *visited_paths = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	gchar *locale_path = utils_get_locale_from_utf8(utf8_path);
	guint filenum;

	clear_node(root, node);
	filenum = scan_directory(locale_path, visited_paths, root->file_table, node);

	g_hash_table_destroy(visited_paths);
	g_free(locale_path);

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

		if (g_pattern_spec_match_string(pattern, utf8_base_filename))
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
	GStatBuf s;
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


static TMSourceFile *create_source_file(const gchar *utf8_path, gboolean will_open,
	GPtrArray *source_files)
{
	gchar *locale_path = utils_get_locale_from_utf8(utf8_path);
	TMSourceFile *sf = tm_source_file_new(locale_path, filetypes_detect(utf8_path)->name);

	if (sf && !will_open && !document_find_by_filename(utf8_path))
		g_ptr_array_add(source_files, sf);

	g_free(locale_path);

	return sf;
}


/* creates TMSourceFiles for all the files below node and adds them to the tag
 * manager unless they are open in Geany or are going to be opened from the
 * session */
static void generate_node_tags(PrjOrgRoot *root, PrjOrgTreeNode *node, const gchar **session_files)
{
	GPtrArray *files = g_ptr_array_new();
	GPtrArray *source_files = g_ptr_array_new();
	gchar *utf8_path;
	guint i;

	collect_node_files(node, files);

	foreach_ptr_array (utf8_path, i, files)
	{
		gboolean will_open = session_files && g_strv_contains(session_files, utf8_path);
		TMSourceFile *sf = create_source_file(utf8_path, will_open, source_files);

		g_hash_table_insert(root->file_table, g_strdup(utf8_path), sf);
	}
	tm_workspace_add_source_files(source_files);

	g_ptr_array_free(files, TRUE);
	g_ptr_array_free(source_files, TRUE);
}


static gboolean tags_enabled(guint filenum)
{
	return prj_org->generate_tag_prefs == PrjOrgTagYes ||
		(prj_org->generate_tag_prefs == PrjOrgTagAuto && filenum < 1000);
}


static guint get_file_count(void)
{
	GSList *elem;
	guint num = 0;

	foreach_slist (elem, prj_org->roots)
	{
		PrjOrgRoot *root = elem->data;

		num += g_hash_table_size(root->file_table);
	}

	return num;
}


void rescan_project(gchar **session_files)
{
	GSList *elem;
	guint filenum = 0;

	if (!prj_org)
		return;

	clear_idle_queue(&s_idle_add_funcs);
	clear_idle_queue(&s_idle_remove_funcs);

	foreach_slist(elem, prj_org->roots)
	{
		PrjOrgRoot *root = elem->data;

		filenum += rescan_node(root, root->tree, root->base_dir);
	}

	if (tags_enabled(filenum))
	{
		foreach_slist(elem, prj_org->roots)
		{
			PrjOrgRoot *root = elem->data;

			generate_node_tags(root, root->tree, (const gchar **)session_files);
		}
	}
}


void prjorg_project_rescan() {
    rescan_project(NULL);
}


static PrjOrgRoot *create_root(const gchar *utf8_base_dir)
{
	PrjOrgRoot *root = (PrjOrgRoot *) g_new0(PrjOrgRoot, 1);
	root->base_dir = g_strdup(utf8_base_dir);
	root->file_table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GFreeFunc)tm_source_file_free);
	root->tree = create_tree_node(utf8_base_dir, NULL, FALSE);
	return root;
}


static GPtrArray *get_file_patterns(void)
{
	gchar **file_patterns = geany_data->app->project->file_patterns;
	GPtrArray *pattern_list;

	if (!file_patterns || !file_patterns[0])
	{
		gchar **all_pattern = g_strsplit("*", " ", -1);
		pattern_list = get_precompiled_patterns(all_pattern);
		g_strfreev(all_pattern);
	}
	else
		pattern_list = get_precompiled_patterns(file_patterns);

	return pattern_list;
}


static void update_project(
	gchar **source_patterns,
	gchar **header_patterns,
	gchar **ignored_dirs_patterns,
	gchar **ignored_file_patterns,
	gchar **session_files,
	PrjOrgTagPrefs generate_tag_prefs,
	gboolean show_empty_dirs)
{
	gchar *utf8_base_path;

	if (prj_org->source_patterns)
		g_strfreev(prj_org->source_patterns);
	prj_org->source_patterns = g_strdupv(source_patterns);

	if (prj_org->header_patterns)
		g_strfreev(prj_org->header_patterns);
	prj_org->header_patterns = g_strdupv(header_patterns);

	if (prj_org->ignored_dirs_patterns)
		g_strfreev(prj_org->ignored_dirs_patterns);
	prj_org->ignored_dirs_patterns = g_strdupv(ignored_dirs_patterns);
	if (prj_org->ignored_dirs_patterns_prec)
		g_ptr_array_free(prj_org->ignored_dirs_patterns_prec, TRUE);
	prj_org->ignored_dirs_patterns_prec = get_precompiled_patterns(prj_org->ignored_dirs_patterns);

	if (prj_org->ignored_file_patterns)
		g_strfreev(prj_org->ignored_file_patterns);
	prj_org->ignored_file_patterns = g_strdupv(ignored_file_patterns);
	if (prj_org->ignored_file_patterns_prec)
		g_ptr_array_free(prj_org->ignored_file_patterns_prec, TRUE);
	prj_org->ignored_file_patterns_prec = get_precompiled_patterns(prj_org->ignored_file_patterns);

	if (prj_org->file_patterns_prec)
		g_ptr_array_free(prj_org->file_patterns_prec, TRUE);
	prj_org->file_patterns_prec = get_file_patterns();

	prj_org->generate_tag_prefs = generate_tag_prefs;
	prj_org->show_empty_dirs = show_empty_dirs;

	/* re-read the base path in case it was changed in project settings */
	close_root(prj_org->roots->data, NULL);
	prj_org->roots = g_slist_delete_link(prj_org->roots, prj_org->roots);
	utf8_base_path = get_project_base_path();
	prj_org->roots = g_slist_prepend(prj_org->roots, create_root(utf8_base_path));
	g_free(utf8_base_path);

	rescan_project(session_files);
}


static void save_expanded_paths(GKeyFile * key_file)
{
	gchar **expanded_paths = prjorg_sidebar_get_expanded_paths();

	g_key_file_set_string_list(key_file, "prjorg", "expanded_paths",
		(const gchar**) expanded_paths, g_strv_length(expanded_paths));
	g_strfreev(expanded_paths);
}


void prjorg_project_save(GKeyFile * key_file)
{
	GPtrArray *array;
	GSList *elem = NULL, *lst;

	if (!prj_org)
		return;

	save_expanded_paths(key_file);

	g_key_file_set_string_list(key_file, "prjorg", "source_patterns",
		(const gchar**) prj_org->source_patterns, g_strv_length(prj_org->source_patterns));
	g_key_file_set_string_list(key_file, "prjorg", "header_patterns",
		(const gchar**) prj_org->header_patterns, g_strv_length(prj_org->header_patterns));
	g_key_file_set_string_list(key_file, "prjorg", "ignored_dirs_patterns",
		(const gchar**) prj_org->ignored_dirs_patterns, g_strv_length(prj_org->ignored_dirs_patterns));
	g_key_file_set_string_list(key_file, "prjorg", "ignored_file_patterns",
		(const gchar**) prj_org->ignored_file_patterns, g_strv_length(prj_org->ignored_file_patterns));
	g_key_file_set_integer(key_file, "prjorg", "generate_tag_prefs", prj_org->generate_tag_prefs);
	g_key_file_set_boolean(key_file, "prjorg", "show_empty_dirs", prj_org->show_empty_dirs);

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


static void close_root(PrjOrgRoot *root, gpointer user_data)
{
	clear_node(root, root->tree);

	g_hash_table_destroy(root->file_table);
	g_free(root->base_dir);
	free_tree_node(root->tree);
	g_free(root);
}


static gint root_comparator(PrjOrgRoot *a, PrjOrgRoot *b)
{
	gchar *a_realpath, *b_realpath, *a_locale_base_dir, *b_locale_base_dir;
	gint res;

	a_locale_base_dir = utils_get_locale_from_utf8(a->base_dir);
	b_locale_base_dir = utils_get_locale_from_utf8(b->base_dir);
	a_realpath = utils_get_real_path(a_locale_base_dir);
	b_realpath = utils_get_real_path(b_locale_base_dir);

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


gchar **prjorg_project_load_expanded_paths(GKeyFile * key_file)
{
	return g_key_file_get_string_list(key_file, "prjorg", "expanded_paths", NULL, NULL);
}


static gchar **get_session_files(GKeyFile *config)
{
	guint i;
	gboolean have_session_files;
	gchar entry[16];
	gchar **tmp_array;
	GError *error = NULL;
	GPtrArray *files;
	gchar *unescaped_filename;

	files = g_ptr_array_new();
	have_session_files = TRUE;
	i = 0;
	while (have_session_files)
	{
		g_snprintf(entry, sizeof(entry), "FILE_NAME_%d", i);
		tmp_array = g_key_file_get_string_list(config, "files", entry, NULL, &error);
		if (! tmp_array || error)
		{
			g_clear_error(&error);
			have_session_files = FALSE;
		}
		else
		{
			/* the file name is the 8th field of the entry */
			if (g_strv_length(tmp_array) > 7)
			{
				unescaped_filename = g_uri_unescape_string(tmp_array[7], NULL);
				if (unescaped_filename)
					g_ptr_array_add(files, unescaped_filename);
			}
			g_strfreev(tmp_array);
		}
		i++;
	}
	g_ptr_array_add(files, NULL);

	return (gchar **) g_ptr_array_free(files, FALSE);
}


void prjorg_project_open(GKeyFile * key_file)
{
	gchar **source_patterns, **header_patterns, **ignored_dirs_patterns, **ignored_file_patterns, **external_dirs, **dir_ptr, *last_name;
	gint generate_tag_prefs;
	gboolean show_empty_dirs;
	GSList *elem = NULL, *ext_list = NULL;
	gchar *utf8_base_path;
	gchar **session_files;

	if (prj_org != NULL)
		prjorg_project_close();

	prj_org = (PrjOrg *) g_new0(PrjOrg, 1);

	prj_org->source_patterns = NULL;
	prj_org->header_patterns = NULL;
	prj_org->ignored_dirs_patterns = NULL;
	prj_org->ignored_file_patterns = NULL;
	prj_org->generate_tag_prefs = PrjOrgTagAuto;
	prj_org->show_empty_dirs = TRUE;

	source_patterns = g_key_file_get_string_list(key_file, "prjorg", "source_patterns", NULL, NULL);
	if (!source_patterns)
		source_patterns = g_strsplit(PRJORG_PATTERNS_SOURCE, " ", -1);
	header_patterns = g_key_file_get_string_list(key_file, "prjorg", "header_patterns", NULL, NULL);
	if (!header_patterns)
		header_patterns = g_strsplit(PRJORG_PATTERNS_HEADER, " ", -1);
	ignored_dirs_patterns = g_key_file_get_string_list(key_file, "prjorg", "ignored_dirs_patterns", NULL, NULL);
	if (!ignored_dirs_patterns)
		ignored_dirs_patterns = g_strsplit(PRJORG_PATTERNS_IGNORED_DIRS, " ", -1);
	ignored_file_patterns = g_key_file_get_string_list(key_file, "prjorg", "ignored_file_patterns", NULL, NULL);
	if (!ignored_file_patterns)
		ignored_file_patterns = g_strsplit(PRJORG_PATTERNS_IGNORED_FILE, " ", -1);
	generate_tag_prefs = utils_get_setting_integer(key_file, "prjorg", "generate_tag_prefs", PrjOrgTagAuto);
	show_empty_dirs = utils_get_setting_boolean(key_file, "prjorg", "show_empty_dirs", TRUE);

	/* we need to know which files will be opened and parsed by Geany, to avoid parsing them twice */
	session_files = get_session_files(key_file);
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
		session_files,
		generate_tag_prefs,
		show_empty_dirs);

	g_strfreev(source_patterns);
	g_strfreev(header_patterns);
	g_strfreev(ignored_dirs_patterns);
	g_strfreev(ignored_file_patterns);
	g_strfreev(external_dirs);
	g_strfreev(session_files);
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
		source_patterns, header_patterns, ignored_dirs_patterns, ignored_file_patterns, NULL,
		gtk_combo_box_get_active(GTK_COMBO_BOX(e->generate_tag_prefs)),
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(e->show_empty_dirs)));

	g_strfreev(source_patterns);
	g_strfreev(header_patterns);
	g_strfreev(ignored_dirs_patterns);
	g_strfreev(ignored_file_patterns);
}


GtkWidget *prjorg_project_add_properties_tab(GtkWidget *notebook)
{
	GtkWidget *vbox, *hbox, *hbox1, *ebox, *table_box;
	GtkWidget *label;
	gchar *str;
	GtkSizeGroup *size_group;

	e = g_new0(PropertyDialogElements, 1);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

	table_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
	gtk_box_set_spacing(GTK_BOX(table_box), 6);

	size_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	label = gtk_label_new(_("Source patterns:"));
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_size_group_add_widget(size_group, label);
	e->source_patterns = gtk_entry_new();
	ui_entry_add_clear_icon(GTK_ENTRY(e->source_patterns));
	gtk_widget_set_tooltip_text(e->source_patterns,
		_("Space separated list of patterns that are used to identify source files. "
		  "Used for header/source swapping."));
	str = g_strjoinv(" ", prj_org->source_patterns);
	gtk_entry_set_text(GTK_ENTRY(e->source_patterns), str);
	g_free(str);
	ebox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start(GTK_BOX(ebox), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(ebox), e->source_patterns, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(table_box), ebox, TRUE, FALSE, 0);

	label = gtk_label_new(_("Header patterns:"));
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_size_group_add_widget(size_group, label);
	e->header_patterns = gtk_entry_new();
	ui_entry_add_clear_icon(GTK_ENTRY(e->header_patterns));
	gtk_widget_set_tooltip_text(e->header_patterns,
		_("Space separated list of patterns that are used to identify headers. "
		  "Used for header/source swapping."));
	str = g_strjoinv(" ", prj_org->header_patterns);
	gtk_entry_set_text(GTK_ENTRY(e->header_patterns), str);
	g_free(str);
	ebox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start(GTK_BOX(ebox), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(ebox), e->header_patterns, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(table_box), ebox, TRUE, FALSE, 0);

	label = gtk_label_new(_("Ignored file patterns:"));
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_size_group_add_widget(size_group, label);
	e->ignored_file_patterns = gtk_entry_new();
	ui_entry_add_clear_icon(GTK_ENTRY(e->ignored_file_patterns));
	gtk_widget_set_tooltip_text(e->ignored_file_patterns,
		_("Space separated list of patterns that are used to identify files "
		  "that are not displayed in the project tree."));
	str = g_strjoinv(" ", prj_org->ignored_file_patterns);
	gtk_entry_set_text(GTK_ENTRY(e->ignored_file_patterns), str);
	g_free(str);
	ebox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start(GTK_BOX(ebox), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(ebox), e->ignored_file_patterns, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(table_box), ebox, TRUE, FALSE, 0);

	label = gtk_label_new(_("Ignored directory patterns:"));
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_size_group_add_widget(size_group, label);
	e->ignored_dirs_patterns = gtk_entry_new();
	ui_entry_add_clear_icon(GTK_ENTRY(e->ignored_dirs_patterns));
	gtk_widget_set_tooltip_text(e->ignored_dirs_patterns,
		_("Space separated list of patterns that are used to identify directories "
		  "that are not scanned for source files."));
	str = g_strjoinv(" ", prj_org->ignored_dirs_patterns);
	gtk_entry_set_text(GTK_ENTRY(e->ignored_dirs_patterns), str);
	g_free(str);
	ebox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start(GTK_BOX(ebox), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(ebox), e->ignored_dirs_patterns, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(table_box), ebox, TRUE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(vbox), table_box, FALSE, FALSE, 6);

	hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	label = prjorg_wrap_label_new(_("The patterns above affect only sidebar and indexing and are not used in the Find in Files "
	"dialog. You can further restrict the files belonging to the project by setting the"
	"File Patterns under the Project tab (these are also used for the Find in Files dialog)."));
	gtk_box_pack_start(GTK_BOX(hbox1), label, TRUE, TRUE, 12);
	gtk_box_pack_start(GTK_BOX(vbox), hbox1, FALSE, FALSE, 0);

	hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	label = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(label), _("<b>Various</b>"));
	gtk_box_pack_start(GTK_BOX(hbox1), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox1, FALSE, FALSE, 12);

	hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	e->show_empty_dirs = gtk_check_button_new_with_label(_("Show empty directories in sidebar"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(e->show_empty_dirs), prj_org->show_empty_dirs);
	gtk_widget_set_tooltip_text(e->show_empty_dirs,
		_("Whether to show empty directories in the sidebar or not. Showing empty "
		  "directories is useful when using file operations from the context menu, "
		  "e.g. to create a new file in the directory."));
	gtk_box_pack_start(GTK_BOX(hbox1), e->show_empty_dirs, FALSE, FALSE, 12);
	gtk_box_pack_start(GTK_BOX(vbox), hbox1, FALSE, FALSE, 0);

	table_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
	gtk_box_set_spacing(GTK_BOX(table_box), 6);

	size_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	label = gtk_label_new(_("Index all project files:"));
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_size_group_add_widget(size_group, label);
	e->generate_tag_prefs = gtk_combo_box_text_new();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(e->generate_tag_prefs), _("Auto (index if less than 1000 files)"));
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(e->generate_tag_prefs), _("Yes"));
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(e->generate_tag_prefs), _("No"));
	gtk_combo_box_set_active(GTK_COMBO_BOX(e->generate_tag_prefs), prj_org->generate_tag_prefs);
	gtk_widget_set_tooltip_text(e->generate_tag_prefs,
		_("Generate symbol list for all project files instead of only for the currently opened files. "
		  "Might be slow for big projects."));
	ebox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start(GTK_BOX(ebox), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(ebox), e->generate_tag_prefs, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(table_box), ebox, TRUE, FALSE, 0);

	hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(hbox1), table_box, FALSE, FALSE, 12);
	gtk_box_pack_start(GTK_BOX(vbox), hbox1, FALSE, FALSE, 6);

	label = gtk_label_new("Project Organizer");

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 6);

	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), hbox, label);
	gtk_widget_show_all(notebook);

	return hbox;
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
	g_ptr_array_free(prj_org->ignored_dirs_patterns_prec, TRUE);
	g_strfreev(prj_org->ignored_file_patterns);
	g_ptr_array_free(prj_org->ignored_file_patterns_prec, TRUE);
	g_ptr_array_free(prj_org->file_patterns_prec, TRUE);

	g_free(prj_org);
	prj_org = NULL;
}


gboolean prjorg_project_is_in_project(const gchar *utf8_filename)
{
	GSList *elem = NULL;

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


static PrjOrgRoot *find_root(const gchar *utf8_path)
{
	GSList *elem = NULL;

	foreach_slist (elem, prj_org->roots)
	{
		PrjOrgRoot *root = elem->data;
		gchar *utf8_rel_path;

		if (g_strcmp0(root->base_dir, utf8_path) == 0)
			return root;

		utf8_rel_path = get_relative_path(root->base_dir, utf8_path);
		if (utf8_rel_path)
		{
			g_free(utf8_rel_path);
			return root;
		}
	}

	return NULL;
}


/* returns the directory node corresponding to utf8_path or NULL if there is
 * no such directory in the tree */
static PrjOrgTreeNode *find_dir_node(PrjOrgRoot *root, const gchar *utf8_path)
{
	PrjOrgTreeNode *node = root->tree;
	gchar *utf8_rel_path;
	gchar **pathv, **name;

	if (g_strcmp0(root->base_dir, utf8_path) == 0)
		return node;

	utf8_rel_path = get_relative_path(root->base_dir, utf8_path);
	if (!utf8_rel_path)
		return NULL;

	pathv = g_strsplit_set(utf8_rel_path, "/\\", 0);
	foreach_strv (name, pathv)
	{
		PrjOrgTreeNode *child;
		PrjOrgTreeNode *found = NULL;
		guint i;

		foreach_ptr_array (child, i, node->subtrees)
		{
			if (!child->is_file && g_strcmp0(child->name, *name) == 0)
			{
				found = child;
				break;
			}
		}

		node = found;
		if (!node)
			break;
	}

	g_strfreev(pathv);
	g_free(utf8_rel_path);

	return node;
}


/* Rescans the directory utf8_path and updates the file tree, the file table
 * and the tag manager accordingly. Returns the tree node of the directory or
 * NULL if the directory isn't part of the project. */
PrjOrgTreeNode *prjorg_project_reload_directory(const gchar *utf8_path)
{
	PrjOrgRoot *root;
	PrjOrgTreeNode *node;

	if (!prj_org)
		return NULL;

	root = find_root(utf8_path);
	if (!root)
		return NULL;

	node = find_dir_node(root, utf8_path);
	if (!node)
		return NULL;

	/* see rescan_project() */
	clear_idle_queue(&s_idle_add_funcs);
	clear_idle_queue(&s_idle_remove_funcs);

	rescan_node(root, node, utf8_path);

	if (tags_enabled(get_file_count()))
		generate_node_tags(root, node, NULL);

	return node;
}


static gboolean add_tm_idle(gpointer foo)
{
	GSList *elem2 = NULL;

	if (!prj_org || !s_idle_add_funcs)
		return FALSE;

	foreach_slist (elem2, s_idle_add_funcs)
	{
		GSList *elem = NULL;
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
	GSList *elem2 = NULL;

	if (!prj_org || !s_idle_remove_funcs)
		return FALSE;

	foreach_slist (elem2, s_idle_remove_funcs)
	{
		GSList *elem = NULL;
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
