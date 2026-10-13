/*
 *      treebrowser.c - v0.20
 *
 *      Copyright 2010 Adrian Dimitrov <dimitrov.adrian@gmail.com>
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gdk/gdkkeysyms.h>

#include "geany.h"
#include <geanyplugin.h>
#include <gp_gtkcompat.h>

#include <gio/gio.h>

#ifdef G_OS_WIN32
# include <windows.h>
#endif

/* These items are set by Geany before plugin_init() is called. */
GeanyPlugin 				*geany_plugin;
GeanyData 					*geany_data;

static gint 				page_number 				= 0;
static GtkTreeStore 		*treestore;
static GtkWidget 			*treeview;
static GtkWidget 			*sidebar_vbox;
static GtkWidget 			*sidebar_vbox_bars;
static GtkWidget 			*filter;
static GtkWidget 			*addressbar;
static GtkWidget 			*toolbar_tools_button;
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

#ifndef G_OS_WIN32
# define CONFIG_OPEN_EXTERNAL_CMD_DEFAULT "xdg-open '%d'"
# define CONFIG_OPEN_TERMINAL_DEFAULT "xterm"
#else
# define CONFIG_OPEN_EXTERNAL_CMD_DEFAULT "explorer %d"
# define CONFIG_OPEN_TERMINAL_DEFAULT "cmd"
#endif

static gchar 				*CONFIG_FILE 				= NULL;
static gchar 				*CONFIG_OPEN_EXTERNAL_CMD 	= NULL;
static gchar 				*CONFIG_OPEN_TERMINAL 	= NULL;
static gboolean 			CONFIG_REVERSE_FILTER 		= FALSE;
static gboolean 			CONFIG_ONE_CLICK_CHDOC 		= FALSE;
static gboolean 			CONFIG_SHOW_HIDDEN_FILES 	= FALSE;
static gboolean 			CONFIG_HIDE_OBJECT_FILES 	= FALSE;
static gint 				CONFIG_SHOW_BARS			= 1;
static gboolean 			CONFIG_CHROOT_ON_DCLICK		= FALSE;
static gboolean 			CONFIG_FOLLOW_CURRENT_DOC 	= TRUE;
static gboolean 			CONFIG_ON_DELETE_CLOSE_FILE = TRUE;
static gboolean 			CONFIG_ON_OPEN_FOCUS_EDITOR = FALSE;
static gboolean 			CONFIG_SHOW_TREE_LINES 		= TRUE;
static gboolean 			CONFIG_SHOW_BOOKMARKS 		= FALSE;
static gint 				CONFIG_SHOW_ICONS 			= 2;
static gboolean				CONFIG_OPEN_NEW_FILES 		= TRUE;

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
	KB_CREATE_FILE,
	KB_CREATE_DIR,
	KB_REFRESH,
	KB_TRACK_CURRENT,
	KB_COUNT
};


/* ------------------
 * PLUGIN INFO
 * ------------------ */

PLUGIN_VERSION_CHECK(224)

PLUGIN_SET_TRANSLATABLE_INFO(
	LOCALEDIR,
	GETTEXT_PACKAGE,
	_("TreeBrowser"),
	_("This plugin adds a tree browser to Geany, allowing the user to "
	  "browse files using a tree view of the directory being browsed."),
	"1.32" ,
	"Adrian Dimitrov (dimitrov.adrian@gmail.com)")


/* ------------------
 * PREDEFINES
 * ------------------ */

#define foreach_slist_free(node, list) for (node = list, list = NULL; g_slist_free_1(list), node != NULL; list = node, node = node->next)


/* ------------------
 * PROTOTYPES
 * ------------------ */

static void 	project_open_cb(G_GNUC_UNUSED GObject *obj, G_GNUC_UNUSED GKeyFile *config, G_GNUC_UNUSED gpointer data);
static void 	treebrowser_browse(gchar *directory, gpointer parent);
static void 	treebrowser_bookmarks_set_state(void);
static void 	treebrowser_load_bookmarks(void);
static void 	treebrowser_tree_store_iter_clear_nodes(gpointer iter, gboolean delete_root);
static void 	treebrowser_rename_current(void);
static void 	on_menu_create_new_object(GtkMenuItem *menuitem, const gchar *type);
static void 	on_menu_copy_relative_uri(GtkMenuItem *menuitem, gchar *uri);
static void 	load_settings(void);
static gboolean save_settings(void);


/* ------------------
 * PLUGIN CALLBACKS
 * ------------------ */

PluginCallback plugin_callbacks[] =
{
	{ "project-open", (GCallback) &project_open_cb, TRUE, NULL },
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

#if GTK_CHECK_VERSION(3, 10, 0)
static GdkPixbuf *
utils_pixbuf_from_name(const gchar *icon_name)
{
	GError *error = NULL;
	GtkIconTheme *icon_theme;
	GdkPixbuf *pixbuf;

	icon_theme = gtk_icon_theme_get_default ();
	pixbuf = gtk_icon_theme_load_icon(icon_theme, icon_name, 16, 0, &error);

	if (!pixbuf)
	{
		g_warning("Couldn't load icon: %s", error->message);
		g_error_free(error);
		return NULL;
	}

	return pixbuf;
}
#else
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
#endif

static GdkPixbuf *
utils_pixbuf_from_path(gchar *path)
{
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
		const GtkIconLookupFlags flags = GTK_ICON_LOOKUP_USE_BUILTIN | GTK_ICON_LOOKUP_FORCE_SIZE;

		gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &width, NULL);
		info = gtk_icon_theme_lookup_by_gicon(gtk_icon_theme_get_default(), icon, width, flags);
		g_object_unref(icon);
		if (!info)
		{
			icon = g_themed_icon_new("text-x-generic");
			if (icon != NULL)
			{
				info = gtk_icon_theme_lookup_by_gicon(gtk_icon_theme_get_default(), icon, width, flags);
				g_object_unref(icon);
			}
		}
		if (!info)
			return NULL;
		ret = gtk_icon_info_load_icon (info, NULL);
		gtk_icon_info_free(info);
	}
	return ret;
}


/* result must be freed */
static gchar*
path_is_in_dir(gchar* src, gchar* find)
{
	guint i = 0;

	gchar *diffed_path = NULL, *tmp = NULL;
	gchar **src_segments = NULL, **find_segments = NULL;
	guint src_segments_n = 0, find_segments_n = 0, n = 0;

	src_segments = g_strsplit(src, G_DIR_SEPARATOR_S, 0);
	find_segments = g_strsplit(find, G_DIR_SEPARATOR_S, 0);

	src_segments_n = g_strv_length(src_segments);
	find_segments_n = g_strv_length(find_segments);

	n = src_segments_n;
	if (find_segments_n < n)
		n = find_segments_n;

	for(i = 1; i<n; i++)
		if (utils_str_equal(find_segments[i], src_segments[i]) != TRUE)
			break;
		else
		{
			tmp = g_strconcat(diffed_path == NULL ? "" : diffed_path,
								G_DIR_SEPARATOR_S, find_segments[i], NULL);
			g_free(diffed_path);
			diffed_path = tmp;
		}

	g_strfreev(src_segments);
	g_strfreev(find_segments);

	return diffed_path;
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

	if (EMPTY(gtk_entry_get_text(GTK_ENTRY(filter))))
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
	gsize len;
	gchar *base_name = NULL;
	base_name = g_path_get_basename(filename);

	if (EMPTY(base_name))
	{
		g_free(base_name);
		return FALSE;
	}

	if (CONFIG_SHOW_HIDDEN_FILES)
	{
		g_free(base_name);
		return FALSE;
	}

#ifdef G_OS_WIN32
	if (win32_check_hidden(filename))
	{
		g_free(base_name);
		return TRUE;
	}
#else
	if (base_name[0] == '.')
	{
		if (g_strcmp0(base_name, ".claude") == 0 ||
		    g_strcmp0(base_name, ".github") == 0 ||
			g_strcmp0(base_name, ".codex") ==  0 ||
		    g_strcmp0(base_name, ".copilot") == 0 ||
		    g_strcmp0(base_name, ".azuredevops") == 0 ||
		    g_strcmp0(base_name, ".requirements") == 0 ||
		    g_strcmp0(base_name, ".planning") == 0 ||
		    g_strcmp0(base_name, ".process") == 0 )
		{
			g_free(base_name);
			return FALSE;
		}
		g_free(base_name);
		return TRUE;
	}
#endif

	len = strlen(base_name);
	if (base_name[len - 1] == '~')
	{
		g_free(base_name);
		return TRUE;
	}

	g_free(base_name);
	return FALSE;
}

static gchar*
get_default_dir(void)
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

	if (! EMPTY(dir))
		return utils_get_locale_from_utf8(dir);

	return g_get_current_dir();
}

