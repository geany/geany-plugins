/*
 *      treebrowser.c - v0.20
 *
 *      Copyright 2010 Adrian Dimitrov <dimitrov.adrian@gmail.com>
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <glib.h>
#include <glib/gstdio.h>

#include "geany.h"
#include "geanyplugin.h"

#ifdef HAVE_GIO
# include <gio/gio.h>
#endif

#ifdef G_OS_WIN32
# include <windows.h>
#endif

/* These items are set by Geany before plugin_init() is called. */
GeanyPlugin 				*geany_plugin;
GeanyData 					*geany_data;
GeanyFunctions 				*geany_functions;

static gint 				page_number 				= 0;
static GtkTreeStore 		*treestore;
static GtkWidget 			*treeview;
static GtkWidget 			*sidebar_vbox;
static GtkWidget 			*sidebar_vbox_bars;
static GtkWidget 			*filter;
static GtkWidget 			*addressbar;
static gchar 				*addressbar_last_address 	= NULL;

static GtkTreeIter 			bookmarks_iter;
static gboolean 			bookmarks_expanded = FALSE;

static GtkTreeViewColumn 	*treeview_column_text;
static GtkCellRenderer 		*render_icon, *render_text;

/* ------------------
 * FLAGS
 * ------------------ */

static gboolean 			flag_on_expand_refresh 		= FALSE;

/* ------------------
 *  CONFIG VARS
 * ------------------ */

static gchar 				*CONFIG_FILE 				= NULL;
#ifdef G_OS_WIN32
static gchar 				*CONFIG_OPEN_EXTERNAL_CMD 	= "nautilus '%d'";
#else
static gchar 				*CONFIG_OPEN_EXTERNAL_CMD 	= "explorer '%d'";
#endif
static gboolean 			CONFIG_REVERSE_FILTER 		= FALSE;
static gboolean 			CONFIG_ONE_CLICK_CHDOC 		= FALSE;
static gboolean 			CONFIG_SHOW_HIDDEN_FILES 	= FALSE;
static gboolean 			CONFIG_HIDE_OBJECT_FILES 	= FALSE;
static gint 				CONFIG_SHOW_BARS			= 1;
static gboolean 			CONFIG_CHROOT_ON_DCLICK		= FALSE;
static gboolean 			CONFIG_FOLLOW_CURRENT_DOC 	= TRUE;
static gboolean 			CONFIG_ON_DELETE_CLOSE_FILE = TRUE;
static gboolean 			CONFIG_SHOW_TREE_LINES 		= TRUE;
static gboolean 			CONFIG_SHOW_BOOKMARKS 		= FALSE;
static gint 				CONFIG_SHOW_ICONS 			= 2;

/* ------------------
 * TREEVIEW STRUCT
 * ------------------ */

enum
{
	TREEBROWSER_COLUMNC 								= 4,

	TREEBROWSER_COLUMN_ICON 							= 0,
	TREEBROWSER_COLUMN_NAME 							= 1,
	TREEBROWSER_COLUMN_URI 								= 2,
	TREEBROWSER_COLUMN_FLAG 							= 3,

	TREEBROWSER_RENDER_ICON 							= 0,
	TREEBROWSER_RENDER_TEXT 							= 1,

	TREEBROWSER_FLAGS_SEPARATOR 						= -1
};


/* Keybinding(s) */
enum
{
	KB_FOCUS_FILE_LIST,
	KB_FOCUS_PATH_ENTRY,
	KB_RENAME_OBJECT,
	KB_REFRESH,
	KB_COUNT
};


/* ------------------
 * PLUGIN INFO
 * ------------------ */

PLUGIN_VERSION_CHECK(188)

PLUGIN_SET_TRANSLATABLE_INFO(
	LOCALEDIR,
	GETTEXT_PACKAGE,
	_("TreeBrowser"),
	_("This plugin adds a tree browser to Geany, allowing the user to browse files using a tree view of the directory being browsed."),
	"0.20" ,
	"Adrian Dimitrov (dimitrov.adrian@gmail.com)")


/* ------------------
 * PREDEFINES
 * ------------------ */

#define foreach_slist_free(node, list) for (node = list, list = NULL; g_slist_free_1(list), node != NULL; list = node, node = node->next)

static GList*
_gtk_cell_layout_get_cells(GtkTreeViewColumn *column)
{
#if GTK_CHECK_VERSION(2, 12, 0)
	return gtk_cell_layout_get_cells(GTK_CELL_LAYOUT(column));
#else
	return gtk_tree_view_column_get_cell_renderers(column);
#endif
}


/* ------------------
 * PROTOTYPES
 * ------------------ */

static void 	project_change_cb(G_GNUC_UNUSED GObject *obj, G_GNUC_UNUSED GKeyFile *config, G_GNUC_UNUSED gpointer data);
static void 	treebrowser_browse(gchar *directory, gpointer parent);
static void 	treebrowser_bookmarks_set_state();
static void 	treebrowser_load_bookmarks();
static void 	gtk_tree_store_iter_clear_nodes(gpointer iter, gboolean delete_root);
static void 	treebrowser_rename_current();
static void 	load_settings();
static gboolean save_settings();


/* ------------------
 * PLUGIN CALLBACKS
 * ------------------ */

PluginCallback plugin_callbacks[] =
{
	{ "project-open", (GCallback) &project_change_cb, TRUE, NULL },
	{ "project-save", (GCallback) &project_change_cb, TRUE, NULL },
	{ NULL, NULL, FALSE, NULL }
};


/* ------------------
 * TREEBROWSER CORE FUNCTIONS
 * ------------------ */


static gboolean
tree_view_row_expanded_iter(GtkTreeView *tree_view, GtkTreeIter *iter)
{
	GtkTreePath *tree_path;
	gboolean expanded;

	tree_path = gtk_tree_model_get_path(gtk_tree_view_get_model(tree_view), iter);
	expanded = gtk_tree_view_row_expanded(tree_view, tree_path);
	gtk_tree_path_free(tree_path);

	return expanded;
}

static GdkPixbuf *
utils_pixbuf_from_stock(const gchar *stock_id)
{
	GtkIconSet *icon_set;

	icon_set = gtk_icon_factory_lookup_default(stock_id);

	if (icon_set)
		return gtk_icon_set_render_icon(icon_set, gtk_widget_get_default_style(),
										gtk_widget_get_default_direction(),
										GTK_STATE_NORMAL, GTK_ICON_SIZE_MENU, NULL, NULL);
	return NULL;
}

static GdkPixbuf *
utils_pixbuf_from_path(gchar *path)
{
#if defined(HAVE_GIO) && GTK_CHECK_VERSION(2, 14, 0)
	GIcon 		*icon;
	GdkPixbuf 	*ret = NULL;
	GtkIconInfo *info;
	gchar 		*ctype;
	gint 		width;

	ctype = g_content_type_guess(path, NULL, 0, NULL);
	icon = g_content_type_get_icon(ctype);
	g_free(ctype);

	if (icon != NULL)
	{
		gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &width, NULL);
		info = gtk_icon_theme_lookup_by_gicon(gtk_icon_theme_get_default(), icon, width, GTK_ICON_LOOKUP_USE_BUILTIN);
		g_object_unref(icon);
		if (!info)
			return NULL;
		ret = gtk_icon_info_load_icon (info, NULL);
		gtk_icon_info_free(info);
	}
	return ret;
#else
	return utils_pixbuf_from_stock(g_file_test(path, G_FILE_TEST_IS_DIR)
									? GTK_STOCK_DIRECTORY
									: GTK_STOCK_FILE);
#endif
}

/* Return: FALSE - if file is filtered and not shown, and TRUE - if file isn`t filtered, and have to be shown */
static gboolean
check_filtered(const gchar *base_name)
{
	gchar		**filters;
	guint 		i;
	gboolean 	temporary_reverse 	= FALSE;
	const gchar *exts[] 			= {".o", ".obj", ".so", ".dll", ".a", ".lib", ".la", ".lo", ".pyc"};
	guint exts_len;
	const gchar *ext;
	gboolean	filtered;

	if (CONFIG_HIDE_OBJECT_FILES)
	{
		exts_len = G_N_ELEMENTS(exts);
		for (i = 0; i < exts_len; i++)
		{
			ext = exts[i];
			if (g_str_has_suffix(base_name, ext))
				return FALSE;
		}
	}

	if (! NZV(gtk_entry_get_text(GTK_ENTRY(filter))))
		return TRUE;

	filters = g_strsplit(gtk_entry_get_text(GTK_ENTRY(filter)), ";", 0);

	if (utils_str_equal(filters[0], "!") == TRUE)
	{
		temporary_reverse = TRUE;
		i = 1;
	}
	else
		i = 0;

	filtered = CONFIG_REVERSE_FILTER || temporary_reverse ? TRUE : FALSE;
	for (; filters[i]; i++)
	{
		if (utils_str_equal(base_name, "*") || g_pattern_match_simple(filters[i], base_name))
		{
			filtered = CONFIG_REVERSE_FILTER || temporary_reverse ? FALSE : TRUE;
			break;
		}
	}
	g_strfreev(filters);

	return filtered;
}

#ifdef G_OS_WIN32
static gboolean
win32_check_hidden(const gchar *filename)
{
	DWORD attrs;
	static wchar_t w_filename[MAX_PATH];
	MultiByteToWideChar(CP_UTF8, 0, filename, -1, w_filename, sizeof(w_filename));
	attrs = GetFileAttributesW(w_filename);
	if (attrs != INVALID_FILE_ATTRIBUTES && attrs & FILE_ATTRIBUTE_HIDDEN)
		return TRUE;
	return FALSE;
}
#endif

/* Returns: whether name should be hidden. */
static gboolean
check_hidden(const gchar *filename)
{
	const gchar *base_name = NULL;
	base_name = g_path_get_basename(filename);
	gsize len;

	if (! NZV(base_name))
		return FALSE;

#ifdef G_OS_WIN32
	if (win32_check_hidden(filename))
		return TRUE;
#else
	if (base_name[0] == '.')
		return TRUE;
#endif

	len = strlen(base_name);
	if (base_name[len - 1] == '~')
		return TRUE;

	return FALSE;
}

static gchar*
get_default_dir()
{
	gchar 			*dir;
	GeanyProject 	*project 	= geany->app->project;
	GeanyDocument	*doc 		= document_get_current();

	if (doc != NULL && doc->file_name != NULL && g_path_is_absolute(doc->file_name))
	{
		gchar *dir_name;
		gchar *ret;

		dir_name = g_path_get_dirname(doc->file_name);
		ret = utils_get_locale_from_utf8(dir_name);
		g_free (dir_name);

		return ret;
	}

	if (project)
		dir = project->base_path;
	else
		dir = geany->prefs->default_open_path;

	if (NZV(dir))
		return utils_get_locale_from_utf8(dir);

	return g_get_current_dir();
}

static gchar *
get_terminal()
{
	gchar 		*terminal;
	const gchar *term = g_getenv("TERM");

	if (term != NULL)
		terminal = g_strdup(term);
	else
		terminal = g_strdup("xterm");
	return terminal;
}

static gboolean
treebrowser_checkdir(gchar *directory)
{
	gboolean is_dir;
	static const GdkColor red 	= {0, 0xffff, 0x6666, 0x6666};
	static const GdkColor white = {0, 0xffff, 0xffff, 0xffff};
	static gboolean old_value = TRUE;

	is_dir = g_file_test(directory, G_FILE_TEST_IS_DIR);

	if (old_value != is_dir)
	{
		gtk_widget_modify_base(GTK_WIDGET(addressbar), GTK_STATE_NORMAL, is_dir ? NULL : &red);
		gtk_widget_modify_text(GTK_WIDGET(addressbar), GTK_STATE_NORMAL, is_dir ? NULL : &white);
		old_value = is_dir;
	}

	if (!is_dir)
	{
		if (CONFIG_SHOW_BARS == 0)
			dialogs_show_msgbox(GTK_MESSAGE_ERROR, _("%s: no such directory."), directory);

		return FALSE;
	}

	return is_dir;
}

static void
treebrowser_chroot(gchar *directory)
{
	if (g_str_has_suffix(directory, "/"))
		g_strlcpy(directory, directory, strlen(directory));

	gtk_entry_set_text(GTK_ENTRY(addressbar), directory);

	if (!directory || strlen(directory) == 0)
		directory = "/";

	if (! treebrowser_checkdir(directory))
		return;

	treebrowser_bookmarks_set_state();

	gtk_tree_store_clear(treestore);
	setptr(addressbar_last_address, g_strdup(directory));

	treebrowser_browse(addressbar_last_address, NULL);
	treebrowser_load_bookmarks();
}

static void
treebrowser_browse(gchar *directory, gpointer parent)
{
	GtkTreeIter 	iter, iter_empty, *last_dir_iter = NULL;
	gboolean 		is_dir;
	gboolean 		expanded = FALSE, has_parent;
	gchar 			*utf8_name;
	GSList 			*list, *node;

	gchar 			*fname;
	gchar 			*uri;

	directory 		= g_strconcat(directory, G_DIR_SEPARATOR_S, NULL);

	has_parent = parent ? gtk_tree_store_iter_is_valid(treestore, parent) : FALSE;
	if (has_parent)
	{
		if (parent == &bookmarks_iter)
			treebrowser_load_bookmarks();
	}
	else
		parent = NULL;

	if (has_parent && tree_view_row_expanded_iter(GTK_TREE_VIEW(treeview), parent))
	{
		expanded = TRUE;
		treebrowser_bookmarks_set_state();
	}

	gtk_tree_store_iter_clear_nodes(parent, FALSE);

	list = utils_get_file_list(directory, NULL, NULL);
	if (list != NULL)
	{
		foreach_slist_free(node, list)
		{
			fname 		= node->data;
			uri 		= g_strconcat(directory, fname, NULL);
			is_dir 		= g_file_test (uri, G_FILE_TEST_IS_DIR);
			utf8_name 	= utils_get_utf8_from_locale(fname);

			if (!check_hidden(uri))
			{
				GdkPixbuf *icon = NULL;

				if (is_dir)
				{
					if (last_dir_iter == NULL)
						gtk_tree_store_prepend(treestore, &iter, parent);
					else
					{
						gtk_tree_store_insert_after(treestore, &iter, parent, last_dir_iter);
						gtk_tree_iter_free(last_dir_iter);
					}
					last_dir_iter = gtk_tree_iter_copy(&iter);
					icon = CONFIG_SHOW_ICONS ? utils_pixbuf_from_stock(GTK_STOCK_DIRECTORY) : NULL;
					gtk_tree_store_set(treestore, &iter,
										TREEBROWSER_COLUMN_ICON, 	icon,
										TREEBROWSER_COLUMN_NAME, 	fname,
										TREEBROWSER_COLUMN_URI, 	uri,
										-1);
					gtk_tree_store_prepend(treestore, &iter_empty, &iter);
					gtk_tree_store_set(treestore, &iter_empty,
									TREEBROWSER_COLUMN_ICON, 	NULL,
									TREEBROWSER_COLUMN_NAME, 	_("(Empty)"),
									TREEBROWSER_COLUMN_URI, 	NULL,
									-1);
				}
				else
				{
					if (check_filtered(utf8_name))
					{
						icon = CONFIG_SHOW_ICONS == 2
									? utils_pixbuf_from_path(uri)
									: CONFIG_SHOW_ICONS
										? utils_pixbuf_from_stock(GTK_STOCK_FILE)
										: NULL;
						gtk_tree_store_append(treestore, &iter, parent);
						gtk_tree_store_set(treestore, &iter,
										TREEBROWSER_COLUMN_ICON, 	icon,
										TREEBROWSER_COLUMN_NAME, 	fname,
										TREEBROWSER_COLUMN_URI, 	uri,
										-1);
					}
				}

				if (icon)
					g_object_unref(icon);
			}
			g_free(utf8_name);
			g_free(uri);
			g_free(fname);
		}
	}
	else
	{
		gtk_tree_store_prepend(treestore, &iter_empty, parent);
		gtk_tree_store_set(treestore, &iter_empty,
						TREEBROWSER_COLUMN_ICON, 	NULL,
						TREEBROWSER_COLUMN_NAME, 	_("(Empty)"),
						TREEBROWSER_COLUMN_URI, 	NULL,
						-1);
	}

	if (has_parent)
	{
		if (expanded)
			gtk_tree_view_expand_row(GTK_TREE_VIEW(treeview), gtk_tree_model_get_path(GTK_TREE_MODEL(treestore), parent), FALSE);
	}
	else
		treebrowser_load_bookmarks();

	g_free(directory);

}

static void
treebrowser_bookmarks_set_state()
{
	if (gtk_tree_store_iter_is_valid(treestore, &bookmarks_iter))
		bookmarks_expanded = tree_view_row_expanded_iter(GTK_TREE_VIEW(treeview), &bookmarks_iter);
	else
		bookmarks_expanded = FALSE;
}