static gboolean
treebrowser_checkdir(gchar *directory)
{
	gboolean is_dir;
	static gboolean old_value = TRUE;
#if !GTK_CHECK_VERSION(3, 0, 0)
	static const GdkColor red 	= {0, 0xffff, 0x6666, 0x6666};
	static const GdkColor white = {0, 0xffff, 0xffff, 0xffff};
#endif

	is_dir = g_file_test(directory, G_FILE_TEST_IS_DIR);

	if (old_value != is_dir)
	{
#if GTK_CHECK_VERSION(3, 0, 0)
		GtkStyleContext *context;
		context = gtk_widget_get_style_context(GTK_WIDGET(addressbar));
		if (is_dir)
		{
			gtk_style_context_remove_class(context, "invalid");
		}
		else
		{
			gtk_style_context_add_class(context, "invalid");
		}
#else
		gtk_widget_modify_base(GTK_WIDGET(addressbar), GTK_STATE_NORMAL, is_dir ? NULL : &red);
		gtk_widget_modify_text(GTK_WIDGET(addressbar), GTK_STATE_NORMAL, is_dir ? NULL : &white);
#endif
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
treebrowser_chroot(const gchar *dir)
{
	gchar *directory;

	if (g_str_has_suffix(dir, G_DIR_SEPARATOR_S))
		directory = g_strndup(dir, strlen(dir)-1);
	else
		directory = g_strdup(dir);

	gtk_entry_set_text(GTK_ENTRY(addressbar), directory);

	if (!directory || strlen(directory) == 0)
		SETPTR(directory, g_strdup(G_DIR_SEPARATOR_S));

	if (! treebrowser_checkdir(directory))
	{
		g_free(directory);
		return;
	}

	treebrowser_bookmarks_set_state();

	SETPTR(addressbar_last_address, directory);

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

	if (parent)
		treebrowser_tree_store_iter_clear_nodes(parent, FALSE);
	else
		gtk_tree_store_clear(treestore);

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
#if GTK_CHECK_VERSION(3, 10, 0)
					icon = CONFIG_SHOW_ICONS ? utils_pixbuf_from_name("folder") : NULL;
#else
					icon = CONFIG_SHOW_ICONS ? utils_pixbuf_from_stock(GTK_STOCK_DIRECTORY) : NULL;
#endif
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
#if GTK_CHECK_VERSION(3, 10, 0)
										? utils_pixbuf_from_name("text-x-generic")
#else
										? utils_pixbuf_from_stock(GTK_STOCK_FILE)
#endif
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

	if (!has_parent)
	{
		/* Prepend a "." node so the root directory is right-clickable (New File/Folder etc.) */
		GtkTreeIter root_iter;
		GdkPixbuf *root_icon = NULL;
		gchar *root_uri = g_strndup(directory, strlen(directory) - 1);
#if GTK_CHECK_VERSION(3, 10, 0)
		root_icon = CONFIG_SHOW_ICONS ? utils_pixbuf_from_name("folder") : NULL;
#else
		root_icon = CONFIG_SHOW_ICONS ? utils_pixbuf_from_stock(GTK_STOCK_DIRECTORY) : NULL;
#endif
		gtk_tree_store_prepend(treestore, &root_iter, NULL);
		gtk_tree_store_set(treestore, &root_iter,
						TREEBROWSER_COLUMN_ICON,	root_icon,
						TREEBROWSER_COLUMN_NAME,	".",
						TREEBROWSER_COLUMN_URI,		root_uri,
						-1);
		if (root_icon)
			g_object_unref(root_icon);
		g_free(root_uri);
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
treebrowser_bookmarks_set_state(void)
{
	if (gtk_tree_store_iter_is_valid(treestore, &bookmarks_iter))
		bookmarks_expanded = tree_view_row_expanded_iter(GTK_TREE_VIEW(treeview), &bookmarks_iter);
	else
		bookmarks_expanded = FALSE;
}

static void
treebrowser_load_bookmarks(void)
{
	gchar 		*bookmarks;
	gchar 		*contents, *path_full;
	gchar 		**lines, **line;
	GtkTreeIter iter;
	gchar 		*pos;
	GdkPixbuf 	*icon = NULL;

	if (! CONFIG_SHOW_BOOKMARKS)
		return;

	bookmarks = g_build_filename(g_get_home_dir(), ".gtk-bookmarks", NULL);
	if (g_file_get_contents(bookmarks, &contents, NULL, NULL))
	{
		if (gtk_tree_store_iter_is_valid(treestore, &bookmarks_iter))
		{
			bookmarks_expanded = tree_view_row_expanded_iter(GTK_TREE_VIEW(treeview), &bookmarks_iter);
			treebrowser_tree_store_iter_clear_nodes(&bookmarks_iter, FALSE);
		}
		else
		{
			gtk_tree_store_prepend(treestore, &bookmarks_iter, NULL);
#if GTK_CHECK_VERSION(3, 10, 0)
			icon = CONFIG_SHOW_ICONS ? utils_pixbuf_from_name("go-home") : NULL;
#else
			icon = CONFIG_SHOW_ICONS ? utils_pixbuf_from_stock(GTK_STOCK_HOME) : NULL;
#endif
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
				}
			}
			path_full = g_filename_from_uri(*line, NULL, NULL);
			if (path_full != NULL)
			{
				if (g_file_test(path_full, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))
				{
					gchar *file_name = g_path_get_basename(path_full);

					gtk_tree_store_append(treestore, &iter, &bookmarks_iter);
#if GTK_CHECK_VERSION(3, 10, 0)
					icon = CONFIG_SHOW_ICONS ? utils_pixbuf_from_name("folder") : NULL;
#else
					icon = CONFIG_SHOW_ICONS ? utils_pixbuf_from_stock(GTK_STOCK_DIRECTORY) : NULL;
#endif
					gtk_tree_store_set(treestore, &iter,
												TREEBROWSER_COLUMN_ICON, 	icon,
												TREEBROWSER_COLUMN_NAME, 	file_name,
												TREEBROWSER_COLUMN_URI, 	path_full,
												-1);
					g_free(file_name);
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
	g_free(bookmarks);
}

static gboolean
treebrowser_search(gchar *uri, gpointer parent)
{
	GtkTreeIter 	iter;
	GtkTreePath 	*path;
	gchar 			*uri_current;

	if (gtk_tree_model_iter_children(GTK_TREE_MODEL(treestore), &iter, parent))
	{
		do
		{
			if (gtk_tree_model_iter_has_child(GTK_TREE_MODEL(treestore), &iter))
			{
				gchar *dir_uri = NULL;
				gtk_tree_model_get(GTK_TREE_MODEL(treestore), &iter,
				                   TREEBROWSER_COLUMN_URI, &dir_uri, -1);

				/* Lazy-load: if this unloaded dir is an ancestor of the target URI,
				 * expand it now so its real children are populated from disk. */
				if (dir_uri &&
				    g_str_has_prefix(uri, dir_uri) &&
				    uri[strlen(dir_uri)] == G_DIR_SEPARATOR)
				{
					GtkTreeIter child;
					if (gtk_tree_model_iter_children(GTK_TREE_MODEL(treestore), &child, &iter))
					{
						gchar *child_uri = NULL;
						gtk_tree_model_get(GTK_TREE_MODEL(treestore), &child,
						                   TREEBROWSER_COLUMN_URI, &child_uri, -1);
						GtkTreeIter sib = child;
						if (!child_uri &&
						    !gtk_tree_model_iter_next(GTK_TREE_MODEL(treestore), &sib))
						{
							/* Only the placeholder child — trigger lazy load via expand */
							GtkTreePath *dp = gtk_tree_model_get_path(
							    GTK_TREE_MODEL(treestore), &iter);
							gtk_tree_view_expand_row(GTK_TREE_VIEW(treeview), dp, FALSE);
							gtk_tree_path_free(dp);
						}
						g_free(child_uri);
					}
				}
				g_free(dir_uri);

				if (treebrowser_search(uri, &iter))
					return TRUE;
			}

			gtk_tree_model_get(GTK_TREE_MODEL(treestore), &iter, TREEBROWSER_COLUMN_URI, &uri_current, -1);

			if (utils_str_equal(uri, uri_current) == TRUE)
			{
				path = gtk_tree_model_get_path(GTK_TREE_MODEL(treestore), &iter);
				gtk_tree_view_expand_to_path(GTK_TREE_VIEW(treeview), path);
				gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(treeview), path, NULL, FALSE, 0, 0);
				gtk_tree_view_set_cursor(GTK_TREE_VIEW(treeview), path, treeview_column_text, FALSE);
				gtk_tree_path_free(path);
				g_free(uri_current);
				return TRUE;
			}
			else
				g_free(uri_current);

		} while (gtk_tree_model_iter_next(GTK_TREE_MODEL(treestore), &iter));
	}

	return FALSE;
}

static void
treebrowser_focus_first_child(GtkTreeIter *parent_iter)
{
	GtkTreeIter child;
	if (gtk_tree_model_iter_children(GTK_TREE_MODEL(treestore), &child, parent_iter))
	{
		gchar *child_uri = NULL;
		gtk_tree_model_get(GTK_TREE_MODEL(treestore), &child,
		                   TREEBROWSER_COLUMN_URI, &child_uri, -1);
		if (child_uri)
		{
			GtkTreePath *cp = gtk_tree_model_get_path(GTK_TREE_MODEL(treestore), &child);
			gtk_tree_view_set_cursor(GTK_TREE_VIEW(treeview), cp, NULL, FALSE);
			gtk_tree_path_free(cp);
		}
		g_free(child_uri);
	}
}

static void
fs_remove(gchar *root, gboolean delete_root)
{
	gchar *path;
	const gchar *name;

	if (! g_file_test(root, G_FILE_TEST_EXISTS))
		return;

	if (g_file_test(root, G_FILE_TEST_IS_DIR))
	{
		GDir *dir;
		dir = g_dir_open (root, 0, NULL);

		if (!dir)
		{
			if (delete_root)
				g_remove(root);
			return;
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
		g_dir_close(dir);
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
treebrowser_tree_store_iter_clear_nodes(gpointer iter, gboolean delete_root)
{
	GtkTreeIter i;

	if (gtk_tree_model_iter_children(GTK_TREE_MODEL(treestore), &i, iter))
	{
		while (gtk_tree_store_remove(GTK_TREE_STORE(treestore), &i))
			/* do nothing */;
	}
	if (delete_root)
		gtk_tree_store_remove(GTK_TREE_STORE(treestore), iter);
}

static gboolean
treebrowser_expand_to_path(gchar* root, gchar* find)
{
	guint i = 0;
	gboolean founded = FALSE, global_founded = FALSE;
	gchar *new = NULL;
	gchar **root_segments = NULL, **find_segments = NULL;
	guint find_segments_n = 0;

	root_segments = g_strsplit(root, G_DIR_SEPARATOR_S, 0);
	find_segments = g_strsplit(find, G_DIR_SEPARATOR_S, 0);

	find_segments_n = g_strv_length(find_segments)-1;


	for (i = 1; i<=find_segments_n; i++)
	{
		new = g_strconcat(new ? new : "", G_DIR_SEPARATOR_S, find_segments[i], NULL);

		if (founded)
		{
			if (treebrowser_search(new, NULL))
				global_founded = TRUE;
		}
		else
			if (utils_str_equal(root, new) == TRUE)
				founded = TRUE;
	}

	g_free(new);
	g_strfreev(root_segments);
	g_strfreev(find_segments);

	return global_founded;
}

static gboolean
treebrowser_track_current(void)
{

	GeanyDocument	*doc 		= document_get_current();
	gchar 			*path_current;
	gchar			**path_segments = NULL;
	gchar 			*froot = NULL;

	if (doc != NULL && doc->file_name != NULL && g_path_is_absolute(doc->file_name))
	{
		path_current = utils_get_locale_from_utf8(doc->file_name);

		/*
		 * Checking if the document is in the expanded or collapsed files
		 */
		if (! treebrowser_search(path_current, NULL))
		{
			/*
			 * Else we have to chroting to the document`s nearles path
			 */

			froot = path_is_in_dir(addressbar_last_address, g_path_get_dirname(path_current));

			if (froot == NULL)
				froot = g_strdup(G_DIR_SEPARATOR_S);

			if (utils_str_equal(froot, addressbar_last_address) != TRUE)
				treebrowser_chroot(froot);

			treebrowser_expand_to_path(froot, path_current);
		}

		gtk_widget_grab_focus(treeview);

		g_strfreev(path_segments);
		g_free(froot);
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
			renderers 	= gtk_cell_layout_get_cells(GTK_CELL_LAYOUT(column));
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
treebrowser_rename_current(void)
{
	GtkTreeSelection 	*selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	GtkTreeIter 		iter;
	GtkTreeModel 		*model;

	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		treebrowser_iter_rename(&iter);
	}
}

static void
treebrowser_create_new_current(const gchar *type)
{
	on_menu_create_new_object(NULL, type);
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
on_menu_open_externally(GtkMenuItem *menuitem, const gchar *uri)
{
	gchar 				*cmd, *locale_cmd, *dir, *c;
	GString 			*cmd_str 	= g_string_new(CONFIG_OPEN_EXTERNAL_CMD);
	GError 				*error 		= NULL;

	dir = g_file_test(uri, G_FILE_TEST_IS_DIR) ? g_strdup(uri) : g_path_get_dirname(uri);

	utils_string_replace_all(cmd_str, "%f", uri);
	utils_string_replace_all(cmd_str, "%d", dir);

	cmd = g_string_free(cmd_str, FALSE);
	locale_cmd = utils_get_locale_from_utf8(cmd);
	if (! spawn_async(dir, locale_cmd, NULL, NULL, NULL, &error))
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
on_menu_open_terminal(GtkMenuItem *menuitem, const gchar *uri)
{
	gchar *cwd;
	if (g_file_test(uri, G_FILE_TEST_EXISTS))
		cwd = g_file_test(uri, G_FILE_TEST_IS_DIR) ? g_strdup(uri) : g_path_get_dirname(uri);
	else
		cwd = g_strdup(addressbar_last_address);

	spawn_async(cwd, CONFIG_OPEN_TERMINAL, NULL, NULL, NULL, NULL);
	g_free(cwd);
}

static void
on_menu_set_as_root(GtkMenuItem *menuitem, gchar *uri)
{
	if (g_file_test(uri, G_FILE_TEST_IS_DIR))
		treebrowser_chroot(uri);
}

static void
on_menu_find_in_files(GtkMenuItem *menuitem, gchar *uri)
{
	search_show_find_in_files_dialog(uri);
}

/* Shows a modal dialog asking for a filename. Returns newly allocated string or NULL on cancel. */
static gchar *
ask_for_filename(const gchar *title, const gchar *default_name)
{
	GtkWidget *dialog = gtk_dialog_new_with_buttons(
		title,
		GTK_WINDOW(geany_data->main_widgets->window),
		GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("_Create"), GTK_RESPONSE_OK,
		NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

	GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	gtk_container_set_border_width(GTK_CONTAINER(content), 8);

	GtkWidget *label = gtk_label_new(_("File name:"));
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_box_pack_start(GTK_BOX(content), label, FALSE, FALSE, 2);

	GtkWidget *entry = gtk_entry_new();
	if (default_name && *default_name)
		gtk_entry_set_text(GTK_ENTRY(entry), default_name);
	gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
	gtk_editable_select_region(GTK_EDITABLE(entry), 0, -1);
	gtk_box_pack_start(GTK_BOX(content), entry, FALSE, FALSE, 2);
	gtk_widget_show_all(dialog);

	gchar *result = NULL;
	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK)
	{
		const gchar *text = gtk_entry_get_text(GTK_ENTRY(entry));
		if (text && *text)
			result = g_strdup(text);
	}
	gtk_widget_destroy(dialog);
	return result;
}

static void
on_menu_create_new_object(GtkMenuItem *menuitem, const gchar *type)
{
	GtkTreeSelection 	*selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	GtkTreeIter 		iter, iter_parent;
	GtkTreeModel 		*model;
	gchar 				*uri, *uri_new = NULL;
	gboolean 			refresh_root = FALSE;

	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		gtk_tree_model_get(model, &iter, TREEBROWSER_COLUMN_URI, &uri, -1);
		/* If not a directory, find parent directory */
		if (! g_file_test(uri, G_FILE_TEST_IS_DIR))
		{
			if (gtk_tree_model_iter_parent(GTK_TREE_MODEL(treestore), &iter_parent, &iter))
			{
				iter = iter_parent;
				/* Set URI from parent iter */
				g_free(uri);
				gtk_tree_model_get(model, &iter_parent, TREEBROWSER_COLUMN_URI, &uri, -1);
			}
			else
			{
				SETPTR(uri, g_path_get_dirname(uri));
				refresh_root = TRUE;
			}
		}
	}
	else
	{
		refresh_root 	= TRUE;
		uri 			= g_strdup(addressbar_last_address);
	}

	if (utils_str_equal(type, "file"))
	{
		/* Ask for the filename upfront so we can create it with the right name
		 * and open it in Geany without fighting the inline-rename focus. */
		gchar *fname = ask_for_filename(_("New File"), "");
		if (!fname)
		{
			g_free(uri);
			return;
		}
		uri_new = g_build_filename(uri, fname, NULL);
		g_free(fname);

		if (g_file_test(uri_new, G_FILE_TEST_EXISTS) &&
			!dialogs_show_question(_("Target file '%s' exists.\nDo you really want to replace it with an empty file?"), uri_new))
		{
			g_free(uri_new);
			g_free(uri);
			return;
		}

		gint fd = g_creat(uri_new, 0644);
		if (fd != -1)
		{
			close(fd);
			treebrowser_browse(uri, refresh_root ? NULL : &iter);
			if (CONFIG_OPEN_NEW_FILES)
				document_open_file(uri_new, FALSE, NULL, NULL);
		}
		g_free(uri_new);
	}
	else if (utils_str_equal(type, "directory"))
	{
		uri_new = g_strconcat(uri, G_DIR_SEPARATOR_S, _("NewDirectory"), NULL);

		if (!(g_file_test(uri_new, G_FILE_TEST_EXISTS) &&
			!dialogs_show_question(_("Target file '%s' exists.\nDo you really want to replace it with an empty file?"), uri_new)))
		{
			while (g_file_test(uri_new, G_FILE_TEST_EXISTS))
				SETPTR(uri_new, g_strconcat(uri_new, "_", NULL));

			if (g_mkdir(uri_new, 0755) == 0)
			{
				treebrowser_browse(uri, refresh_root ? NULL : &iter);
				if (treebrowser_search(uri_new, NULL))
					treebrowser_rename_current();
			}
		}
		g_free(uri_new);
	}

	/* cppcheck-suppress doubleFree symbolName=uri */
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
	GtkTreeSelection	*selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	GtkTreeIter		 iter, iter_parent;
	GtkTreeModel		*model;
	gchar			*uri, *uri_parent;
	gboolean		 is_dir, confirmed;

	if (! gtk_tree_selection_get_selected(selection, &model, &iter))
		return;

	gtk_tree_model_get(model, &iter, TREEBROWSER_COLUMN_URI, &uri, -1);

	is_dir = g_file_test(uri, G_FILE_TEST_IS_DIR);

	if (is_dir)
	{
		GDir    *dir      = g_dir_open(uri, 0, NULL);
		gboolean is_empty = dir && g_dir_read_name(dir) == NULL;
		if (dir)
			g_dir_close(dir);

		if (is_empty)
			confirmed = dialogs_show_question(_("Delete empty folder '%s'?"), uri);
		else
			confirmed = dialogs_show_question(
				_("'%s' is not empty.\nDelete the folder and all its contents?"), uri);
	}
	else
		confirmed = dialogs_show_question(_("Do you really want to delete '%s'?"), uri);

	if (confirmed)
	{
		if (CONFIG_ON_DELETE_CLOSE_FILE)
		{
			if (is_dir)
			{
				guint i;
				gsize uri_len = strlen(uri);
				for (i = 0; i < GEANY(documents_array)->len; i++)
				{
					GeanyDocument *doc = documents[i];
					if (doc->is_valid && doc->file_name &&
					    strlen(doc->file_name) > uri_len &&
					    strncmp(uri, doc->file_name, uri_len) == 0 &&
					    doc->file_name[uri_len] == G_DIR_SEPARATOR)
						document_close(doc);
				}
			}
			else
				document_close(document_find_by_filename(uri));
		}

		uri_parent = g_path_get_dirname(uri);
		fs_remove(uri, TRUE);
		if (gtk_tree_model_iter_parent(GTK_TREE_MODEL(treestore), &iter_parent, &iter))
			treebrowser_browse(uri_parent, &iter_parent);
		else
			treebrowser_browse(uri_parent, NULL);
		g_free(uri_parent);
	}
	g_free(uri);
}

static void
on_menu_refresh(GtkMenuItem *menuitem, gpointer *user_data)
{
	GtkTreeSelection 	*selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	GtkTreeIter 		iter, parent_iter;
	GtkTreeIter 		*target_iter = NULL;
	GtkTreeModel 		*model;
	gchar 				*uri;

	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		gtk_tree_model_get(model, &iter, TREEBROWSER_COLUMN_URI, &uri, -1);
		/* if a directory is selected, use it directly for treebrowser_browse()
		 * if a file within a directory is selected, use its parent for treebrowser_browse()
		 * if a file on the top level is selected, use *no* parent for treebrowser_browse() */
		if (g_file_test(uri, G_FILE_TEST_IS_DIR))
			target_iter = &iter;
		else
		{
			if (gtk_tree_model_iter_parent(model, &parent_iter, &iter))
			{
				g_free(uri);
				gtk_tree_model_get(model, &iter, TREEBROWSER_COLUMN_URI, &uri, -1);
				target_iter = &parent_iter;
			}
			else
			{
				SETPTR(uri, g_path_get_dirname(uri));
				target_iter = NULL;
			}
		}
		treebrowser_browse(uri, target_iter);
		if (target_iter == &iter)
		{
			GtkTreePath *tp = gtk_tree_model_get_path(GTK_TREE_MODEL(treestore), &iter);
			gtk_tree_view_expand_row(GTK_TREE_VIEW(treeview), tp, FALSE);
			gtk_tree_path_free(tp);
			treebrowser_focus_first_child(&iter);
		}
		/* cppcheck-suppress doubleFree symbolName=uri */
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
	guint i;
	size_t uri_len = strlen(uri);
	for (i=0; i < GEANY(documents_array)->len; i++)
	{
		if (documents[i]->is_valid)
		{
			/* the document filename should always be longer than the uri when closing children
			 * Compare the beginning of the filename string to see if it matchs the uri*/
			if (strlen(documents[i]->file_name) > uri_len)
			{
				if (strncmp(uri, documents[i]->file_name, uri_len)==0)
					document_close(documents[i]);
			}
		}
	}
}

static void
on_menu_copy_uri(GtkMenuItem *menuitem, gchar *uri)
{
	gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), uri, -1);
	gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_PRIMARY), uri, -1);
}

static void
on_menu_copy_relative_uri(GtkMenuItem *menuitem, gchar *uri)
{
	const gchar *root = addressbar_last_address;
	const gchar *relative = uri;

	if (root && g_str_has_prefix(uri, root))
	{
		relative = uri + strlen(root);
		if (*relative == G_DIR_SEPARATOR)
			relative++;
	}
	gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), relative, -1);
	gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_PRIMARY), relative, -1);
}

static void
on_menu_copy_name(GtkMenuItem *menuitem, gchar *uri)
{
	gchar *name = g_path_get_basename(uri);
	gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), name, -1);
	gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_PRIMARY), name, -1);
	g_free(name);
}