static void
treebrowser_load_bookmarks()
{
	gchar 		*bookmarks;
	gchar 		*contents, *path_full;
	gchar 		**lines, **line;
	GtkTreeIter iter;
	gchar 		*pos;
	gchar 		*name;
	GdkPixbuf 	*icon = NULL;

	if (! CONFIG_SHOW_BOOKMARKS)
		return;

	bookmarks = g_build_filename(g_get_home_dir(), ".gtk-bookmarks", NULL);
	if (g_file_get_contents(bookmarks, &contents, NULL, NULL))
	{
		if (gtk_tree_store_iter_is_valid(treestore, &bookmarks_iter))
		{
			bookmarks_expanded = tree_view_row_expanded_iter(GTK_TREE_VIEW(treeview), &bookmarks_iter);
			gtk_tree_store_iter_clear_nodes(&bookmarks_iter, FALSE);
		}
		else
		{
			gtk_tree_store_prepend(treestore, &bookmarks_iter, NULL);
			icon = CONFIG_SHOW_ICONS ? utils_pixbuf_from_stock(GTK_STOCK_HOME) : NULL;
			gtk_tree_store_set(treestore, &bookmarks_iter,
											TREEBROWSER_COLUMN_ICON, 	icon,
											TREEBROWSER_COLUMN_NAME, 	_("Bookmarks"),
											TREEBROWSER_COLUMN_URI, 	NULL,
											-1);
			if (icon)
				g_object_unref(icon);

			gtk_tree_store_insert_after(treestore, &iter, NULL, &bookmarks_iter);
			gtk_tree_store_set(treestore, &iter,
											TREEBROWSER_COLUMN_ICON, 	NULL,
											TREEBROWSER_COLUMN_NAME, 	NULL,
											TREEBROWSER_COLUMN_URI, 	NULL,
											TREEBROWSER_COLUMN_FLAG, 	TREEBROWSER_FLAGS_SEPARATOR,
											-1);
		}
		lines = g_strsplit (contents, "\n", 0);
		for (line = lines; *line; ++line)
		{
			if (**line)
			{
				pos = g_utf8_strchr (*line, -1, ' ');
				if (pos != NULL)
				{
					*pos = '\0';
					name = pos + 1;
				}
				else
					name = NULL;
			}
			path_full = g_filename_from_uri(*line, NULL, NULL);
			if (path_full != NULL)
			{
				if (g_file_test(path_full, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))
				{
					gtk_tree_store_append(treestore, &iter, &bookmarks_iter);
					icon = CONFIG_SHOW_ICONS ? utils_pixbuf_from_stock(GTK_STOCK_DIRECTORY) : NULL;
					gtk_tree_store_set(treestore, &iter,
												TREEBROWSER_COLUMN_ICON, 	icon,
												TREEBROWSER_COLUMN_NAME, 	g_basename(path_full),
												TREEBROWSER_COLUMN_URI, 	path_full,
												-1);
					if (icon)
						g_object_unref(icon);
					gtk_tree_store_append(treestore, &iter, &iter);
					gtk_tree_store_set(treestore, &iter,
											TREEBROWSER_COLUMN_ICON, 	NULL,
											TREEBROWSER_COLUMN_NAME, 	_("(Empty)"),
											TREEBROWSER_COLUMN_URI, 	NULL,
												-1);
				}
				g_free(path_full);
			}
		}
		g_strfreev(lines);
		g_free(contents);
		if (bookmarks_expanded)
		{
			GtkTreePath *tree_path;

			tree_path = gtk_tree_model_get_path(GTK_TREE_MODEL(treestore), &bookmarks_iter);
			gtk_tree_view_expand_row(GTK_TREE_VIEW(treeview), tree_path, FALSE);
			gtk_tree_path_free(tree_path);
		}
	}
}

static gboolean
treebrowser_search(gchar *uri, gpointer parent)
{
	GtkTreeIter 	iter;
	GtkTreePath 	*path;
	gchar 			*uri_current;

	gtk_tree_model_iter_children(GTK_TREE_MODEL(treestore), &iter, parent);

	do
	{
		if (gtk_tree_model_iter_has_child(GTK_TREE_MODEL(treestore), &iter))
			if (treebrowser_search(uri, &iter))
				return TRUE;

		gtk_tree_model_get(GTK_TREE_MODEL(treestore), &iter, TREEBROWSER_COLUMN_URI, &uri_current, -1);

		if (utils_str_equal(uri, uri_current) == TRUE)
		{
			path = gtk_tree_model_get_path(GTK_TREE_MODEL(treestore), &iter);
			gtk_tree_view_expand_to_path(GTK_TREE_VIEW(treeview), path);
			gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(treeview), path, TREEBROWSER_COLUMN_ICON, FALSE, 0, 0);
			gtk_tree_view_set_cursor(GTK_TREE_VIEW(treeview), path, treeview_column_text, FALSE);
			gtk_tree_path_free(path);
			g_free(uri_current);
			return TRUE;
		}
		else
			g_free(uri_current);

	} while(gtk_tree_model_iter_next(GTK_TREE_MODEL(treestore), &iter));

	return FALSE;
}

static void
fs_remove(gchar *root, gboolean delete_root)
{
	GDir *dir;
	gchar *path;
	const gchar *name;

	if (! g_file_test(root, G_FILE_TEST_EXISTS))
		return;

	if (g_file_test(root, G_FILE_TEST_IS_DIR))
	{
		dir = g_dir_open (root, 0, NULL);

		if (!dir)
		{
			if (delete_root)
			{
				g_remove(root);
			}
			else return;
		}

		name = g_dir_read_name (dir);
		while (name != NULL)
		{
			path = g_build_filename(root, name, NULL);
			if (g_file_test(path, G_FILE_TEST_IS_DIR))
				fs_remove(path, delete_root);
			g_remove(path);
			g_free(path);
			name = g_dir_read_name(dir);
		}
	}
	else
		delete_root = TRUE;

	if (delete_root)
		g_remove(root);

	return;
}

static void
showbars(gboolean state)
{
	if (state)
	{
		gtk_widget_show(sidebar_vbox_bars);
		if (!CONFIG_SHOW_BARS)
			CONFIG_SHOW_BARS = 1;
	}
	else
	{
		gtk_widget_hide(sidebar_vbox_bars);
		CONFIG_SHOW_BARS = 0;
	}

	save_settings();
}

static void
gtk_tree_store_iter_clear_nodes(gpointer iter, gboolean delete_root)
{
	GtkTreeIter i;

	while (gtk_tree_model_iter_children(GTK_TREE_MODEL(treestore), &i, iter))
	{
		if (gtk_tree_model_iter_has_child(GTK_TREE_MODEL(treestore), &i))
			gtk_tree_store_iter_clear_nodes(&i, TRUE);
		gtk_tree_store_remove(GTK_TREE_STORE(treestore), &i);
	}
	if (delete_root)
		gtk_tree_store_remove(GTK_TREE_STORE(treestore), iter);
}

static gboolean
treebrowser_track_current()
{

	GeanyDocument	*doc 		= document_get_current();
	gchar 			*path_current;
	gchar			**path_segments;

	if (doc != NULL && doc->file_name != NULL && g_path_is_absolute(doc->file_name))
	{
		path_current = utils_get_locale_from_utf8(doc->file_name);

		path_segments = g_strsplit(path_current, G_DIR_SEPARATOR_S, 0);

		treebrowser_search(path_current, NULL);
		/*
		 * NEED TO REWORK THE CONCEPT
		 */

		g_strfreev(path_segments);
		g_free(path_current);

		return FALSE;
	}
	return FALSE;
}

static gboolean
treebrowser_iter_rename(gpointer iter)
{
	GtkTreeViewColumn 	*column;
	GtkCellRenderer 	*renderer;
	GtkTreePath 		*path;
	GList 				*renderers;

	if (gtk_tree_store_iter_is_valid(treestore, iter))
	{
		path = gtk_tree_model_get_path(GTK_TREE_MODEL(treestore), iter);
		if (G_LIKELY(path != NULL))
		{
			column 		= gtk_tree_view_get_column(GTK_TREE_VIEW(treeview), 0);
			renderers 	= _gtk_cell_layout_get_cells(column);
			renderer 	= g_list_nth_data(renderers, TREEBROWSER_RENDER_TEXT);

			g_object_set(G_OBJECT(renderer), "editable", TRUE, NULL);
			gtk_tree_view_set_cursor_on_cell(GTK_TREE_VIEW(treeview), path, column, renderer, TRUE);

			gtk_tree_path_free(path);
			g_list_free(renderers);
			return TRUE;
		}
	}
	return FALSE;
}

static void
treebrowser_rename_current()
{
	GtkTreeSelection 	*selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	GtkTreeIter 		iter;
	GtkTreeModel 		*model;

	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		treebrowser_iter_rename(&iter);
	}
}

/* ------------------
 * RIGHTCLICK MENU EVENTS
 * ------------------*/

static void
on_menu_go_up(GtkMenuItem *menuitem, gpointer *user_data)
{
	gchar *uri;

	uri = g_path_get_dirname(addressbar_last_address);
	treebrowser_chroot(uri);
	g_free(uri);
}

static void
on_menu_current_path(GtkMenuItem *menuitem, gpointer *user_data)
{
	gchar *uri;

	uri = get_default_dir();
	treebrowser_chroot(uri);
	g_free(uri);
}

static void
on_menu_open_externally(GtkMenuItem *menuitem, gchar *uri)
{
	gchar 				*cmd, *locale_cmd, *dir, *c;
	GString 			*cmd_str 	= g_string_new(CONFIG_OPEN_EXTERNAL_CMD);
	GError 				*error 		= NULL;

	dir = g_file_test(uri, G_FILE_TEST_IS_DIR) ? g_strdup(uri) : g_path_get_dirname(uri);

	utils_string_replace_all(cmd_str, "%f", uri);
	utils_string_replace_all(cmd_str, "%d", dir);

	cmd = g_string_free(cmd_str, FALSE);
	locale_cmd = utils_get_locale_from_utf8(cmd);
	if (! g_spawn_command_line_async(locale_cmd, &error))
	{
		c = strchr(cmd, ' ');
		if (c != NULL)
			*c = '\0';
		ui_set_statusbar(TRUE,
			_("Could not execute configured external command '%s' (%s)."),
			cmd, error->message);
		g_error_free(error);
	}
	g_free(locale_cmd);
	g_free(cmd);
	g_free(dir);
}