/* Returns the interpreter to use for a script file (e.g. "bash", "python3").
 * Reads the shebang first; falls back to file extension. Caller frees result. */
static gchar *
get_script_interpreter(const gchar *uri)
{
	FILE *fp = fopen(uri, "r");
	if (fp)
	{
		char buf[256] = {0};
		if (fgets(buf, sizeof(buf), fp) && strncmp(buf, "#!", 2) == 0)
		{
			fclose(fp);
			gchar *interp = g_strstrip(g_strdup(buf + 2));
			/* /usr/bin/env PROG → PROG */
			const gchar *env_marker = "/usr/bin/env ";
			if (g_str_has_prefix(interp, env_marker))
			{
				gchar *prog = g_strdup(interp + strlen(env_marker));
				g_free(interp);
				/* strip any trailing args */
				gchar *space = strchr(prog, ' ');
				if (space) *space = '\0';
				return prog;
			}
			/* strip args from bare path (/bin/bash -x → /bin/bash) */
			gchar *space = strchr(interp, ' ');
			if (space) *space = '\0';
			return interp;
		}
		fclose(fp);
	}

	/* extension fallback */
	const gchar *ext = strrchr(uri, '.');
	if (ext)
	{
		if (g_strcmp0(ext, ".sh") == 0 || g_strcmp0(ext, ".bash") == 0)
			return g_strdup("bash");
		if (g_strcmp0(ext, ".py") == 0)
			return g_strdup("python3");
		if (g_strcmp0(ext, ".rb") == 0)
			return g_strdup("ruby");
		if (g_strcmp0(ext, ".pl") == 0)
			return g_strdup("perl");
		if (g_strcmp0(ext, ".js") == 0)
			return g_strdup("node");
	}

	return NULL;
}

static void
on_menu_execute_in_terminal(GtkMenuItem *menuitem, gchar *uri)
{
	GType obj_type = G_OBJECT_TYPE(geany->object);

	/* 1. geanycli file-type-aware: looks up filetypetools.conf for this file */
	if (g_signal_lookup("geanycli-run-file", obj_type))
	{
		g_signal_emit_by_name(geany->object, "geanycli-run-file", uri, FALSE);
		return;
	}

	/* 2. Detect interpreter from shebang / extension for the generic path */
	gchar *interp     = get_script_interpreter(uri);
	gchar *cmd_to_run = interp
		? g_strconcat(interp, " ", uri, NULL)
		: g_strdup(uri);
	g_free(interp);

	/* 3. geanycli generic: run an arbitrary command in the terminal tab */
	if (g_signal_lookup("geanycli-run-command", obj_type))
	{
		g_signal_emit_by_name(geany->object, "geanycli-run-command", cmd_to_run, FALSE);
		g_free(cmd_to_run);
		return;
	}

	/* 4. Legacy geanyterminal */
	if (g_signal_lookup("geanyterminal-run-command", obj_type))
	{
		gchar *quoted = g_shell_quote(cmd_to_run);
		g_signal_emit_by_name(geany->object, "geanyterminal-run-command", quoted, TRUE);
		g_free(quoted);
		g_free(cmd_to_run);
		return;
	}

	/* 5. Last resort: spawn external terminal */
	gchar  *dir    = g_path_get_dirname(uri);
	gchar  *quoted = g_shell_quote(cmd_to_run);
	gchar  *cmd    = g_strconcat(CONFIG_OPEN_TERMINAL, " -e ", quoted, NULL);
	GError *error  = NULL;

	if (!spawn_async(dir, cmd, NULL, NULL, NULL, &error))
	{
		ui_set_statusbar(TRUE, _("Could not execute '%s' (%s)."), uri, error->message);
		g_error_free(error);
	}
	g_free(quoted);
	g_free(cmd);
	g_free(cmd_to_run);
	g_free(dir);
}

static void
on_menu_run_file(GtkMenuItem *menuitem, gchar *uri)
{
	gchar  *dir    = g_path_get_dirname(uri);
	gchar  *interp = get_script_interpreter(uri);
	gchar  *cmd    = interp ? g_strconcat(interp, " ", uri, NULL) : g_strdup(uri);
	GError *error  = NULL;

	if (!spawn_async(dir, cmd, NULL, NULL, NULL, &error))
	{
		ui_set_statusbar(TRUE, _("Could not run '%s' (%s)."), uri, error->message);
		g_error_free(error);
	}
	g_free(cmd);
	g_free(interp);
	g_free(dir);
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

static gboolean
fs_copy_recursive(const gchar *src_path, const gchar *dst_path, GError **error)
{
	if (g_file_test(src_path, G_FILE_TEST_IS_DIR))
	{
		if (g_mkdir_with_parents(dst_path, 0755) != 0)
		{
			g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
			            "Failed to create directory '%s'", dst_path);
			return FALSE;
		}
		GDir *dir = g_dir_open(src_path, 0, error);
		if (!dir)
			return FALSE;
		const gchar *name;
		gboolean ok = TRUE;
		while (ok && (name = g_dir_read_name(dir)) != NULL)
		{
			gchar *child_src = g_build_filename(src_path, name, NULL);
			gchar *child_dst = g_build_filename(dst_path, name, NULL);
			ok = fs_copy_recursive(child_src, child_dst, error);
			g_free(child_src);
			g_free(child_dst);
		}
		g_dir_close(dir);
		return ok;
	}
	else
	{
		GFile *src = g_file_new_for_path(src_path);
		GFile *dst = g_file_new_for_path(dst_path);
		gboolean ok = g_file_copy(src, dst, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, error);
		g_object_unref(src);
		g_object_unref(dst);
		return ok;
	}
}

static void
on_menu_paste_file(G_GNUC_UNUSED GtkMenuItem *menuitem, gchar *target_dir)
{
	GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
	gchar        *clip_text = gtk_clipboard_wait_for_text(clipboard);

	if (!clip_text || !*clip_text)
	{
		ui_set_statusbar(TRUE, _("Clipboard is empty."));
		g_free(clip_text);
		return;
	}
	g_strstrip(clip_text);

	gchar *src_path = g_path_is_absolute(clip_text)
		? g_strdup(clip_text)
		: g_build_filename(addressbar_last_address, clip_text, NULL);
	g_free(clip_text);

	if (!g_file_test(src_path, G_FILE_TEST_EXISTS))
	{
		ui_set_statusbar(TRUE, _("Clipboard does not contain a valid path: %s"), src_path);
		g_free(src_path);
		return;
	}

	gboolean src_is_dir = g_file_test(src_path, G_FILE_TEST_IS_DIR);
	gchar *basename  = g_path_get_basename(src_path);
	gchar *dest_path = g_build_filename(target_dir, basename, NULL);

	if (strcmp(src_path, dest_path) == 0)
	{
		/* Pasting into the same directory — ask for a new name */
		gchar *new_name = ask_for_filename(_("Enter new name for copy"), basename);
		g_free(basename);
		if (!new_name)
		{
			g_free(dest_path);
			g_free(src_path);
			return;
		}
		g_free(dest_path);
		dest_path = g_build_filename(target_dir, new_name, NULL);
		g_free(new_name);
	}
	else
	{
		g_free(basename);
		if (g_file_test(dest_path, G_FILE_TEST_EXISTS))
		{
			if (!dialogs_show_question(
			        src_is_dir
			            ? _("'%s' already exists.\nDo you want to overwrite it?")
			            : _("'%s' already exists.\nDo you want to overwrite it?"),
			        dest_path))
			{
				g_free(dest_path);
				g_free(src_path);
				return;
			}
		}
	}

	GError *error = NULL;
	if (!fs_copy_recursive(src_path, dest_path, &error))
	{
		ui_set_statusbar(TRUE, _("Failed to copy '%s': %s"), src_path, error->message);
		g_error_free(error);
	}
	else
	{
		on_menu_refresh(NULL, NULL);
	}

	g_free(src_path);
	g_free(dest_path);
}

static GtkWidget*
create_popup_menu(const gchar *name, const gchar *uri)
{
	GtkWidget *item, *menu = gtk_menu_new();

	gboolean is_exists 		= g_file_test(uri, G_FILE_TEST_EXISTS);
	gboolean is_dir 		= is_exists ? g_file_test(uri, G_FILE_TEST_IS_DIR) : FALSE;
	gboolean is_document 	= document_find_by_filename(uri) != NULL ? TRUE : FALSE;
	gchar    *interp        = is_exists && !is_dir ? get_script_interpreter(uri) : NULL;
	gboolean is_executable	= is_exists && !is_dir &&
	                          (g_file_test(uri, G_FILE_TEST_IS_EXECUTABLE) || interp != NULL);
	g_free(interp);

#if GTK_CHECK_VERSION(3, 10, 0)
	item = ui_image_menu_item_new("go-up", _("Go _Up"));
#else
	item = ui_image_menu_item_new(GTK_STOCK_GO_UP, _("Go _Up"));
#endif
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_go_up), NULL);

#if GTK_CHECK_VERSION(3, 10, 0)
	item = ui_image_menu_item_new("go-up", _("Set _Path From Document"));
#else
	item = ui_image_menu_item_new(GTK_STOCK_GO_UP, _("Set _Path From Document"));
#endif
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_current_path), NULL);

#if GTK_CHECK_VERSION(3, 10, 0)
	item = ui_image_menu_item_new("document-open", _("_Open Externally"));
#else
	item = ui_image_menu_item_new(GTK_STOCK_OPEN, _("_Open Externally"));
#endif
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect_data(item, "activate", G_CALLBACK(on_menu_open_externally), g_strdup(uri), (GClosureNotify)g_free, 0);
	gtk_widget_set_sensitive(item, is_exists);

	item = ui_image_menu_item_new("utilities-terminal", _("Open _Terminal"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect_data(item, "activate", G_CALLBACK(on_menu_open_terminal), g_strdup(uri), (GClosureNotify)g_free, 0);

	/* Dynamic file/dir tools from filetypetools.conf via geanycli-append-tools signal.
	 * Falls back to the static "Execute Cli" item when geanycli is not loaded or has
	 * no tools configured for this path. */
	{
		GType    obj_type      = G_OBJECT_TYPE(geany->object);
		gboolean showed_tools  = FALSE;

		if (is_exists && g_signal_lookup("geanycli-append-tools", obj_type))
		{
			GtkWidget *submenu = gtk_menu_new();
			g_signal_emit_by_name(geany->object, "geanycli-append-tools",
			                      uri, is_dir, (gpointer)submenu);
			GList *children = gtk_container_get_children(GTK_CONTAINER(submenu));
			if (children)
			{
				item = ui_image_menu_item_new("system-run", _("_File Tools"));
				gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
				gtk_container_add(GTK_CONTAINER(menu), item);
				gtk_widget_show(submenu);
				showed_tools = TRUE;
				g_list_free(children);
			}
			else
				gtk_widget_destroy(submenu);
		}

		if (!showed_tools && is_executable)
		{
			item = ui_image_menu_item_new("utilities-terminal", _("Execute _cli"));
			gtk_container_add(GTK_CONTAINER(menu), item);
			g_signal_connect_data(item, "activate", G_CALLBACK(on_menu_execute_in_terminal),
			                      g_strdup(uri), (GClosureNotify)g_free, 0);
		}
	}

	item = ui_image_menu_item_new("system-run", _("_Run"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect_data(item, "activate", G_CALLBACK(on_menu_run_file), g_strdup(uri), (GClosureNotify)g_free, 0);
	gtk_widget_set_sensitive(item, is_executable);

#if GTK_CHECK_VERSION(3, 10, 0)
	item = ui_image_menu_item_new("go-top", _("Set as _Root"));
#else
	item = ui_image_menu_item_new(GTK_STOCK_GOTO_TOP, _("Set as _Root"));
#endif
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect_data(item, "activate", G_CALLBACK(on_menu_set_as_root), g_strdup(uri), (GClosureNotify)g_free, 0);
	gtk_widget_set_sensitive(item, is_dir);

#if GTK_CHECK_VERSION(3, 10, 0)
	item = ui_image_menu_item_new("view-refresh", _("Refres_h"));
#else
	item = ui_image_menu_item_new(GTK_STOCK_REFRESH, _("Refres_h"));
#endif
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_refresh), NULL);

#if GTK_CHECK_VERSION(3, 10, 0)
	item = ui_image_menu_item_new("edit-find", _("_Find in Files"));
#else
	item = ui_image_menu_item_new(GTK_STOCK_FIND, _("_Find in Files"));
#endif
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect_data(item, "activate", G_CALLBACK(on_menu_find_in_files), g_strdup(uri), (GClosureNotify)g_free, 0);
	gtk_widget_set_sensitive(item, is_dir);

	item = gtk_separator_menu_item_new();
	gtk_container_add(GTK_CONTAINER(menu), item);

#if GTK_CHECK_VERSION(3, 10, 0)
	item = ui_image_menu_item_new("list-add", _("N_ew Folder"));
#else
	item = ui_image_menu_item_new(GTK_STOCK_ADD, _("N_ew Folder"));
#endif
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_create_new_object), (gpointer)"directory");

#if GTK_CHECK_VERSION(3, 10, 0)
	item = ui_image_menu_item_new("document-new", _("_New File"));
#else
	item = ui_image_menu_item_new(GTK_STOCK_NEW, _("_New File"));
#endif
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_create_new_object), (gpointer)"file");

#if GTK_CHECK_VERSION(3, 10, 0)
	item = ui_image_menu_item_new("edit-copy", _("_Copy"));
#else
	item = ui_image_menu_item_new(GTK_STOCK_COPY, _("_Copy"));
#endif
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect_data(item, "activate", G_CALLBACK(on_menu_copy_uri), g_strdup(uri), (GClosureNotify)g_free, 0);
	gtk_widget_set_sensitive(item, is_exists);

	{
		gchar *paste_dir = (!uri || !*uri)
			? g_strdup(addressbar_last_address)
			: is_dir ? g_strdup(uri) : g_path_get_dirname(uri);
#if GTK_CHECK_VERSION(3, 10, 0)
		item = ui_image_menu_item_new("edit-paste", _("_Paste"));
#else
		item = ui_image_menu_item_new(GTK_STOCK_PASTE, _("_Paste"));
#endif
		gtk_container_add(GTK_CONTAINER(menu), item);
		g_signal_connect_data(item, "activate", G_CALLBACK(on_menu_paste_file),
		                      paste_dir, (GClosureNotify)g_free, 0);
	}

#if GTK_CHECK_VERSION(3, 10, 0)
	item = ui_image_menu_item_new("document-save-as", _("Rena_me"));
#else
	item = ui_image_menu_item_new(GTK_STOCK_SAVE_AS, _("Rena_me"));
#endif
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_rename), NULL);
	gtk_widget_set_sensitive(item, is_exists);

#if GTK_CHECK_VERSION(3, 10, 0)
	item = ui_image_menu_item_new("edit-delete", _("_Delete"));
#else
	item = ui_image_menu_item_new(GTK_STOCK_DELETE, _("_Delete"));
#endif
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_delete), NULL);
	gtk_widget_set_sensitive(item, is_exists);

	item = gtk_separator_menu_item_new();
	gtk_container_add(GTK_CONTAINER(menu), item);

#if GTK_CHECK_VERSION(3, 10, 0)
	item = ui_image_menu_item_new("window-close", g_strdup_printf(_("Close: %s"), name));
#else
	item = ui_image_menu_item_new(GTK_STOCK_CLOSE, g_strdup_printf(_("Close: %s"), name));
#endif
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect_data(item, "activate", G_CALLBACK(on_menu_close), g_strdup(uri), (GClosureNotify)g_free, 0);
	gtk_widget_set_sensitive(item, is_document);

#if GTK_CHECK_VERSION(3, 10, 0)
	item = ui_image_menu_item_new("window-close", g_strdup_printf(_("Clo_se Child Documents ")));
#else
	item = ui_image_menu_item_new(GTK_STOCK_CLOSE, g_strdup_printf(_("Clo_se Child Documents ")));
#endif
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect_data(item, "activate", G_CALLBACK(on_menu_close_children), g_strdup(uri), (GClosureNotify)g_free, 0);
	gtk_widget_set_sensitive(item, is_dir);

#if GTK_CHECK_VERSION(3, 10, 0)
	item = ui_image_menu_item_new("edit-copy", _("_Copy Full Path"));
#else
	item = ui_image_menu_item_new(GTK_STOCK_COPY, _("_Copy Full Path"));
#endif
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect_data(item, "activate", G_CALLBACK(on_menu_copy_uri), g_strdup(uri), (GClosureNotify)g_free, 0);
	gtk_widget_set_sensitive(item, is_exists);

#if GTK_CHECK_VERSION(3, 10, 0)
	item = ui_image_menu_item_new("edit-copy", _("Copy _Relative Path"));
#else
	item = ui_image_menu_item_new(GTK_STOCK_COPY, _("Copy _Relative Path"));
#endif
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect_data(item, "activate", G_CALLBACK(on_menu_copy_relative_uri), g_strdup(uri), (GClosureNotify)g_free, 0);
	gtk_widget_set_sensitive(item, is_exists);

#if GTK_CHECK_VERSION(3, 10, 0)
	item = ui_image_menu_item_new("edit-copy", _("Copy File _Name"));
#else
	item = ui_image_menu_item_new(GTK_STOCK_COPY, _("Copy File _Name"));
#endif
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect_data(item, "activate", G_CALLBACK(on_menu_copy_name), g_strdup(uri), (GClosureNotify)g_free, 0);
	gtk_widget_set_sensitive(item, is_exists && !is_dir);

	item = gtk_separator_menu_item_new();
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_widget_show(item);

#if GTK_CHECK_VERSION(3, 10, 0)
	item = ui_image_menu_item_new("go-next", _("E_xpand All"));
#else
	item = ui_image_menu_item_new(GTK_STOCK_GO_FORWARD, _("E_xpand All"));
#endif
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_expand_all), NULL);

#if GTK_CHECK_VERSION(3, 10, 0)
	item = ui_image_menu_item_new("go-previous", _("Coll_apse All"));
#else
	item = ui_image_menu_item_new(GTK_STOCK_GO_BACK, _("Coll_apse All"));
#endif
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_collapse_all), NULL);

	item = gtk_separator_menu_item_new();
	gtk_container_add(GTK_CONTAINER(menu), item);

	item = gtk_check_menu_item_new_with_mnemonic(_("Show Boo_kmarks"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), CONFIG_SHOW_BOOKMARKS);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_show_bookmarks), NULL);

	item = gtk_check_menu_item_new_with_mnemonic(_("Sho_w Hidden Files"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), CONFIG_SHOW_HIDDEN_FILES);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_show_hidden_files), NULL);

	item = gtk_check_menu_item_new_with_mnemonic(_("Show Tool_bars"));
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
on_button_go_up(void)
{
	gchar *uri;

	uri = g_path_get_dirname(addressbar_last_address);
	treebrowser_chroot(uri);
	g_free(uri);
}

static void
on_button_refresh(void)
{
	treebrowser_chroot(addressbar_last_address);
	gtk_widget_grab_focus(treeview);
}

static void
on_button_go_home(void)
{
	gchar *uri;

	uri = g_strdup(g_get_home_dir());
	treebrowser_chroot(uri);
	g_free(uri);
	gtk_widget_grab_focus(treeview);
}

static void
on_button_go_project(void)
{
	GeanyProject *project = geany->app->project;
	if (project && !EMPTY(project->base_path))
	{
		gchar *uri = utils_get_locale_from_utf8(project->base_path);
		treebrowser_chroot(uri);
		g_free(uri);
		gtk_widget_grab_focus(treeview);
	}
}

static void
on_button_track_path(G_GNUC_UNUSED GtkToolButton *button, G_GNUC_UNUSED gpointer data)
{
	GdkEvent *ev = gtk_get_current_event();
	GdkModifierType state = 0;
	if (ev)
	{
		gdk_event_get_state(ev, &state);
		gdk_event_free(ev);
	}

	treebrowser_track_current();

	if (state & GDK_CONTROL_MASK)
	{
		GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
		GtkTreeIter iter;
		GtkTreeModel *model;
		if (gtk_tree_selection_get_selected(sel, &model, &iter))
		{
			gchar *uri = NULL;
			gtk_tree_model_get(model, &iter, TREEBROWSER_COLUMN_URI, &uri, -1);
			if (uri)
			{
				on_menu_copy_relative_uri(NULL, uri);
				g_free(uri);
			}
		}
	}
}

static void
on_button_current_path(void)
{
	gchar *uri;

	uri = get_default_dir();
	treebrowser_chroot(uri);
	g_free(uri);
}

static void
on_button_hide_bars(void)
{
	showbars(FALSE);
}