static void
on_menu_open_terminal(GtkMenuItem *menuitem, gchar *uri)
{
	gchar *argv[2] = {NULL, NULL};
	argv[0] = get_terminal();

	if (g_file_test(uri, G_FILE_TEST_EXISTS))
		uri = g_file_test(uri, G_FILE_TEST_IS_DIR) ? g_strdup(uri) : g_path_get_dirname(uri);
	else
		uri = g_strdup(addressbar_last_address);

	g_spawn_async(uri, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
	g_free(uri);
	g_free(argv[0]);
}

static void
on_menu_set_as_root(GtkMenuItem *menuitem, gchar *uri)
{
	if (g_file_test(uri, G_FILE_TEST_IS_DIR))
		treebrowser_chroot(uri);
}

static void
on_menu_create_new_object(GtkMenuItem *menuitem, gchar *type)
{
	GtkTreeSelection 	*selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	GtkTreeIter 		iter;
	GtkTreeModel 		*model;
	gchar 				*uri, *uri_new = NULL;
	GtkTreePath 		*path_parent;
	gboolean 			refresh_root = FALSE;

	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		gtk_tree_model_get(model, &iter, TREEBROWSER_COLUMN_URI, &uri, -1);
		if (! g_file_test(uri, G_FILE_TEST_IS_DIR))
		{
			setptr(uri, g_path_get_dirname(uri));
			path_parent = gtk_tree_model_get_path(GTK_TREE_MODEL(treestore), &iter);
			if (gtk_tree_path_up(path_parent) &&
				gtk_tree_model_get_iter(GTK_TREE_MODEL(treestore), &iter, path_parent));
			else
				refresh_root = TRUE;
		}
	}
	else
	{
		refresh_root 	= TRUE;
		uri 			= g_strdup(addressbar_last_address);
	}

	if (utils_str_equal(type, "directory"))
		uri_new = g_strconcat(uri, G_DIR_SEPARATOR_S, _("NewDirectory"), NULL);
	else if (utils_str_equal(type, "file"))
		uri_new = g_strconcat(uri, G_DIR_SEPARATOR_S, _("NewFile"), NULL);

	if (uri_new)
	{
		if (!(g_file_test(uri_new, G_FILE_TEST_EXISTS) &&
			!dialogs_show_question(_("Target file '%s' exists\n, do you really want to replace it with empty file?"), uri_new)))
		{
			gboolean creation_success = FALSE;

			while(g_file_test(uri_new, G_FILE_TEST_EXISTS))
				setptr(uri_new, g_strconcat(uri_new, "_", NULL));

			if (utils_str_equal(type, "directory"))
				creation_success = (g_mkdir(uri_new, 0755) == 0);
			else
				creation_success = (g_creat(uri_new, 0755) != -1);

			if (creation_success)
			{
				treebrowser_browse(uri, refresh_root ? NULL : &iter);
				if (treebrowser_search(uri_new, NULL))
					treebrowser_rename_current();
			}
		}
		g_free(uri_new);
	}
	g_free(uri);
}

static void
on_menu_rename(GtkMenuItem *menuitem, gpointer *user_data)
{
	treebrowser_rename_current();
}

static void
on_menu_delete(GtkMenuItem *menuitem, gpointer *user_data)
{

	GtkTreeSelection 	*selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	GtkTreeIter 		iter;
	GtkTreeModel 		*model;
	GtkTreePath 		*path_parent;
	gchar 				*uri, *uri_parent;

	if (! gtk_tree_selection_get_selected(selection, &model, &iter))
		return;

	gtk_tree_model_get(model, &iter, TREEBROWSER_COLUMN_URI, &uri, -1);

	if (dialogs_show_question(_("Do you really want to delete '%s' ?"), uri))
	{
		if (CONFIG_ON_DELETE_CLOSE_FILE && !g_file_test(uri, G_FILE_TEST_IS_DIR))
			document_close(document_find_by_filename(uri));

		uri_parent = g_path_get_dirname(uri);
		fs_remove(uri, TRUE);
		path_parent = gtk_tree_model_get_path(GTK_TREE_MODEL(treestore), &iter);
		if (gtk_tree_path_up(path_parent))
		{
			if (gtk_tree_model_get_iter(GTK_TREE_MODEL(treestore), &iter, path_parent))
				treebrowser_browse(uri_parent, &iter);
			else
				treebrowser_browse(uri_parent, NULL);
		}
		else
			treebrowser_browse(uri_parent, NULL);
		gtk_tree_path_free(path_parent);
		g_free(uri_parent);
	}
	g_free(uri);
}

static void
on_menu_refresh(GtkMenuItem *menuitem, gpointer *user_data)
{
	GtkTreeSelection 	*selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	GtkTreeIter 		iter;
	GtkTreeModel 		*model;
	gchar 				*uri;

	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		gtk_tree_model_get(model, &iter, TREEBROWSER_COLUMN_URI, &uri, -1);
		if (g_file_test(uri, G_FILE_TEST_IS_DIR))
			treebrowser_browse(uri, &iter);
		g_free(uri);
	}
	else
		treebrowser_browse(addressbar_last_address, NULL);
}

static void
on_menu_expand_all(GtkMenuItem *menuitem, gpointer *user_data)
{
	gtk_tree_view_expand_all(GTK_TREE_VIEW(treeview));
}

static void
on_menu_collapse_all(GtkMenuItem *menuitem, gpointer *user_data)
{
	gtk_tree_view_collapse_all(GTK_TREE_VIEW(treeview));
}

static void
on_menu_close(GtkMenuItem *menuitem, gchar *uri)
{
	if (g_file_test(uri, G_FILE_TEST_EXISTS))
		document_close(document_find_by_filename(uri));
}

static void
on_menu_close_children(GtkMenuItem *menuitem, gchar *uri)
{
	guint nb_documents = geany->documents_array->len;
	int i;
	int uri_len=strlen(uri);
	for(i=0; i<GEANY(documents_array)->len; i++)
	{
		if(documents[i]->is_valid)
		{
			/* the docuemnt filename shoudl always be longer than the uri when closing children
			 * Compare the beginingin of the filename string to see if it matchs the uri*/
			if(strlen(documents[i]->file_name)>uri_len)
			{
				if(strncmp(uri,documents[i]->file_name,uri_len)==0)
					document_close(documents[i]);
			}
		}
	}
}

static void
on_menu_copy_uri(GtkMenuItem *menuitem, gchar *uri)
{
	GtkClipboard *cb = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_set_text(cb, uri, -1);
}

static void
on_menu_show_bookmarks(GtkMenuItem *menuitem, gpointer *user_data)
{
	CONFIG_SHOW_BOOKMARKS = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem));
	save_settings();
	treebrowser_chroot(addressbar_last_address);
}

static void
on_menu_show_hidden_files(GtkMenuItem *menuitem, gpointer *user_data)
{
	CONFIG_SHOW_HIDDEN_FILES = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem));
	save_settings();
	treebrowser_browse(addressbar_last_address, NULL);
}

static void
on_menu_show_bars(GtkMenuItem *menuitem, gpointer *user_data)
{
	showbars(gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem)));
}