static void
on_toolbar_tools_show_menu(GtkMenuToolButton *button, gpointer user_data)
{
	GtkWidget *menu     = gtk_menu_new();
	GType      obj_type = G_OBJECT_TYPE(geany->object);
	gboolean   has_tools = FALSE;

	if (addressbar_last_address && g_signal_lookup("geanycli-append-tools", obj_type))
	{
		g_signal_emit_by_name(geany->object, "geanycli-append-tools",
		                      addressbar_last_address, TRUE, (gpointer)menu);
		GList *children = gtk_container_get_children(GTK_CONTAINER(menu));
		if (children)
		{
			has_tools = TRUE;
			g_list_free(children);
		}
	}

	if (!has_tools)
	{
		GtkWidget *item = gtk_menu_item_new_with_label(_("No tools configured for this directory"));
		gtk_widget_set_sensitive(item, FALSE);
		gtk_container_add(GTK_CONTAINER(menu), item);
	}

	gtk_widget_show_all(menu);
	gtk_menu_tool_button_set_menu(button, menu);
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
	if (event->button == 1 && (event->state & GDK_MOD1_MASK))
	{
		GtkTreeIter iter;
		GtkTreeModel *model;
		GtkTreePath *path;
		gchar *uri = NULL;

		if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(treeview),
		                                   (gint) event->x,
		                                   (gint) event->y,
		                                   &path, NULL, NULL, NULL))
		{
			gtk_tree_selection_unselect_all(selection);
			gtk_tree_selection_select_path(selection, path);
			gtk_tree_path_free(path);
		}

		if (gtk_tree_selection_get_selected(selection, &model, &iter))
			gtk_tree_model_get(GTK_TREE_MODEL(treestore), &iter,
			                   TREEBROWSER_COLUMN_URI, &uri, -1);

		if (uri)
		{
			on_menu_copy_uri(NULL, uri);
			g_free(uri);
		}
		return TRUE;
	}

	if (event->button == 1 && (event->state & GDK_CONTROL_MASK))
	{
		GtkTreeIter iter;
		GtkTreeModel *model;
		GtkTreePath *path;
		gchar *uri = NULL;

		if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(treeview),
		                                   (gint) event->x,
		                                   (gint) event->y,
		                                   &path, NULL, NULL, NULL))
		{
			gtk_tree_selection_unselect_all(selection);
			gtk_tree_selection_select_path(selection, path);
			gtk_tree_path_free(path);
		}

		if (gtk_tree_selection_get_selected(selection, &model, &iter))
			gtk_tree_model_get(GTK_TREE_MODEL(treestore), &iter,
			                   TREEBROWSER_COLUMN_URI, &uri, -1);

		if (uri)
		{
			on_menu_copy_relative_uri(NULL, uri);
			g_free(uri);
		}
		return TRUE;
	}

	if (event->button == 3)
	{
		GtkTreeIter 	iter;
		GtkTreeModel 	*model;
		GtkTreePath *path;
		GtkWidget *menu;
		gchar *name = NULL, *uri = NULL;

		/* Get tree path for row that was clicked */
		if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(treeview),
																		 (gint) event->x,
																		 (gint) event->y,
																		 &path, NULL, NULL, NULL))
		{
			/* Unselect current selection; select clicked row from path */
			gtk_tree_selection_unselect_all(selection);
			gtk_tree_selection_select_path(selection, path);
			gtk_tree_path_free(path);
		}

		if (gtk_tree_selection_get_selected(selection, &model, &iter))
			gtk_tree_model_get(GTK_TREE_MODEL(treestore), &iter,
								TREEBROWSER_COLUMN_NAME, &name,
								TREEBROWSER_COLUMN_URI, &uri,
								-1);

		menu = create_popup_menu(name != NULL ? name : "", uri != NULL ? uri : "");

#if GTK_CHECK_VERSION(3, 22, 0)
		gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)event);
#else
		gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, event->time);
#endif

		g_free(name);
		g_free(uri);
		return TRUE;
	}

	return FALSE;
}

static gboolean
on_treeview_keypress(GtkWidget *widget, GdkEventKey *event)
{
	GtkTreeIter		iter;
	GtkTreeModel	*model;
	GtkTreePath		*path;
	GdkModifierType	modifiers = gtk_accelerator_get_default_mod_mask();

	if (event->keyval == GDK_space)
	{
		if (gtk_tree_selection_get_selected(gtk_tree_view_get_selection(GTK_TREE_VIEW(widget)), &model, &iter))
		{
			path = gtk_tree_model_get_path(model, &iter);
			if (gtk_tree_view_row_expanded(GTK_TREE_VIEW(widget), path))
				gtk_tree_view_collapse_row(GTK_TREE_VIEW(widget), path);
			else
			{
				gtk_tree_view_expand_row(GTK_TREE_VIEW(widget), path, FALSE);
				treebrowser_focus_first_child(&iter);
			}
			return TRUE;
		}
	}
	if (event->keyval == GDK_BackSpace)
	{
		on_button_go_up();
		return TRUE;
	}
	if (event->keyval == GDK_Delete)
	{
		if (gtk_tree_selection_get_selected(gtk_tree_view_get_selection(GTK_TREE_VIEW(widget)), &model, &iter))
			on_menu_delete(NULL, NULL);
		return TRUE;
	}
	if ((event->keyval == GDK_Menu) ||
	    (event->keyval == GDK_F10 && (event->state & modifiers) == GDK_SHIFT_MASK))
	{
		gchar *name = NULL, *uri = NULL;
		GtkWidget *menu;

		if (gtk_tree_selection_get_selected(gtk_tree_view_get_selection(GTK_TREE_VIEW(widget)), &model, &iter))
			gtk_tree_model_get(GTK_TREE_MODEL(treestore), &iter,
								TREEBROWSER_COLUMN_NAME, &name,
								TREEBROWSER_COLUMN_URI, &uri,
								-1);

		menu = create_popup_menu(name != NULL ? name : "", uri != NULL ? uri : "");
#if GTK_CHECK_VERSION(3, 22, 0)
		gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)event);
#else
		gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 0, event->time);
#endif

		g_free(name);
		g_free(uri);

		return TRUE;
	}
	if (event->keyval == GDK_Left)
	{
		if (gtk_tree_selection_get_selected(gtk_tree_view_get_selection(GTK_TREE_VIEW(widget)), &model, &iter))
		{
			path = gtk_tree_model_get_path(model, &iter);
			if (gtk_tree_view_row_expanded(GTK_TREE_VIEW(widget), path))
				gtk_tree_view_collapse_row(GTK_TREE_VIEW(widget), path);
			else if (gtk_tree_path_get_depth(path) > 1) {
				gtk_tree_path_up(path);
				gtk_tree_view_set_cursor(GTK_TREE_VIEW(widget), path, NULL, FALSE);
				gtk_tree_selection_select_path(gtk_tree_view_get_selection(GTK_TREE_VIEW(widget)), path);
			}
		}
		return TRUE;
	}
	if (event->keyval == GDK_Right)
	{
		if (gtk_tree_selection_get_selected(gtk_tree_view_get_selection(GTK_TREE_VIEW(widget)), &model, &iter))
		{
			path = gtk_tree_model_get_path(model, &iter);
			if (!gtk_tree_view_row_expanded(GTK_TREE_VIEW(widget), path))
			{
				gtk_tree_view_expand_row(GTK_TREE_VIEW(widget), path, FALSE);
				treebrowser_focus_first_child(&iter);
			}
		}
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

		if (g_file_test(uri, G_FILE_TEST_EXISTS)) {
			if (!g_file_test(uri, G_FILE_TEST_IS_DIR) && CONFIG_ONE_CLICK_CHDOC)
				document_open_file(uri, FALSE, NULL, NULL);
		}

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
			else {
				treebrowser_browse(uri, &iter);
				gtk_tree_view_expand_row(GTK_TREE_VIEW(widget), path, FALSE);
				treebrowser_focus_first_child(&iter);
			}
	else {
		GeanyDocument *cur  = document_get_current();
		GeanyDocument *doc  = document_find_by_filename(uri);
		if (doc && doc == cur)
			document_reload_force(doc, NULL);
		else
			document_open_file(uri, FALSE, NULL, NULL);
		if (CONFIG_ON_OPEN_FOCUS_EDITOR)
			keybindings_send_command(GEANY_KEY_GROUP_FOCUS, GEANY_KEYS_FOCUS_EDITOR);
	}

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
#if GTK_CHECK_VERSION(3, 10, 0)
		GdkPixbuf *icon = utils_pixbuf_from_name("document-open");
#else
		GdkPixbuf *icon = utils_pixbuf_from_stock(GTK_STOCK_OPEN);
#endif

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
#if GTK_CHECK_VERSION(3, 10, 0)
		GdkPixbuf *icon = utils_pixbuf_from_name("folder");
#else
		GdkPixbuf *icon = utils_pixbuf_from_stock(GTK_STOCK_DIRECTORY);
#endif

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
	gchar 				*uri, *uri_new, *dirname;

	column 		= gtk_tree_view_get_column(GTK_TREE_VIEW(treeview), 0);
	renderers 	= gtk_cell_layout_get_cells(GTK_CELL_LAYOUT(column));
	renderer 	= g_list_nth_data(renderers, TREEBROWSER_RENDER_TEXT);
	g_list_free(renderers);

	g_object_set(G_OBJECT(renderer), "editable", FALSE, NULL);

	if (gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(treestore), &iter, path_string))
	{
		gtk_tree_model_get(GTK_TREE_MODEL(treestore), &iter, TREEBROWSER_COLUMN_URI, &uri, -1);
		if (uri)
		{
			dirname = g_path_get_dirname(uri);
			uri_new = g_strconcat(dirname, G_DIR_SEPARATOR_S, name_new, NULL);
			g_free(dirname);
			if (!(g_file_test(uri_new, G_FILE_TEST_EXISTS) &&
				strcmp(uri, uri_new) != 0 &&
				!dialogs_show_question(_("Target file '%s' exists, do you really want to replace it?"), uri_new)))
			{
				if (g_rename(uri, uri_new) == 0)
				{
					dirname = g_path_get_dirname(uri_new);
					gtk_tree_store_set(treestore, &iter,
									TREEBROWSER_COLUMN_NAME, name_new,
									TREEBROWSER_COLUMN_URI, uri_new,
									-1);
					if (gtk_tree_model_iter_parent(GTK_TREE_MODEL(treestore), &iter_parent, &iter))
						treebrowser_browse(dirname, &iter_parent);
					else
						treebrowser_browse(dirname, NULL);
					g_free(dirname);

					if (!g_file_test(uri, G_FILE_TEST_IS_DIR))
					{
						GeanyDocument *doc = document_find_by_filename(uri);
						if (doc && document_close(doc))
							document_open_file(uri_new, FALSE, NULL, NULL);
					}
				}
			}
			g_free(uri_new);
			g_free(uri);
		}
	}
	gtk_widget_grab_focus(treeview);
}

static void
on_treeview_drag_data_get(GtkWidget *widget, GdkDragContext *context,
		GtkSelectionData *data, guint info, guint time, gpointer user_data)
{
	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
	GtkTreeIter iter;
	GtkTreeModel *model;
	gchar *uri;

	if (!gtk_tree_selection_get_selected(selection, &model, &iter))
		return;

	gtk_tree_model_get(model, &iter, TREEBROWSER_COLUMN_URI, &uri, -1);
	if (!uri)
		return;

	if (info == 1) /* text/plain: relative or absolute path for terminal drops */
	{
		const gchar *root = NULL;
		GeanyProject *project = geany->app->project;

		if (project && !EMPTY(project->base_path))
			root = project->base_path;
		else if (addressbar_last_address)
			root = addressbar_last_address;

		const gchar *path = uri;
		if (root && g_str_has_prefix(uri, root))
		{
			path = uri + strlen(root);
			if (*path == G_DIR_SEPARATOR)
				path++;
		}
		gtk_selection_data_set_text(data, path, -1);
	}
	else /* text/uri-list: file URI for treebrowser-internal moves */
	{
		gchar *file_uri = g_filename_to_uri(uri, NULL, NULL);
		if (file_uri)
		{
			gchar *uri_list = g_strconcat(file_uri, "\r\n", NULL);
			gtk_selection_data_set(data,
					gtk_selection_data_get_target(data),
					8, (guchar *)uri_list, strlen(uri_list));
			g_free(uri_list);
			g_free(file_uri);
		}
	}
	g_free(uri);
}

static gboolean
on_treeview_drag_motion(GtkWidget *widget, GdkDragContext *context,
		gint x, gint y, guint time, gpointer user_data)
{
	GtkTreePath *path = NULL;
	GtkTreeViewDropPosition pos;

	/* gtk_drag_dest_set uses GTK_DEST_DEFAULT_MOTION with a G_CONNECT_AFTER handler
	 * that calls gdk_drag_status().  Returning TRUE here stops the accumulator before
	 * that handler fires, so we must call gdk_drag_status() ourselves. */
	if (gtk_drag_dest_find_target(widget, context, NULL) != GDK_NONE &&
	    gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(widget), x, y, &path, &pos))
	{
		GdkDragAction avail = gdk_drag_context_get_actions(context);
		GdkDragAction action = (avail & GDK_ACTION_MOVE) ? GDK_ACTION_MOVE : GDK_ACTION_COPY;

		gtk_tree_view_set_drag_dest_row(GTK_TREE_VIEW(widget), path,
				GTK_TREE_VIEW_DROP_INTO_OR_BEFORE);
		gtk_tree_path_free(path);
		gdk_drag_status(context, action, time);
	}
	else
	{
		gtk_tree_view_set_drag_dest_row(GTK_TREE_VIEW(widget), NULL, 0);
		gdk_drag_status(context, 0, time);
	}

	return TRUE;
}

static void
on_treeview_drag_leave(GtkWidget *widget, GdkDragContext *context,
		guint time, gpointer user_data)
{
	gtk_tree_view_set_drag_dest_row(GTK_TREE_VIEW(widget), NULL, 0);
}

static void
on_treeview_drag_data_received(GtkWidget *widget, GdkDragContext *context,
		gint x, gint y, GtkSelectionData *data, guint info, guint time, gpointer user_data)
{
	GtkTreePath *path = NULL;
	GtkTreeViewDropPosition pos;
	GtkTreeIter iter;
	gchar *dest_uri = NULL, *dest_dir = NULL;
	gchar **uris = NULL;
	gint i;
	gboolean moved = FALSE;

	gtk_tree_view_set_drag_dest_row(GTK_TREE_VIEW(widget), NULL, 0);

	if (!gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(widget), x, y, &path, &pos))
		return;

	gtk_tree_model_get_iter(GTK_TREE_MODEL(treestore), &iter, path);
	gtk_tree_path_free(path);
	gtk_tree_model_get(GTK_TREE_MODEL(treestore), &iter,
			TREEBROWSER_COLUMN_URI, &dest_uri, -1);

	if (!dest_uri)
		return;

	dest_dir = g_file_test(dest_uri, G_FILE_TEST_IS_DIR)
			? g_strdup(dest_uri)
			: g_path_get_dirname(dest_uri);
	g_free(dest_uri);

	uris = gtk_selection_data_get_uris(data);
	if (!uris)
	{
		g_free(dest_dir);
		return;
	}

	for (i = 0; uris[i]; i++)
	{
		gchar *src_path = g_filename_from_uri(uris[i], NULL, NULL);
		gchar *basename, *new_path;

		if (!src_path)
			continue;

		basename = g_path_get_basename(src_path);
		new_path = g_build_filename(dest_dir, basename, NULL);
		g_free(basename);

		if (strcmp(src_path, new_path) != 0)
		{
			if (!g_file_test(new_path, G_FILE_TEST_EXISTS) ||
				dialogs_show_question(_("'%s' already exists. Replace it?"), new_path))
			{
				GeanyDocument *doc = document_find_by_filename(src_path);

				if (g_rename(src_path, new_path) == 0)
				{
					moved = TRUE;
					if (doc && document_close(doc))
						document_open_file(new_path, FALSE, NULL, NULL);
				}
				else
					ui_set_statusbar(TRUE, _("Could not move '%s' to '%s'."),
							src_path, new_path);
			}
		}

		g_free(new_path);
		g_free(src_path);
	}

	g_strfreev(uris);
	g_free(dest_dir);

	if (moved)
		treebrowser_chroot(addressbar_last_address);
}

static void
treebrowser_track_current_cb(void)
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
create_view_and_model(void)
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

	g_object_set(view, "has-tooltip", TRUE, "tooltip-column", TREEBROWSER_COLUMN_URI, NULL);

	gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(view)), GTK_SELECTION_SINGLE);

	gtk_tree_view_set_enable_tree_lines(GTK_TREE_VIEW(view), CONFIG_SHOW_TREE_LINES);

	treestore = gtk_tree_store_new(TREEBROWSER_COLUMNC, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);

	gtk_tree_view_set_model(GTK_TREE_VIEW(view), GTK_TREE_MODEL(treestore));
	g_signal_connect(G_OBJECT(render_text), "edited", G_CALLBACK(on_treeview_renamed), view);

	return view;
}

static void
create_sidebar(void)
{
	GtkWidget 			*scrollwin;
	GtkWidget 			*toolbar;
	GtkWidget 			*wid;
	GtkTreeSelection 	*selection;

#if GTK_CHECK_VERSION(3, 0, 0)
	GtkCssProvider *provider;
	GdkDisplay *display;
	GdkScreen *screen;

	provider = gtk_css_provider_new ();
	display = gdk_display_get_default ();
	screen = gdk_display_get_default_screen (display);
	gtk_style_context_add_provider_for_screen (screen,
		GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	gtk_css_provider_load_from_data (GTK_CSS_PROVIDER(provider),
		"#addressbar.invalid {color: #ffffff; background: #ff6666;}",
		-1, NULL);
#endif

	treeview 				= create_view_and_model();
	sidebar_vbox 			= gtk_vbox_new(FALSE, 0);
	sidebar_vbox_bars 		= gtk_vbox_new(FALSE, 0);
	selection 				= gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	addressbar 				= gtk_entry_new();
#if GTK_CHECK_VERSION(3, 10, 0)
	gtk_widget_set_name(addressbar, "addressbar");
#endif
	filter 					= gtk_entry_new();
	scrollwin 				= gtk_scrolled_window_new(NULL, NULL);

	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollwin), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	toolbar = gtk_toolbar_new();
	gtk_toolbar_set_icon_size(GTK_TOOLBAR(toolbar), GTK_ICON_SIZE_MENU);
	gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);

#if GTK_CHECK_VERSION(3, 10, 0)
	wid = gtk_image_new_from_icon_name("go-up", GTK_ICON_SIZE_SMALL_TOOLBAR);
	wid = GTK_WIDGET(gtk_tool_button_new(wid, NULL));
#else
	wid = GTK_WIDGET(gtk_tool_button_new_from_stock(GTK_STOCK_GO_UP));
#endif
	gtk_widget_set_tooltip_text(wid, _("Go up"));
	g_signal_connect(wid, "clicked", G_CALLBACK(on_button_go_up), NULL);
	gtk_container_add(GTK_CONTAINER(toolbar), wid);

#if GTK_CHECK_VERSION(3, 10, 0)
	wid = gtk_image_new_from_icon_name("view-refresh", GTK_ICON_SIZE_SMALL_TOOLBAR);
	wid = GTK_WIDGET(gtk_tool_button_new(wid, NULL));
#else
	wid = GTK_WIDGET(gtk_tool_button_new_from_stock(GTK_STOCK_REFRESH));
#endif
	gtk_widget_set_tooltip_text(wid, _("Refresh"));
	g_signal_connect(wid, "clicked", G_CALLBACK(on_button_refresh), NULL);
	gtk_container_add(GTK_CONTAINER(toolbar), wid);

#if GTK_CHECK_VERSION(3, 10, 0)
	wid = gtk_image_new_from_icon_name("go-home", GTK_ICON_SIZE_SMALL_TOOLBAR);
	wid = GTK_WIDGET(gtk_tool_button_new(wid, NULL));
#else
	wid = GTK_WIDGET(gtk_tool_button_new_from_stock(GTK_STOCK_HOME));
#endif
	gtk_widget_set_tooltip_text(wid, _("Home"));
	g_signal_connect(wid, "clicked", G_CALLBACK(on_button_go_home), NULL);
	gtk_container_add(GTK_CONTAINER(toolbar), wid);

#if GTK_CHECK_VERSION(3, 10, 0)
	wid = gtk_image_new_from_icon_name("emblem-favorite", GTK_ICON_SIZE_SMALL_TOOLBAR);
	wid = GTK_WIDGET(gtk_tool_button_new(wid, NULL));
#else
	wid = GTK_WIDGET(gtk_tool_button_new_from_stock(GTK_STOCK_ABOUT));
#endif
	gtk_widget_set_tooltip_text(wid, _("Go to project root"));
	g_signal_connect(wid, "clicked", G_CALLBACK(on_button_go_project), NULL);
	gtk_container_add(GTK_CONTAINER(toolbar), wid);

#if GTK_CHECK_VERSION(3, 10, 0)
	wid = gtk_image_new_from_icon_name("go-jump", GTK_ICON_SIZE_SMALL_TOOLBAR);
	wid = GTK_WIDGET(gtk_tool_button_new(wid, NULL));
#else
	wid = GTK_WIDGET(gtk_tool_button_new_from_stock(GTK_STOCK_JUMP_TO));
#endif
	gtk_widget_set_tooltip_text(wid, _("Set path from document"));
	g_signal_connect(wid, "clicked", G_CALLBACK(on_button_current_path), NULL);
	gtk_container_add(GTK_CONTAINER(toolbar), wid);

#if GTK_CHECK_VERSION(3, 10, 0)
	wid = gtk_image_new_from_icon_name("folder", GTK_ICON_SIZE_SMALL_TOOLBAR);
	wid = GTK_WIDGET(gtk_tool_button_new(wid, NULL));
#else
	wid = GTK_WIDGET(gtk_tool_button_new_from_stock(GTK_STOCK_DIRECTORY));
#endif
	gtk_widget_set_tooltip_text(wid, _("Track path (Ctrl+click also copies relative path)"));
	g_signal_connect(wid, "clicked", G_CALLBACK(on_button_track_path), NULL);
	gtk_container_add(GTK_CONTAINER(toolbar), wid);

{
#if GTK_CHECK_VERSION(3, 10, 0)
		GtkWidget *img = gtk_image_new_from_icon_name("system-run", GTK_ICON_SIZE_SMALL_TOOLBAR);
		toolbar_tools_button = GTK_WIDGET(gtk_menu_tool_button_new(img, NULL));
#else
		toolbar_tools_button = GTK_WIDGET(gtk_menu_tool_button_new_from_stock(GTK_STOCK_EXECUTE));
#endif
		gtk_widget_set_tooltip_text(toolbar_tools_button, _("Project file tools for current root"));
		gtk_menu_tool_button_set_menu(GTK_MENU_TOOL_BUTTON(toolbar_tools_button), gtk_menu_new());
		g_signal_connect(toolbar_tools_button, "show-menu",
		                 G_CALLBACK(on_toolbar_tools_show_menu), NULL);
		gtk_container_add(GTK_CONTAINER(toolbar), toolbar_tools_button);
	}

#if GTK_CHECK_VERSION(3, 10, 0)
	wid = gtk_image_new_from_icon_name("window-close", GTK_ICON_SIZE_SMALL_TOOLBAR);
	wid = GTK_WIDGET(gtk_tool_button_new(wid, NULL));
#else
	wid = GTK_WIDGET(gtk_tool_button_new_from_stock(GTK_STOCK_CLOSE));
#endif
	gtk_widget_set_tooltip_text(wid, _("Hide bars"));
	g_signal_connect(wid, "clicked", G_CALLBACK(on_button_hide_bars), NULL);
	gtk_container_add(GTK_CONTAINER(toolbar), wid);

	gtk_container_add(GTK_CONTAINER(scrollwin), 			treeview);
	gtk_box_pack_start(GTK_BOX(sidebar_vbox_bars), 			filter, 			FALSE, TRUE,  1);
	gtk_box_pack_start(GTK_BOX(sidebar_vbox_bars), 			addressbar, 		FALSE, TRUE,  1);
	gtk_box_pack_start(GTK_BOX(sidebar_vbox_bars), 			toolbar, 			FALSE, TRUE,  1);

	gtk_widget_set_tooltip_text(filter,
		_("Filter (*.c;*.h;*.cpp), and if you want temporary filter using the '!' reverse try for example this '!;*.c;*.h;*.cpp'"));
	ui_entry_add_clear_icon(GTK_ENTRY(filter));
	g_signal_connect(filter, "icon-release", G_CALLBACK(on_filter_clear), NULL);

	gtk_widget_set_tooltip_text(addressbar,
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
	g_signal_connect(treeview, 			"key-press-event", 		G_CALLBACK(on_treeview_keypress), 			NULL);
	g_signal_connect(addressbar, 		"activate", 			G_CALLBACK(on_addressbar_activate), 			NULL);
	g_signal_connect(filter, 			"activate", 			G_CALLBACK(on_filter_activate), 				NULL);

	{
		static GtkTargetEntry dnd_targets[] = {
			{ (gchar *)"text/uri-list", 0, 0 },
			{ (gchar *)"text/plain",    0, 1 },
		};
		gtk_drag_source_set(treeview, GDK_BUTTON1_MASK,
				dnd_targets, G_N_ELEMENTS(dnd_targets), GDK_ACTION_MOVE | GDK_ACTION_COPY);
		gtk_drag_dest_set(treeview,
				GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP,
				dnd_targets, G_N_ELEMENTS(dnd_targets), GDK_ACTION_MOVE);
		g_signal_connect(treeview, "drag-data-get",
				G_CALLBACK(on_treeview_drag_data_get), NULL);
		g_signal_connect(treeview, "drag-motion",
				G_CALLBACK(on_treeview_drag_motion), NULL);
		g_signal_connect(treeview, "drag-leave",
				G_CALLBACK(on_treeview_drag_leave), NULL);
		g_signal_connect(treeview, "drag-data-received",
				G_CALLBACK(on_treeview_drag_data_received), NULL);
	}

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
	GtkWidget *OPEN_TERMINAL;
	GtkWidget *REVERSE_FILTER;
	GtkWidget *ONE_CLICK_CHDOC;
	GtkWidget *SHOW_HIDDEN_FILES;
	GtkWidget *HIDE_OBJECT_FILES;
	GtkWidget *SHOW_BARS;
	GtkWidget *CHROOT_ON_DCLICK;
	GtkWidget *FOLLOW_CURRENT_DOC;
	GtkWidget *ON_DELETE_CLOSE_FILE;
	GtkWidget *ON_OPEN_FOCUS_EDITOR;
	GtkWidget *SHOW_TREE_LINES;
	GtkWidget *SHOW_BOOKMARKS;
	GtkWidget *SHOW_ICONS;
	GtkWidget *OPEN_NEW_FILES;
} configure_widgets;