static GtkWidget*
create_popup_menu(gchar *name, gchar *uri)
{
	GtkWidget *item, *menu = gtk_menu_new();

	gboolean is_exists 		= g_file_test(uri, G_FILE_TEST_EXISTS);
	gboolean is_dir 		= is_exists ? g_file_test(uri, G_FILE_TEST_IS_DIR) : FALSE;
	gboolean is_document 	= document_find_by_filename(uri) != NULL ? TRUE : FALSE;

	item = ui_image_menu_item_new(GTK_STOCK_GO_UP, _("Go up"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_go_up), NULL);

	item = ui_image_menu_item_new(GTK_STOCK_GO_UP, _("Set path from document"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_current_path), NULL);

	item = ui_image_menu_item_new(GTK_STOCK_OPEN, _("Open externally"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_open_externally), uri);
	gtk_widget_set_sensitive(item, is_exists);

	item = ui_image_menu_item_new("utilities-terminal", _("Open Terminal"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_open_terminal), uri);

	item = ui_image_menu_item_new(GTK_STOCK_GOTO_TOP, _("Set as root"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_set_as_root), uri);
	gtk_widget_set_sensitive(item, is_dir);

	item = ui_image_menu_item_new(GTK_STOCK_REFRESH, _("Refresh"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_refresh), NULL);

	item = gtk_separator_menu_item_new();
	gtk_container_add(GTK_CONTAINER(menu), item);

	item = ui_image_menu_item_new(GTK_STOCK_ADD, _("Create new directory"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_create_new_object), "directory");

	item = ui_image_menu_item_new(GTK_STOCK_NEW, _("Create new file"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_create_new_object), "file");

	item = ui_image_menu_item_new(GTK_STOCK_SAVE_AS, _("Rename"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_rename), NULL);
	gtk_widget_set_sensitive(item, is_exists);

	item = ui_image_menu_item_new(GTK_STOCK_DELETE, _("Delete"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_delete), NULL);
	gtk_widget_set_sensitive(item, is_exists);

	item = gtk_separator_menu_item_new();
	gtk_container_add(GTK_CONTAINER(menu), item);

	item = ui_image_menu_item_new(GTK_STOCK_CLOSE, g_strdup_printf(_("Close: %s"), name));
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_close), uri);
	gtk_widget_set_sensitive(item, is_document);

	item = ui_image_menu_item_new(GTK_STOCK_CLOSE, g_strdup_printf(_("Close Child Documents ")));
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_close_children), uri);
	gtk_widget_set_sensitive(item, is_dir);

	item = ui_image_menu_item_new(GTK_STOCK_COPY, _("Copy full path to clipboard"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_copy_uri), uri);
	gtk_widget_set_sensitive(item, is_exists);

	item = gtk_separator_menu_item_new();
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_widget_show(item);

	item = ui_image_menu_item_new(GTK_STOCK_GO_FORWARD, _("Expand all"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_expand_all), NULL);

	item = ui_image_menu_item_new(GTK_STOCK_GO_BACK, _("Collapse all"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_collapse_all), NULL);

	item = gtk_separator_menu_item_new();
	gtk_container_add(GTK_CONTAINER(menu), item);

	item = gtk_check_menu_item_new_with_mnemonic(_("Show bookmarks"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), CONFIG_SHOW_BOOKMARKS);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_show_bookmarks), NULL);

	item = gtk_check_menu_item_new_with_mnemonic(_("Show hidden files"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), CONFIG_SHOW_HIDDEN_FILES);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_show_hidden_files), NULL);

	item = gtk_check_menu_item_new_with_mnemonic(_("Show toolbars"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), CONFIG_SHOW_BARS ? TRUE : FALSE);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_show_bars), NULL);

	gtk_widget_show_all(menu);

	return menu;
}


/* ------------------
 * TOOLBAR`S EVENTS
 * ------------------ */

static void
on_button_go_up()
{
	gchar *uri;

	uri = g_path_get_dirname(addressbar_last_address);
	treebrowser_chroot(uri);
	g_free(uri);
}

static void
on_button_refresh()
{
	treebrowser_chroot(addressbar_last_address);
}

static void
on_button_go_home()
{
	gchar *uri;

	uri = g_strdup(g_get_home_dir());
	treebrowser_chroot(uri);
	g_free(uri);
}

static void
on_button_current_path()
{
	gchar *uri;

	uri = get_default_dir();
	treebrowser_chroot(uri);
	g_free(uri);
}

static void
on_button_hide_bars()
{
	showbars(FALSE);
}

static void
on_addressbar_activate(GtkEntry *entry, gpointer user_data)
{
	gchar *directory = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
	treebrowser_chroot(directory);
}

static void
on_filter_activate(GtkEntry *entry, gpointer user_data)
{
	treebrowser_chroot(addressbar_last_address);
}

static void
on_filter_clear(GtkEntry *entry, gint icon_pos, GdkEvent *event, gpointer data)
{
	gtk_entry_set_text(entry, "");
	treebrowser_chroot(addressbar_last_address);
}


/* ------------------
 * TREEVIEW EVENTS
 * ------------------ */

static gboolean
on_treeview_mouseclick(GtkWidget *widget, GdkEventButton *event, GtkTreeSelection *selection)
{
	GtkTreeIter 	iter;
	GtkTreeModel 	*model;
	gchar 			*name = "", *uri = "";

	if (gtk_tree_selection_get_selected(selection, &model, &iter))
		/* FIXME: name and uri should be freed, but they are passed to create_popup_menu()
		 * that pass them directly to some callbacks, so we can't free them here for now.
		 * Gotta find a way out... */
		gtk_tree_model_get(GTK_TREE_MODEL(treestore), &iter,
							TREEBROWSER_COLUMN_NAME, &name,
							TREEBROWSER_COLUMN_URI, &uri,
							-1);

	if (event->button == 3)
	{
		gtk_menu_popup(GTK_MENU(create_popup_menu(name, uri)), NULL, NULL, NULL, NULL, event->button, event->time);
		return TRUE;
	}

	return FALSE;
}

static void
on_treeview_changed(GtkWidget *widget, gpointer user_data)
{
	GtkTreeIter 	iter;
	GtkTreeModel 	*model;
	gchar 			*uri;

	if (gtk_tree_selection_get_selected(GTK_TREE_SELECTION(widget), &model, &iter))
	{
		gtk_tree_model_get(GTK_TREE_MODEL(treestore), &iter,
							TREEBROWSER_COLUMN_URI, &uri,
							-1);
		if (uri == NULL)
			return;

		if (g_file_test(uri, G_FILE_TEST_EXISTS))
		{
			if (g_file_test(uri, G_FILE_TEST_IS_DIR))
				treebrowser_browse(uri, &iter);
			else
				if (CONFIG_ONE_CLICK_CHDOC)
					document_open_file(uri, FALSE, NULL, NULL);
		}
		else
			gtk_tree_store_iter_clear_nodes(&iter, TRUE);

		g_free(uri);
	}
}

static void
on_treeview_row_activated(GtkWidget *widget, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data)
{
	GtkTreeIter 	iter;
	gchar 			*uri;

	gtk_tree_model_get_iter(GTK_TREE_MODEL(treestore), &iter, path);
	gtk_tree_model_get(GTK_TREE_MODEL(treestore), &iter, TREEBROWSER_COLUMN_URI, &uri, -1);

	if (uri == NULL)
		return;

	if (g_file_test (uri, G_FILE_TEST_IS_DIR))
		if (CONFIG_CHROOT_ON_DCLICK)
			treebrowser_chroot(uri);
		else
			if (gtk_tree_view_row_expanded(GTK_TREE_VIEW(widget), path))
				gtk_tree_view_collapse_row(GTK_TREE_VIEW(widget), path);
			else
				gtk_tree_view_expand_row(GTK_TREE_VIEW(widget), path, FALSE);
	else
		document_open_file(uri, FALSE, NULL, NULL);

	g_free(uri);
}

static void
on_treeview_row_expanded(GtkWidget *widget, GtkTreeIter *iter, GtkTreePath *path, gpointer user_data)
{
	gchar *uri;

	gtk_tree_model_get(GTK_TREE_MODEL(treestore), iter, TREEBROWSER_COLUMN_URI, &uri, -1);
	if (uri == NULL)
		return;

	if (flag_on_expand_refresh == FALSE)
	{
		flag_on_expand_refresh = TRUE;
		treebrowser_browse(uri, iter);
		gtk_tree_view_expand_row(GTK_TREE_VIEW(treeview), path, FALSE);
		flag_on_expand_refresh = FALSE;
	}
	if (CONFIG_SHOW_ICONS)
	{
		GdkPixbuf *icon = utils_pixbuf_from_stock(GTK_STOCK_OPEN);

		gtk_tree_store_set(treestore, iter, TREEBROWSER_COLUMN_ICON, icon, -1);
		g_object_unref(icon);
	}

	g_free(uri);
}

static void
on_treeview_row_collapsed(GtkWidget *widget, GtkTreeIter *iter, GtkTreePath *path, gpointer user_data)
{
	gchar *uri;
	gtk_tree_model_get(GTK_TREE_MODEL(treestore), iter, TREEBROWSER_COLUMN_URI, &uri, -1);
	if (uri == NULL)
		return;
	if (CONFIG_SHOW_ICONS)
	{
		GdkPixbuf *icon = utils_pixbuf_from_stock(GTK_STOCK_DIRECTORY);

		gtk_tree_store_set(treestore, iter, TREEBROWSER_COLUMN_ICON, icon, -1);
		g_object_unref(icon);
	}
	g_free(uri);
}

static void
on_treeview_renamed(GtkCellRenderer *renderer, const gchar *path_string, const gchar *name_new, gpointer user_data)
{

	GtkTreeViewColumn 	*column;
	GList 				*renderers;
	GtkTreeIter 		iter, iter_parent;
	gchar 				*uri, *uri_new;
	GtkTreePath 		*path_parent;

	column 		= gtk_tree_view_get_column(GTK_TREE_VIEW(treeview), 0);
	renderers 	= _gtk_cell_layout_get_cells(column);
	renderer 	= g_list_nth_data(renderers, TREEBROWSER_RENDER_TEXT);

	g_object_set(G_OBJECT(renderer), "editable", FALSE, NULL);

	if (gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(treestore), &iter, path_string))
	{
		gtk_tree_model_get(GTK_TREE_MODEL(treestore), &iter, TREEBROWSER_COLUMN_URI, &uri, -1);
		if (uri)
		{
			uri_new = g_strconcat(g_path_get_dirname(uri), G_DIR_SEPARATOR_S, name_new, NULL);
			if (!(g_file_test(uri_new, G_FILE_TEST_EXISTS) &&
				!dialogs_show_question(_("Target file '%s' exists, do you really want to replace it?"), uri_new)))
			{
				if (g_rename(uri, uri_new) == 0)
				{
					gtk_tree_store_set(treestore, &iter,
									TREEBROWSER_COLUMN_NAME, name_new,
									TREEBROWSER_COLUMN_URI, uri_new,
									-1);
					path_parent = gtk_tree_model_get_path(GTK_TREE_MODEL(treestore), &iter);
					if (gtk_tree_path_up(path_parent))
					{
						if (gtk_tree_model_get_iter(GTK_TREE_MODEL(treestore), &iter_parent, path_parent))
							treebrowser_browse(g_path_get_dirname(uri_new), &iter_parent);
						else
							treebrowser_browse(g_path_get_dirname(uri_new), NULL);
					}
					else
						treebrowser_browse(g_path_get_dirname(uri_new), NULL);

					if (!g_file_test(uri, G_FILE_TEST_IS_DIR))
						if (document_close(document_find_by_filename(uri)))
							document_open_file(uri_new, FALSE, NULL, NULL);
				}
			}
			g_free(uri_new);
			g_free(uri);
		}
	}
}