static void
load_settings(void)
{
	GKeyFile *config 	= g_key_file_new();

	g_key_file_load_from_file(config, CONFIG_FILE, G_KEY_FILE_NONE, NULL);

	CONFIG_OPEN_EXTERNAL_CMD 		=  utils_get_setting_string(config, "treebrowser", "open_external_cmd", 	CONFIG_OPEN_EXTERNAL_CMD_DEFAULT);
	CONFIG_OPEN_TERMINAL 				= utils_get_setting_string(config, "treebrowser", "open_terminal", CONFIG_OPEN_TERMINAL_DEFAULT);
	CONFIG_REVERSE_FILTER 			= utils_get_setting_boolean(config, "treebrowser", "reverse_filter", 		CONFIG_REVERSE_FILTER);
	CONFIG_ONE_CLICK_CHDOC 			= utils_get_setting_boolean(config, "treebrowser", "one_click_chdoc", 		CONFIG_ONE_CLICK_CHDOC);
	CONFIG_SHOW_HIDDEN_FILES 		= utils_get_setting_boolean(config, "treebrowser", "show_hidden_files", 	CONFIG_SHOW_HIDDEN_FILES);
	CONFIG_HIDE_OBJECT_FILES 		= utils_get_setting_boolean(config, "treebrowser", "hide_object_files", 	CONFIG_HIDE_OBJECT_FILES);
	CONFIG_SHOW_BARS 				= utils_get_setting_integer(config, "treebrowser", "show_bars", 			CONFIG_SHOW_BARS);
	CONFIG_CHROOT_ON_DCLICK 		= utils_get_setting_boolean(config, "treebrowser", "chroot_on_dclick", 		CONFIG_CHROOT_ON_DCLICK);
	CONFIG_FOLLOW_CURRENT_DOC 		= utils_get_setting_boolean(config, "treebrowser", "follow_current_doc", 	CONFIG_FOLLOW_CURRENT_DOC);
	CONFIG_ON_DELETE_CLOSE_FILE 	= utils_get_setting_boolean(config, "treebrowser", "on_delete_close_file", 	CONFIG_ON_DELETE_CLOSE_FILE);
	CONFIG_ON_OPEN_FOCUS_EDITOR		= utils_get_setting_boolean(config, "treebrowser", "on_open_focus_editor", 	CONFIG_ON_OPEN_FOCUS_EDITOR);
	CONFIG_SHOW_TREE_LINES 			= utils_get_setting_boolean(config, "treebrowser", "show_tree_lines", 		CONFIG_SHOW_TREE_LINES);
	CONFIG_SHOW_BOOKMARKS 			= utils_get_setting_boolean(config, "treebrowser", "show_bookmarks", 		CONFIG_SHOW_BOOKMARKS);
	CONFIG_SHOW_ICONS 				= utils_get_setting_integer(config, "treebrowser", "show_icons", 			CONFIG_SHOW_ICONS);
	CONFIG_OPEN_NEW_FILES			= utils_get_setting_boolean(config, "treebrowser", "open_new_files",		CONFIG_OPEN_NEW_FILES);

	g_key_file_free(config);
}

static gboolean
save_settings(void)
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
	g_key_file_set_string(config, 	"treebrowser", "open_terminal", 	CONFIG_OPEN_TERMINAL);
	g_key_file_set_boolean(config, 	"treebrowser", "reverse_filter", 		CONFIG_REVERSE_FILTER);
	g_key_file_set_boolean(config, 	"treebrowser", "one_click_chdoc", 		CONFIG_ONE_CLICK_CHDOC);
	g_key_file_set_boolean(config, 	"treebrowser", "show_hidden_files", 	CONFIG_SHOW_HIDDEN_FILES);
	g_key_file_set_boolean(config, 	"treebrowser", "hide_object_files", 	CONFIG_HIDE_OBJECT_FILES);
	g_key_file_set_integer(config, 	"treebrowser", "show_bars", 			CONFIG_SHOW_BARS);
	g_key_file_set_boolean(config, 	"treebrowser", "chroot_on_dclick", 		CONFIG_CHROOT_ON_DCLICK);
	g_key_file_set_boolean(config, 	"treebrowser", "follow_current_doc", 	CONFIG_FOLLOW_CURRENT_DOC);
	g_key_file_set_boolean(config, 	"treebrowser", "on_delete_close_file", 	CONFIG_ON_DELETE_CLOSE_FILE);
	g_key_file_set_boolean(config, 	"treebrowser", "on_open_focus_editor", 	CONFIG_ON_OPEN_FOCUS_EDITOR);
	g_key_file_set_boolean(config, 	"treebrowser", "show_tree_lines", 		CONFIG_SHOW_TREE_LINES);
	g_key_file_set_boolean(config, 	"treebrowser", "show_bookmarks", 		CONFIG_SHOW_BOOKMARKS);
	g_key_file_set_integer(config, 	"treebrowser", "show_icons", 			CONFIG_SHOW_ICONS);
	g_key_file_set_boolean(config,	"treebrowser", "open_new_files",		CONFIG_OPEN_NEW_FILES);

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
	CONFIG_OPEN_TERMINAL     	= gtk_editable_get_chars(GTK_EDITABLE(configure_widgets.OPEN_TERMINAL), 0, -1);
	CONFIG_REVERSE_FILTER 		= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(configure_widgets.REVERSE_FILTER));
	CONFIG_ONE_CLICK_CHDOC 		= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(configure_widgets.ONE_CLICK_CHDOC));
	CONFIG_SHOW_HIDDEN_FILES 	= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(configure_widgets.SHOW_HIDDEN_FILES));
	CONFIG_HIDE_OBJECT_FILES 	= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(configure_widgets.HIDE_OBJECT_FILES));
	CONFIG_SHOW_BARS 			= gtk_combo_box_get_active(GTK_COMBO_BOX(configure_widgets.SHOW_BARS));
	CONFIG_CHROOT_ON_DCLICK 	= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(configure_widgets.CHROOT_ON_DCLICK));
	CONFIG_FOLLOW_CURRENT_DOC 	= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(configure_widgets.FOLLOW_CURRENT_DOC));
	CONFIG_ON_DELETE_CLOSE_FILE = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(configure_widgets.ON_DELETE_CLOSE_FILE));
	CONFIG_ON_OPEN_FOCUS_EDITOR	= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(configure_widgets.ON_OPEN_FOCUS_EDITOR));
	CONFIG_SHOW_TREE_LINES 		= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(configure_widgets.SHOW_TREE_LINES));
	CONFIG_SHOW_BOOKMARKS 		= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(configure_widgets.SHOW_BOOKMARKS));
	CONFIG_SHOW_ICONS 			= gtk_combo_box_get_active(GTK_COMBO_BOX(configure_widgets.SHOW_ICONS));
	CONFIG_OPEN_NEW_FILES		= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(configure_widgets.OPEN_NEW_FILES));

	if (save_settings() == TRUE)
	{
		gtk_tree_view_set_enable_tree_lines(GTK_TREE_VIEW(treeview), CONFIG_SHOW_TREE_LINES);
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
#if GTK_CHECK_VERSION(3, 14, 0)
	gtk_widget_set_halign(label, 0);
	gtk_widget_set_valign(label, 0.5);
#else
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
#endif
	gtk_widget_set_tooltip_text(configure_widgets.OPEN_EXTERNAL_CMD,
		/* xgettext:no-c-format, %d/%f are displayed as-is */
		_("The command to execute when using \"Open with\". You can use %f and %d wildcards.\n"
		  "%f will be replaced with the filename including full path\n"
		  "%d will be replaced with the path name of the selected file without the filename"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 6);
	gtk_box_pack_start(GTK_BOX(hbox), configure_widgets.OPEN_EXTERNAL_CMD, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 6);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Terminal"));
	configure_widgets.OPEN_TERMINAL = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(configure_widgets.OPEN_TERMINAL), CONFIG_OPEN_TERMINAL);

#if GTK_CHECK_VERSION(3, 14, 0)
	gtk_widget_set_halign(label, 0);
	gtk_widget_set_valign(label, 0.5);
#else
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
#endif
	gtk_widget_set_tooltip_text(configure_widgets.OPEN_TERMINAL,
		_("The terminal to use with the command \"Open Terminal\""));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 6);
	gtk_box_pack_start(GTK_BOX(hbox), configure_widgets.OPEN_TERMINAL, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 6);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Toolbar"));
	configure_widgets.SHOW_BARS = gtk_combo_box_text_new();
	gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(configure_widgets.SHOW_BARS), _("Hidden"));
	gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(configure_widgets.SHOW_BARS), _("Top"));
	gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(configure_widgets.SHOW_BARS), _("Bottom"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 6);
	gtk_box_pack_start(GTK_BOX(hbox), configure_widgets.SHOW_BARS, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 6);
	gtk_widget_set_tooltip_text(configure_widgets.SHOW_BARS,
		_("If position is changed, the option require plugin restart."));
	gtk_combo_box_set_active(GTK_COMBO_BOX(configure_widgets.SHOW_BARS), CONFIG_SHOW_BARS);

	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(_("Show icons"));
	configure_widgets.SHOW_ICONS = gtk_combo_box_text_new();
	gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(configure_widgets.SHOW_ICONS), _("None"));
	gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(configure_widgets.SHOW_ICONS), _("Base"));
	gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(configure_widgets.SHOW_ICONS), _("Content-type"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 6);
	gtk_box_pack_start(GTK_BOX(hbox), configure_widgets.SHOW_ICONS, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 6);
	gtk_combo_box_set_active(GTK_COMBO_BOX(configure_widgets.SHOW_ICONS), CONFIG_SHOW_ICONS);

	configure_widgets.SHOW_HIDDEN_FILES = gtk_check_button_new_with_label(_("Show hidden files"));
#if GTK_CHECK_VERSION(3, 20, 0)
	gtk_widget_set_focus_on_click(configure_widgets.SHOW_HIDDEN_FILES, FALSE);
#else
	gtk_button_set_focus_on_click(GTK_BUTTON(configure_widgets.SHOW_HIDDEN_FILES), FALSE);
#endif
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(configure_widgets.SHOW_HIDDEN_FILES), CONFIG_SHOW_HIDDEN_FILES);
	gtk_box_pack_start(GTK_BOX(vbox), configure_widgets.SHOW_HIDDEN_FILES, FALSE, FALSE, 0);
	gtk_widget_set_tooltip_text(configure_widgets.SHOW_HIDDEN_FILES,
		_("On Windows, this just hide files that are prefixed with '.' (dot)"));

	configure_widgets.HIDE_OBJECT_FILES = gtk_check_button_new_with_label(_("Hide object files"));
#if GTK_CHECK_VERSION(3, 20, 0)
	gtk_widget_set_focus_on_click(configure_widgets.HIDE_OBJECT_FILES, FALSE);
#else
	gtk_button_set_focus_on_click(GTK_BUTTON(configure_widgets.HIDE_OBJECT_FILES), FALSE);
#endif
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(configure_widgets.HIDE_OBJECT_FILES), CONFIG_HIDE_OBJECT_FILES);
	gtk_box_pack_start(GTK_BOX(vbox), configure_widgets.HIDE_OBJECT_FILES, FALSE, FALSE, 0);
	gtk_widget_set_tooltip_text(configure_widgets.HIDE_OBJECT_FILES,
		_("Don't show generated object files in the file browser, this includes *.o, *.obj. *.so, *.dll, *.a, *.lib"));

	configure_widgets.REVERSE_FILTER = gtk_check_button_new_with_label(_("Reverse filter"));
#if GTK_CHECK_VERSION(3, 20, 0)
	gtk_widget_set_focus_on_click(configure_widgets.REVERSE_FILTER, FALSE);
#else
	gtk_button_set_focus_on_click(GTK_BUTTON(configure_widgets.REVERSE_FILTER), FALSE);
#endif
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(configure_widgets.REVERSE_FILTER), CONFIG_REVERSE_FILTER);
	gtk_box_pack_start(GTK_BOX(vbox), configure_widgets.REVERSE_FILTER, FALSE, FALSE, 0);

	configure_widgets.FOLLOW_CURRENT_DOC = gtk_check_button_new_with_label(_("Follow current document"));
#if GTK_CHECK_VERSION(3, 20, 0)
	gtk_widget_set_focus_on_click(configure_widgets.FOLLOW_CURRENT_DOC, FALSE);
#else
	gtk_button_set_focus_on_click(GTK_BUTTON(configure_widgets.FOLLOW_CURRENT_DOC), FALSE);
#endif
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(configure_widgets.FOLLOW_CURRENT_DOC), CONFIG_FOLLOW_CURRENT_DOC);
	gtk_box_pack_start(GTK_BOX(vbox), configure_widgets.FOLLOW_CURRENT_DOC, FALSE, FALSE, 0);

	configure_widgets.ONE_CLICK_CHDOC = gtk_check_button_new_with_label(_("Single click, open document and focus it"));
#if GTK_CHECK_VERSION(3, 20, 0)
	gtk_widget_set_focus_on_click(configure_widgets.ONE_CLICK_CHDOC, FALSE);
#else
	gtk_button_set_focus_on_click(GTK_BUTTON(configure_widgets.ONE_CLICK_CHDOC), FALSE);
#endif
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(configure_widgets.ONE_CLICK_CHDOC), CONFIG_ONE_CLICK_CHDOC);
	gtk_box_pack_start(GTK_BOX(vbox), configure_widgets.ONE_CLICK_CHDOC, FALSE, FALSE, 0);

	configure_widgets.CHROOT_ON_DCLICK = gtk_check_button_new_with_label(_("Double click open directory"));