static void
treebrowser_track_current_cb()
{
	if (CONFIG_FOLLOW_CURRENT_DOC)
		treebrowser_track_current();
}


/* ------------------
 * TREEBROWSER INITIAL FUNCTIONS
 * ------------------ */

static gboolean
treeview_separator_func(GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
	gint flag;
	gtk_tree_model_get(model, iter, TREEBROWSER_COLUMN_FLAG, &flag, -1);
	return (flag == TREEBROWSER_FLAGS_SEPARATOR);
}

static GtkWidget*
create_view_and_model()
{

	GtkWidget 			*view;

	view 					= gtk_tree_view_new();
	treeview_column_text	= gtk_tree_view_column_new();
	render_icon 			= gtk_cell_renderer_pixbuf_new();
	render_text 			= gtk_cell_renderer_text_new();

	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), FALSE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), treeview_column_text);

	gtk_tree_view_column_pack_start(treeview_column_text, render_icon, FALSE);
	gtk_tree_view_column_set_attributes(treeview_column_text, render_icon, "pixbuf", TREEBROWSER_RENDER_ICON, NULL);

	gtk_tree_view_column_pack_start(treeview_column_text, render_text, TRUE);
	gtk_tree_view_column_add_attribute(treeview_column_text, render_text, "text", TREEBROWSER_RENDER_TEXT);

	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(view), TRUE);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(view), TREEBROWSER_COLUMN_NAME);

	gtk_tree_view_set_row_separator_func(GTK_TREE_VIEW(view), treeview_separator_func, NULL, NULL);

	ui_widget_modify_font_from_string(view, geany->interface_prefs->tagbar_font);

#if GTK_CHECK_VERSION(2, 10, 0)
	g_object_set(view, "has-tooltip", TRUE, "tooltip-column", TREEBROWSER_COLUMN_URI, NULL);
#endif

	gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(view)), GTK_SELECTION_SINGLE);

#if GTK_CHECK_VERSION(2, 10, 0)
	gtk_tree_view_set_enable_tree_lines(GTK_TREE_VIEW(view), CONFIG_SHOW_TREE_LINES);
#endif

	treestore = gtk_tree_store_new(TREEBROWSER_COLUMNC, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);

	gtk_tree_view_set_model(GTK_TREE_VIEW(view), GTK_TREE_MODEL(treestore));
	g_signal_connect(G_OBJECT(render_text), "edited", G_CALLBACK(on_treeview_renamed), view);

	return view;
}

static void
create_sidebar()
{
	GtkWidget 			*scrollwin;
	GtkWidget 			*toolbar;
	GtkWidget 			*wid;
	GtkTreeSelection 	*selection;

	treeview 				= create_view_and_model();
	sidebar_vbox 			= gtk_vbox_new(FALSE, 0);
	sidebar_vbox_bars 		= gtk_vbox_new(FALSE, 0);
	selection 				= gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	addressbar 				= gtk_entry_new();
	filter 					= gtk_entry_new();
	scrollwin 				= gtk_scrolled_window_new(NULL, NULL);

	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollwin), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	toolbar = gtk_toolbar_new();
	gtk_toolbar_set_icon_size(GTK_TOOLBAR(toolbar), GTK_ICON_SIZE_MENU);
	gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);

	wid = GTK_WIDGET(gtk_tool_button_new_from_stock(GTK_STOCK_GO_UP));
	ui_widget_set_tooltip_text(wid, _("Go up"));
	g_signal_connect(wid, "clicked", G_CALLBACK(on_button_go_up), NULL);
	gtk_container_add(GTK_CONTAINER(toolbar), wid);

	wid = GTK_WIDGET(gtk_tool_button_new_from_stock(GTK_STOCK_REFRESH));
	ui_widget_set_tooltip_text(wid, _("Refresh"));
	g_signal_connect(wid, "clicked", G_CALLBACK(on_button_refresh), NULL);
	gtk_container_add(GTK_CONTAINER(toolbar), wid);

	wid = GTK_WIDGET(gtk_tool_button_new_from_stock(GTK_STOCK_HOME));
	ui_widget_set_tooltip_text(wid, _("Home"));
	g_signal_connect(wid, "clicked", G_CALLBACK(on_button_go_home), NULL);
	gtk_container_add(GTK_CONTAINER(toolbar), wid);

	wid = GTK_WIDGET(gtk_tool_button_new_from_stock(GTK_STOCK_JUMP_TO));
	ui_widget_set_tooltip_text(wid, _("Set path from document"));
	g_signal_connect(wid, "clicked", G_CALLBACK(on_button_current_path), NULL);
	gtk_container_add(GTK_CONTAINER(toolbar), wid);

	wid = GTK_WIDGET(gtk_tool_button_new_from_stock(GTK_STOCK_DIRECTORY));
	ui_widget_set_tooltip_text(wid, _("Track path"));
	g_signal_connect(wid, "clicked", G_CALLBACK(treebrowser_track_current), NULL);
	gtk_container_add(GTK_CONTAINER(toolbar), wid);

	wid = GTK_WIDGET(gtk_tool_button_new_from_stock(GTK_STOCK_CLOSE));
	ui_widget_set_tooltip_text(wid, _("Hide bars"));
	g_signal_connect(wid, "clicked", G_CALLBACK(on_button_hide_bars), NULL);
	gtk_container_add(GTK_CONTAINER(toolbar), wid);

	gtk_container_add(GTK_CONTAINER(scrollwin), 			treeview);
	gtk_box_pack_start(GTK_BOX(sidebar_vbox_bars), 			filter, 			FALSE, TRUE,  1);
	gtk_box_pack_start(GTK_BOX(sidebar_vbox_bars), 			addressbar, 		FALSE, TRUE,  1);
	gtk_box_pack_start(GTK_BOX(sidebar_vbox_bars), 			toolbar, 			FALSE, TRUE,  1);

	ui_widget_set_tooltip_text(filter,
		_("Filter (*.c;*.h;*.cpp), and if you want temporary filter using the '!' reverse try for example this '!;*.c;*.h;*.cpp'"));
	if (gtk_check_version(2, 15, 2) == NULL)
	{
		ui_entry_add_clear_icon(GTK_ENTRY(filter));
		g_signal_connect(filter, "icon-release", G_CALLBACK(on_filter_clear), NULL);
	}

	ui_widget_set_tooltip_text(addressbar,
		_("Addressbar for example '/projects/my-project'"));

	if (CONFIG_SHOW_BARS == 2)
	{
		gtk_box_pack_start(GTK_BOX(sidebar_vbox), 				scrollwin, 			TRUE,  TRUE,  1);
		gtk_box_pack_start(GTK_BOX(sidebar_vbox), 				sidebar_vbox_bars, 	FALSE, TRUE,  1);
	}
	else
	{
		gtk_box_pack_start(GTK_BOX(sidebar_vbox), 				sidebar_vbox_bars, 	FALSE, TRUE,  1);
		gtk_box_pack_start(GTK_BOX(sidebar_vbox), 				scrollwin, 			TRUE,  TRUE,  1);
	}

	g_signal_connect(selection, 		"changed", 				G_CALLBACK(on_treeview_changed), 				NULL);
	g_signal_connect(treeview, 			"button-press-event", 	G_CALLBACK(on_treeview_mouseclick), 			selection);
	g_signal_connect(treeview, 			"row-activated", 		G_CALLBACK(on_treeview_row_activated), 			NULL);
	g_signal_connect(treeview, 			"row-collapsed", 		G_CALLBACK(on_treeview_row_collapsed), 			NULL);
	g_signal_connect(treeview, 			"row-expanded", 		G_CALLBACK(on_treeview_row_expanded), 			NULL);
	g_signal_connect(addressbar, 		"activate", 			G_CALLBACK(on_addressbar_activate), 			NULL);
	g_signal_connect(filter, 			"activate", 			G_CALLBACK(on_filter_activate), 				NULL);

	gtk_widget_show_all(sidebar_vbox);

	page_number = gtk_notebook_append_page(GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook),
							sidebar_vbox, gtk_label_new(_("Tree Browser")));

	showbars(CONFIG_SHOW_BARS);
}