#if GTK_CHECK_VERSION(3, 20, 0)
	gtk_widget_set_focus_on_click(configure_widgets.ONE_CLICK_CHDOC, FALSE);
#else
	gtk_button_set_focus_on_click(GTK_BUTTON(configure_widgets.CHROOT_ON_DCLICK), FALSE);
#endif
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(configure_widgets.CHROOT_ON_DCLICK), CONFIG_CHROOT_ON_DCLICK);
	gtk_box_pack_start(GTK_BOX(vbox), configure_widgets.CHROOT_ON_DCLICK, FALSE, FALSE, 0);

	configure_widgets.ON_DELETE_CLOSE_FILE = gtk_check_button_new_with_label(_("On delete file, close it if is opened"));
#if GTK_CHECK_VERSION(3, 20, 0)
	gtk_widget_set_focus_on_click(configure_widgets.ON_DELETE_CLOSE_FILE, FALSE);
#else
	gtk_button_set_focus_on_click(GTK_BUTTON(configure_widgets.ON_DELETE_CLOSE_FILE), FALSE);
#endif
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(configure_widgets.ON_DELETE_CLOSE_FILE), CONFIG_ON_DELETE_CLOSE_FILE);
	gtk_box_pack_start(GTK_BOX(vbox), configure_widgets.ON_DELETE_CLOSE_FILE, FALSE, FALSE, 0);

	configure_widgets.ON_OPEN_FOCUS_EDITOR = gtk_check_button_new_with_label(_("Focus editor on file open"));
#if GTK_CHECK_VERSION(3, 20, 0)
	gtk_widget_set_focus_on_click(configure_widgets.ON_OPEN_FOCUS_EDITOR, FALSE);
#else
	gtk_button_set_focus_on_click(GTK_BUTTON(configure_widgets.ON_OPEN_FOCUS_EDITOR), FALSE);
#endif
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(configure_widgets.ON_OPEN_FOCUS_EDITOR), CONFIG_ON_OPEN_FOCUS_EDITOR);
	gtk_box_pack_start(GTK_BOX(vbox), configure_widgets.ON_OPEN_FOCUS_EDITOR, FALSE, FALSE, 0);

	configure_widgets.SHOW_TREE_LINES = gtk_check_button_new_with_label(_("Show tree lines"));
#if GTK_CHECK_VERSION(3, 20, 0)
	gtk_widget_set_focus_on_click(configure_widgets.SHOW_TREE_LINES, FALSE);
#else
	gtk_button_set_focus_on_click(GTK_BUTTON(configure_widgets.SHOW_TREE_LINES), FALSE);
#endif
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(configure_widgets.SHOW_TREE_LINES), CONFIG_SHOW_TREE_LINES);
	gtk_box_pack_start(GTK_BOX(vbox), configure_widgets.SHOW_TREE_LINES, FALSE, FALSE, 0);

	configure_widgets.SHOW_BOOKMARKS = gtk_check_button_new_with_label(_("Show bookmarks"));
#if GTK_CHECK_VERSION(3, 20, 0)
	gtk_widget_set_focus_on_click(configure_widgets.SHOW_BOOKMARKS, FALSE);
#else
	gtk_button_set_focus_on_click(GTK_BUTTON(configure_widgets.SHOW_BOOKMARKS), FALSE);
#endif
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(configure_widgets.SHOW_BOOKMARKS), CONFIG_SHOW_BOOKMARKS);
	gtk_box_pack_start(GTK_BOX(vbox), configure_widgets.SHOW_BOOKMARKS, FALSE, FALSE, 0);

	configure_widgets.OPEN_NEW_FILES = gtk_check_button_new_with_label(_("Open new files"));
#if GTK_CHECK_VERSION(3, 20, 0)
	gtk_widget_set_focus_on_click(configure_widgets.OPEN_NEW_FILES, FALSE);
#else
	gtk_button_set_focus_on_click(GTK_BUTTON(configure_widgets.OPEN_NEW_FILES ), FALSE);
#endif
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(configure_widgets.OPEN_NEW_FILES ), CONFIG_OPEN_NEW_FILES);
	gtk_box_pack_start(GTK_BOX(vbox), configure_widgets.OPEN_NEW_FILES , FALSE, FALSE, 0);

	gtk_widget_show_all(vbox);

	g_signal_connect(dialog, "response", G_CALLBACK(on_configure_response), NULL);

	return vbox;
}


/* ------------------
 * GEANY HOOKS
 * ------------------ */

static void
project_open_cb(G_GNUC_UNUSED GObject *obj, G_GNUC_UNUSED GKeyFile *config, G_GNUC_UNUSED gpointer data)
{
	GeanyProject *project = geany->app->project;
	if (project && !EMPTY(project->base_path))
	{
		gchar *uri = utils_get_locale_from_utf8(project->base_path);
		treebrowser_chroot(uri);
		g_free(uri);
	}
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

		case KB_CREATE_FILE:
			treebrowser_create_new_current("file");
			break;

		case KB_CREATE_DIR:
			treebrowser_create_new_current("directory");
			break;

		case KB_REFRESH:
			on_menu_refresh(NULL, NULL);
			break;

		case KB_TRACK_CURRENT:
			treebrowser_track_current();
			break;
	}
}

static void on_treebrowser_refresh_signal(G_GNUC_UNUSED GObject *obj,
                                          G_GNUC_UNUSED gpointer data)
{
	if (addressbar_last_address)
		treebrowser_browse(addressbar_last_address, NULL);
}

static void on_signal_tb_home(G_GNUC_UNUSED GObject *obj, G_GNUC_UNUSED gpointer data)
{
	on_button_go_home();
}

static void on_signal_tb_project(G_GNUC_UNUSED GObject *obj, G_GNUC_UNUSED gpointer data)
{
	on_button_go_project();
}

static void on_signal_tb_set_path(G_GNUC_UNUSED GObject *obj, G_GNUC_UNUSED gpointer data)
{
	on_button_current_path();
}

static GtkWidget *current_popup_menu = NULL;

static void on_popup_menu_hide(GtkWidget *menu, G_GNUC_UNUSED gpointer data)
{
	if (current_popup_menu == menu)
		current_popup_menu = NULL;
}

static void on_signal_tb_navigate(G_GNUC_UNUSED GObject *obj, gint delta, G_GNUC_UNUSED gpointer data)
{
	if (current_popup_menu && GTK_IS_WIDGET(current_popup_menu) &&
	    gtk_widget_get_visible(current_popup_menu))
	{
		g_signal_emit_by_name(current_popup_menu, "move-current",
		                      delta > 0 ? GTK_MENU_DIR_NEXT : GTK_MENU_DIR_PREV);
		return;
	}
	if (!treeview) return;
	gtk_widget_grab_focus(treeview);
	gboolean handled = FALSE;
	g_signal_emit_by_name(treeview, "move-cursor",
	                      GTK_MOVEMENT_DISPLAY_LINES, delta, &handled);
}

static void on_signal_tb_activate(G_GNUC_UNUSED GObject *obj, G_GNUC_UNUSED gpointer data)
{
	if (!treeview) return;
	GtkTreePath       *path = NULL;
	GtkTreeViewColumn *col  = NULL;
	gtk_tree_view_get_cursor(GTK_TREE_VIEW(treeview), &path, &col);
	if (!col)
		col = gtk_tree_view_get_column(GTK_TREE_VIEW(treeview), 0);
	if (path) {
		on_treeview_row_activated(treeview, path, col, NULL);
		gtk_tree_path_free(path);
	}
}

static void on_signal_tb_focus(G_GNUC_UNUSED GObject *obj, G_GNUC_UNUSED gpointer data)
{
	if (!treeview) return;
	gtk_notebook_set_current_page(
		GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook), page_number);
	gtk_widget_grab_focus(treeview);
}

static void on_signal_tb_popup_menu(G_GNUC_UNUSED GObject *obj, G_GNUC_UNUSED gpointer data)
{
	if (!toolbar_tools_button) return;

	/* Populate the toolbar tools dropdown (same handler as clicking the arrow) */
	on_toolbar_tools_show_menu(GTK_MENU_TOOL_BUTTON(toolbar_tools_button), NULL);
	GtkWidget *menu = gtk_menu_tool_button_get_menu(
	    GTK_MENU_TOOL_BUTTON(toolbar_tools_button));
	if (!menu) return;

	current_popup_menu = menu;
	g_signal_connect(menu, "hide", G_CALLBACK(on_popup_menu_hide), NULL);

	/* Notify interested parties (e.g. geanyvosk) so they can track the menu */
	GType obj_type = G_OBJECT_TYPE(geany->object);
	if (g_signal_lookup("treebrowser-menu-ready", obj_type))
		g_signal_emit_by_name(geany->object, "treebrowser-menu-ready", menu);

	gtk_menu_popup_at_widget(GTK_MENU(menu), GTK_WIDGET(toolbar_tools_button),
	                         GDK_GRAVITY_SOUTH_WEST, GDK_GRAVITY_NORTH_WEST, NULL);
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
	keybindings_set_item(key_group, KB_CREATE_FILE, kb_activate,
		0, 0, "create_file", _("New File"), NULL);
	keybindings_set_item(key_group, KB_CREATE_DIR, kb_activate,
		0, 0, "create_dir", _("New Folder"), NULL);
	keybindings_set_item(key_group, KB_REFRESH, kb_activate,
		0, 0, "rename_refresh", _("Refresh"), NULL);
	keybindings_set_item(key_group, KB_TRACK_CURRENT, kb_activate,
		0, 0, "track_current", _("Track Current"), NULL);

	plugin_signal_connect(geany_plugin, NULL, "document-activate", TRUE,
		(GCallback)&treebrowser_track_current_cb, NULL);

	/* Connect to geanycontrol-refresh if geanycontrol is loaded */
	{
		GType obj_type = G_OBJECT_TYPE(geany->object);
		if (g_signal_lookup("geanycontrol-refresh", obj_type))
			g_signal_connect(geany->object, "geanycontrol-refresh",
			                 G_CALLBACK(on_treebrowser_refresh_signal), NULL);
	}

	/* Register and connect treebrowser IPC signals */
	{
		GType obj_type = G_OBJECT_TYPE(geany->object);
		static const struct { const gchar *name; GType arg; } tb_sigs[] = {
			{ "treebrowser-refresh",       G_TYPE_NONE    },
			{ "treebrowser-home",          G_TYPE_NONE    },
			{ "treebrowser-project-root",  G_TYPE_NONE    },
			{ "treebrowser-set-path",      G_TYPE_NONE    },
			{ "treebrowser-navigate",      G_TYPE_INT     },
			{ "treebrowser-activate",      G_TYPE_NONE    },
			{ "treebrowser-focus",         G_TYPE_NONE    },
			{ "treebrowser-popup-menu",    G_TYPE_NONE    },
			{ "treebrowser-menu-ready",    G_TYPE_POINTER },
			{ NULL, 0 }
		};
		for (gint i = 0; tb_sigs[i].name; i++) {
			if (!g_signal_lookup(tb_sigs[i].name, obj_type)) {
				if (tb_sigs[i].arg == G_TYPE_NONE)
					g_signal_new(tb_sigs[i].name, obj_type, G_SIGNAL_RUN_LAST,
					             0, NULL, NULL, NULL, G_TYPE_NONE, 0);
				else
					g_signal_new(tb_sigs[i].name, obj_type, G_SIGNAL_RUN_LAST,
					             0, NULL, NULL, NULL, G_TYPE_NONE, 1, tb_sigs[i].arg);
			}
		}
		plugin_signal_connect(geany_plugin, geany->object, "treebrowser-refresh",
		                      FALSE, G_CALLBACK(on_treebrowser_refresh_signal), NULL);
		plugin_signal_connect(geany_plugin, geany->object, "treebrowser-home",
		                      FALSE, G_CALLBACK(on_signal_tb_home), NULL);
		plugin_signal_connect(geany_plugin, geany->object, "treebrowser-project-root",
		                      FALSE, G_CALLBACK(on_signal_tb_project), NULL);
		plugin_signal_connect(geany_plugin, geany->object, "treebrowser-set-path",
		                      FALSE, G_CALLBACK(on_signal_tb_set_path), NULL);
		plugin_signal_connect(geany_plugin, geany->object, "treebrowser-navigate",
		                      FALSE, G_CALLBACK(on_signal_tb_navigate), NULL);
		plugin_signal_connect(geany_plugin, geany->object, "treebrowser-activate",
		                      FALSE, G_CALLBACK(on_signal_tb_activate), NULL);
		plugin_signal_connect(geany_plugin, geany->object, "treebrowser-focus",
		                      FALSE, G_CALLBACK(on_signal_tb_focus), NULL);
		plugin_signal_connect(geany_plugin, geany->object, "treebrowser-popup-menu",
		                      FALSE, G_CALLBACK(on_signal_tb_popup_menu), NULL);
	}
}

void
plugin_cleanup(void)
{
	g_signal_handlers_disconnect_by_func(geany->object,
	                                     G_CALLBACK(on_treebrowser_refresh_signal), NULL);
	g_free(addressbar_last_address);
	g_free(CONFIG_FILE);
	g_free(CONFIG_OPEN_EXTERNAL_CMD);
	g_free(CONFIG_OPEN_TERMINAL);
	gtk_widget_destroy(sidebar_vbox);
}