/* ------------------
 * CONFIG DIALOG
 * ------------------ */

static struct
{
	GtkWidget *OPEN_EXTERNAL_CMD;
	GtkWidget *REVERSE_FILTER;
	GtkWidget *ONE_CLICK_CHDOC;
	GtkWidget *SHOW_HIDDEN_FILES;
	GtkWidget *HIDE_OBJECT_FILES;
	GtkWidget *SHOW_BARS;
	GtkWidget *CHROOT_ON_DCLICK;
	GtkWidget *FOLLOW_CURRENT_DOC;
	GtkWidget *ON_DELETE_CLOSE_FILE;
	GtkWidget *SHOW_TREE_LINES;
	GtkWidget *SHOW_BOOKMARKS;
	GtkWidget *SHOW_ICONS;

} configure_widgets;

static void
load_settings()
{
	GKeyFile *config 	= g_key_file_new();

	g_key_file_load_from_file(config, CONFIG_FILE, G_KEY_FILE_NONE, NULL);

	CONFIG_OPEN_EXTERNAL_CMD 		=  utils_get_setting_string(config, "treebrowser", "open_external_cmd", 	CONFIG_OPEN_EXTERNAL_CMD);
	CONFIG_REVERSE_FILTER 			= utils_get_setting_boolean(config, "treebrowser", "reverse_filter", 		CONFIG_REVERSE_FILTER);
	CONFIG_ONE_CLICK_CHDOC 			= utils_get_setting_boolean(config, "treebrowser", "one_click_chdoc", 		CONFIG_ONE_CLICK_CHDOC);
	CONFIG_SHOW_HIDDEN_FILES 		= utils_get_setting_boolean(config, "treebrowser", "show_hidden_files", 	CONFIG_SHOW_HIDDEN_FILES);
	CONFIG_HIDE_OBJECT_FILES 		= utils_get_setting_boolean(config, "treebrowser", "hide_object_files", 	CONFIG_HIDE_OBJECT_FILES);
	CONFIG_SHOW_BARS 				= utils_get_setting_integer(config, "treebrowser", "show_bars", 			CONFIG_SHOW_BARS);
	CONFIG_CHROOT_ON_DCLICK 		= utils_get_setting_boolean(config, "treebrowser", "chroot_on_dclick", 		CONFIG_CHROOT_ON_DCLICK);
	CONFIG_FOLLOW_CURRENT_DOC 		= utils_get_setting_boolean(config, "treebrowser", "follow_current_doc", 	CONFIG_FOLLOW_CURRENT_DOC);
	CONFIG_ON_DELETE_CLOSE_FILE 	= utils_get_setting_boolean(config, "treebrowser", "on_delete_close_file", 	CONFIG_ON_DELETE_CLOSE_FILE);
	CONFIG_SHOW_TREE_LINES 			= utils_get_setting_boolean(config, "treebrowser", "show_tree_lines", 		CONFIG_SHOW_TREE_LINES);
	CONFIG_SHOW_BOOKMARKS 			= utils_get_setting_boolean(config, "treebrowser", "show_bookmarks", 		CONFIG_SHOW_BOOKMARKS);
	CONFIG_SHOW_ICONS 				= utils_get_setting_integer(config, "treebrowser", "show_icons", 			CONFIG_SHOW_ICONS);

	g_key_file_free(config);
}

static gboolean
save_settings()
{
	GKeyFile 	*config 		= g_key_file_new();
	gchar 		*config_dir 	= g_path_get_dirname(CONFIG_FILE);
	gchar 		*data;

	g_key_file_load_from_file(config, CONFIG_FILE, G_KEY_FILE_NONE, NULL);
	if (! g_file_test(config_dir, G_FILE_TEST_IS_DIR) && utils_mkdir(config_dir, TRUE) != 0)
	{
		g_free(config_dir);
		g_key_file_free(config);
		return FALSE;
	}

	g_key_file_set_string(config, 	"treebrowser", "open_external_cmd", 	CONFIG_OPEN_EXTERNAL_CMD);
	g_key_file_set_boolean(config, 	"treebrowser", "reverse_filter", 		CONFIG_REVERSE_FILTER);
	g_key_file_set_boolean(config, 	"treebrowser", "one_click_chdoc", 		CONFIG_ONE_CLICK_CHDOC);
	g_key_file_set_boolean(config, 	"treebrowser", "show_hidden_files", 	CONFIG_SHOW_HIDDEN_FILES);
	g_key_file_set_boolean(config, 	"treebrowser", "hide_object_files", 	CONFIG_HIDE_OBJECT_FILES);
	g_key_file_set_integer(config, 	"treebrowser", "show_bars", 			CONFIG_SHOW_BARS);
	g_key_file_set_boolean(config, 	"treebrowser", "chroot_on_dclick", 		CONFIG_CHROOT_ON_DCLICK);
	g_key_file_set_boolean(config, 	"treebrowser", "follow_current_doc", 	CONFIG_FOLLOW_CURRENT_DOC);
	g_key_file_set_boolean(config, 	"treebrowser", "on_delete_close_file", 	CONFIG_ON_DELETE_CLOSE_FILE);
	g_key_file_set_boolean(config, 	"treebrowser", "show_tree_lines", 		CONFIG_SHOW_TREE_LINES);
	g_key_file_set_boolean(config, 	"treebrowser", "show_bookmarks", 		CONFIG_SHOW_BOOKMARKS);
	g_key_file_set_integer(config, 	"treebrowser", "show_icons", 			CONFIG_SHOW_ICONS);

	data = g_key_file_to_data(config, NULL, NULL);
	utils_write_file(CONFIG_FILE, data);
	g_free(data);

	g_free(config_dir);
	g_key_file_free(config);

	return TRUE;
}

static void
on_configure_response(GtkDialog *dialog, gint response, gpointer user_data)
{

	if (! (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY))
		return;

	CONFIG_OPEN_EXTERNAL_CMD 	= gtk_editable_get_chars(GTK_EDITABLE(configure_widgets.OPEN_EXTERNAL_CMD), 0, -1);
	CONFIG_REVERSE_FILTER 		= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(configure_widgets.REVERSE_FILTER));
	CONFIG_ONE_CLICK_CHDOC 		= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(configure_widgets.ONE_CLICK_CHDOC));
	CONFIG_SHOW_HIDDEN_FILES 	= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(configure_widgets.SHOW_HIDDEN_FILES));
	CONFIG_HIDE_OBJECT_FILES 	= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(configure_widgets.HIDE_OBJECT_FILES));
	CONFIG_SHOW_BARS 			= gtk_combo_box_get_active(GTK_COMBO_BOX(configure_widgets.SHOW_BARS));
	CONFIG_CHROOT_ON_DCLICK 	= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(configure_widgets.CHROOT_ON_DCLICK));
	CONFIG_FOLLOW_CURRENT_DOC 	= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(configure_widgets.FOLLOW_CURRENT_DOC));
	CONFIG_ON_DELETE_CLOSE_FILE = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(configure_widgets.ON_DELETE_CLOSE_FILE));
	CONFIG_SHOW_TREE_LINES 		= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(configure_widgets.SHOW_TREE_LINES));
	CONFIG_SHOW_BOOKMARKS 		= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(configure_widgets.SHOW_BOOKMARKS));
	CONFIG_SHOW_ICONS 			= gtk_combo_box_get_active(GTK_COMBO_BOX(configure_widgets.SHOW_ICONS));

	if (save_settings() == TRUE)
	{
#if GTK_CHECK_VERSION(2, 10, 0)
		gtk_tree_view_set_enable_tree_lines(GTK_TREE_VIEW(treeview), CONFIG_SHOW_TREE_LINES);
#endif
		treebrowser_chroot(addressbar_last_address);
		if (CONFIG_SHOW_BOOKMARKS)
			treebrowser_load_bookmarks();
		showbars(CONFIG_SHOW_BARS);
	}
	else
		dialogs_show_msgbox(GTK_MESSAGE_ERROR,
			_("Plugin configuration directory could not be created."));
}

GtkWidget*
plugin_configure(GtkDialog *dialog)
{
	GtkWidget 		*label;
	GtkWidget 		*vbox, *hbox;

	vbox 	= gtk_vbox_new(FALSE, 0);
	hbox 	= gtk_hbox_new(FALSE, 0);

	label 	= gtk_label_new(_("External open command"));
	configure_widgets.OPEN_EXTERNAL_CMD = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(configure_widgets.OPEN_EXTERNAL_CMD), CONFIG_OPEN_EXTERNAL_CMD);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	ui_widget_set_tooltip_text(configure_widgets.OPEN_EXTERNAL_CMD,
		_("The command to execute when using \"Open with\". You can use %f and %d wildcards.\n"
		  "%f will be replaced with the filename including full path\n"
		  "%d will be replaced with the path name of the selected file without the filename"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 6);
	gtk_box_pack_start(GTK_BOX(hbox), configure_widgets.OPEN_EXTERNAL_CMD, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 6);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Toolbar"));
	configure_widgets.SHOW_BARS = gtk_combo_box_new_text();
	gtk_combo_box_append_text( GTK_COMBO_BOX(configure_widgets.SHOW_BARS), _("Hidden"));
	gtk_combo_box_append_text( GTK_COMBO_BOX(configure_widgets.SHOW_BARS), _("Top"));
	gtk_combo_box_append_text( GTK_COMBO_BOX(configure_widgets.SHOW_BARS), _("Bottom"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 6);
	gtk_box_pack_start(GTK_BOX(hbox), configure_widgets.SHOW_BARS, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 6);
	ui_widget_set_tooltip_text(configure_widgets.SHOW_BARS,
		_("If position is changed, the option require plugin restart."));
	gtk_combo_box_set_active(GTK_COMBO_BOX(configure_widgets.SHOW_BARS), CONFIG_SHOW_BARS);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Show icons"));
	configure_widgets.SHOW_ICONS = gtk_combo_box_new_text();
	gtk_combo_box_append_text( GTK_COMBO_BOX(configure_widgets.SHOW_ICONS), _("None"));
	gtk_combo_box_append_text( GTK_COMBO_BOX(configure_widgets.SHOW_ICONS), _("Base"));
	gtk_combo_box_append_text( GTK_COMBO_BOX(configure_widgets.SHOW_ICONS), _("Content-type"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 6);
	gtk_box_pack_start(GTK_BOX(hbox), configure_widgets.SHOW_ICONS, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 6);
	gtk_combo_box_set_active(GTK_COMBO_BOX(configure_widgets.SHOW_ICONS), CONFIG_SHOW_ICONS);

	configure_widgets.SHOW_HIDDEN_FILES = gtk_check_button_new_with_label(_("Show hidden files"));
	gtk_button_set_focus_on_click(GTK_BUTTON(configure_widgets.SHOW_HIDDEN_FILES), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(configure_widgets.SHOW_HIDDEN_FILES), CONFIG_SHOW_HIDDEN_FILES);
	gtk_box_pack_start(GTK_BOX(vbox), configure_widgets.SHOW_HIDDEN_FILES, FALSE, FALSE, 0);
	ui_widget_set_tooltip_text(configure_widgets.SHOW_HIDDEN_FILES,
		_("On Windows, this just hide files that are prefixed with '.' (dot)"));

	configure_widgets.HIDE_OBJECT_FILES = gtk_check_button_new_with_label(_("Hide object files"));
	gtk_button_set_focus_on_click(GTK_BUTTON(configure_widgets.HIDE_OBJECT_FILES), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(configure_widgets.HIDE_OBJECT_FILES), CONFIG_HIDE_OBJECT_FILES);
	gtk_box_pack_start(GTK_BOX(vbox), configure_widgets.HIDE_OBJECT_FILES, FALSE, FALSE, 0);
	ui_widget_set_tooltip_text(configure_widgets.HIDE_OBJECT_FILES,
		_("Don't show generated object files in the file browser, this includes *.o, *.obj. *.so, *.dll, *.a, *.lib"));

	configure_widgets.REVERSE_FILTER = gtk_check_button_new_with_label(_("Reverse filter"));
	gtk_button_set_focus_on_click(GTK_BUTTON(configure_widgets.REVERSE_FILTER), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(configure_widgets.REVERSE_FILTER), CONFIG_REVERSE_FILTER);
	gtk_box_pack_start(GTK_BOX(vbox), configure_widgets.REVERSE_FILTER, FALSE, FALSE, 0);

	configure_widgets.FOLLOW_CURRENT_DOC = gtk_check_button_new_with_label(_("Follow current document"));
	gtk_button_set_focus_on_click(GTK_BUTTON(configure_widgets.FOLLOW_CURRENT_DOC), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(configure_widgets.FOLLOW_CURRENT_DOC), CONFIG_FOLLOW_CURRENT_DOC);
	gtk_box_pack_start(GTK_BOX(vbox), configure_widgets.FOLLOW_CURRENT_DOC, FALSE, FALSE, 0);

	configure_widgets.ONE_CLICK_CHDOC = gtk_check_button_new_with_label(_("Single click, open document and focus it"));
	gtk_button_set_focus_on_click(GTK_BUTTON(configure_widgets.ONE_CLICK_CHDOC), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(configure_widgets.ONE_CLICK_CHDOC), CONFIG_ONE_CLICK_CHDOC);
	gtk_box_pack_start(GTK_BOX(vbox), configure_widgets.ONE_CLICK_CHDOC, FALSE, FALSE, 0);

	configure_widgets.CHROOT_ON_DCLICK = gtk_check_button_new_with_label(_("Double click open directory"));
	gtk_button_set_focus_on_click(GTK_BUTTON(configure_widgets.CHROOT_ON_DCLICK), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(configure_widgets.CHROOT_ON_DCLICK), CONFIG_CHROOT_ON_DCLICK);
	gtk_box_pack_start(GTK_BOX(vbox), configure_widgets.CHROOT_ON_DCLICK, FALSE, FALSE, 0);

	configure_widgets.ON_DELETE_CLOSE_FILE = gtk_check_button_new_with_label(_("On delete file, close it if is opened"));
	gtk_button_set_focus_on_click(GTK_BUTTON(configure_widgets.ON_DELETE_CLOSE_FILE), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(configure_widgets.ON_DELETE_CLOSE_FILE), CONFIG_ON_DELETE_CLOSE_FILE);
	gtk_box_pack_start(GTK_BOX(vbox), configure_widgets.ON_DELETE_CLOSE_FILE, FALSE, FALSE, 0);

	configure_widgets.SHOW_TREE_LINES = gtk_check_button_new_with_label(_("Show tree lines"));
	gtk_button_set_focus_on_click(GTK_BUTTON(configure_widgets.SHOW_TREE_LINES), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(configure_widgets.SHOW_TREE_LINES), CONFIG_SHOW_TREE_LINES);
#if GTK_CHECK_VERSION(2, 10, 0)
	gtk_box_pack_start(GTK_BOX(vbox), configure_widgets.SHOW_TREE_LINES, FALSE, FALSE, 0);
#endif

	configure_widgets.SHOW_BOOKMARKS = gtk_check_button_new_with_label(_("Show bookmarks"));
	gtk_button_set_focus_on_click(GTK_BUTTON(configure_widgets.SHOW_BOOKMARKS), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(configure_widgets.SHOW_BOOKMARKS), CONFIG_SHOW_BOOKMARKS);
	gtk_box_pack_start(GTK_BOX(vbox), configure_widgets.SHOW_BOOKMARKS, FALSE, FALSE, 0);

	gtk_widget_show_all(vbox);

	g_signal_connect(dialog, "response", G_CALLBACK(on_configure_response), NULL);

	return vbox;
}


/* ------------------
 * GEANY HOOKS
 * ------------------ */

static void
project_change_cb(G_GNUC_UNUSED GObject *obj, G_GNUC_UNUSED GKeyFile *config, G_GNUC_UNUSED gpointer data)
{
	gchar *uri;

	uri = get_default_dir();
	treebrowser_chroot(uri);
	g_free(uri);
}

static void kb_activate(guint key_id)
{
	gtk_notebook_set_current_page(GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook), page_number);
	switch (key_id)
	{
		case KB_FOCUS_FILE_LIST:
			gtk_widget_grab_focus(treeview);
			break;

		case KB_FOCUS_PATH_ENTRY:
			gtk_widget_grab_focus(addressbar);
			break;

		case KB_RENAME_OBJECT:
			treebrowser_rename_current();
			break;

		case KB_REFRESH:
			on_menu_refresh(NULL, NULL);
			break;
	}
}

void
plugin_init(GeanyData *data)
{
	GeanyKeyGroup *key_group;

	CONFIG_FILE = g_strconcat(geany->app->configdir, G_DIR_SEPARATOR_S, "plugins", G_DIR_SEPARATOR_S,
		"treebrowser", G_DIR_SEPARATOR_S, "treebrowser.conf", NULL);

	flag_on_expand_refresh = FALSE;

	load_settings();
	create_sidebar();
	treebrowser_chroot(get_default_dir());

	/* setup keybindings */
	key_group = plugin_set_key_group(geany_plugin, "file_browser", KB_COUNT, NULL);

	keybindings_set_item(key_group, KB_FOCUS_FILE_LIST, kb_activate,
		0, 0, "focus_file_list", _("Focus File List"), NULL);
	keybindings_set_item(key_group, KB_FOCUS_PATH_ENTRY, kb_activate,
		0, 0, "focus_path_entry", _("Focus Path Entry"), NULL);
	keybindings_set_item(key_group, KB_RENAME_OBJECT, kb_activate,
		0, 0, "rename_object", _("Rename Object"), NULL);
	keybindings_set_item(key_group, KB_REFRESH, kb_activate,
		0, 0, "rename_refresh", _("Refresh"), NULL);

	plugin_signal_connect(geany_plugin, NULL, "document-activate", TRUE,
		(GCallback)&treebrowser_track_current_cb, NULL);
}

void
plugin_cleanup()
{
	g_free(addressbar_last_address);
	g_free(CONFIG_FILE);
	g_free(CONFIG_OPEN_EXTERNAL_CMD);
	gtk_widget_destroy(sidebar_vbox);
}
