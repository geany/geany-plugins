//      treebrowser.c
//
//      Copyright 2010 Adrian Dimitrov <dimitrov.adrian@gmail.com>
//
//      This program is free software; you can redistribute it and/or modify
//      it under the terms of the GNU General Public License as published by
//      the Free Software Foundation; either version 2 of the License, or
//      (at your option) any later version.
//
//      This program is distributed in the hope that it will be useful,
//      but WITHOUT ANY WARRANTY; without even the implied warranty of
//      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//      GNU General Public License for more details.
//
//      You should have received a copy of the GNU General Public License
//      along with this program; if not, write to the Free Software
//      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
//      MA 02110-1301, USA.

/* Development release ChangeLog
 * -----------------------------
 *
 * (16-03-2010)
 * 		fix compilation warning messages
 * 		fix some crytical errors
 *
 * (21-02-2010)
 * 		fix patch applyed from Enrico about initial directories
 *
 * (20-02-2010)
 * v0.1.1
 * 		made strings suitable for localization
 * 		fixed problem with default chroot
 * 		added option to disable chrooting on double click on directory
 *
 * (17-02-2010)
 * 	v0.1
 *		added options to add/remove/rename directories and files
 * 		code cleanup
 *
 * (14-02-2010)
 * 	v0.0.1
 * 		initial, with base options
 *
 */



#include "geanyplugin.h"
#include "Scintilla.h"

/* These items are set by Geany before plugin_init() is called. */
GeanyPlugin					*geany_plugin;
GeanyData					*geany_data;
GeanyFunctions				*geany_functions;

static gint					page_number 				= 0;
static GtkTreeStore 		*treestore;
static GtkWidget			*treeview;
static GtkWidget			*vbox;
static GtkWidget			*vbox_bars;
static GtkWidget			*addressbar;
static gchar				*addressbar_last_address 	= NULL;
static GtkWidget 			*filter;

static GtkTreeViewColumn 	*treeview_column_icon, *treeview_column_text;
static GtkCellRenderer 		*render_icon, *render_text;

// config
static gint					CONFIG_DIRECTORY_DEEP 		= 1;
static gboolean				CONFIG_SHOW_HIDDEN_FILES 	= FALSE;
static gboolean				CONFIG_SHOW_BARS			= TRUE;
static gboolean				CONFIG_CHROOT_ON_DCLICK		= FALSE;

// treeview struct
enum
{
	TREEBROWSER_COLUMNC 								= 3,

	TREEBROWSER_COLUMN_ICON								= 0,
	TREEBROWSER_COLUMN_NAME								= 1,
	TREEBROWSER_COLUMN_URI								= 2,

	TREEBROWSER_RENDER_ICON								= 0,
	TREEBROWSER_RENDER_TEXT								= 1
};


PLUGIN_VERSION_CHECK(147)
PLUGIN_SET_INFO(_("Tree Browser"), _("Treeview filebrowser plugin."), "0.1" , " e01 (Enzo_01@abv.bg)")


// predefines
#define foreach_slist_free(node, list) \
	for (node = list, list = NULL; g_slist_free_1(list), node != NULL; list = node, node = node->next)

// prototypes
static void 		msgbox(gchar *message);
static gboolean 	dialogs_yesno(gchar* message);
static gboolean 	check_filtered(const gchar* base_name);
static gchar* 		get_default_dir(void);
static void 		on_renamed(GtkCellRenderer *renderer, const gchar *path_string, const gchar *newname, gpointer data);
static GtkWidget* 	create_popup_menu(gpointer *user_data);
static GtkWidget* 	create_view_and_model (void);
static int 			gtk_tree_store_iter_clear_nodes(GtkTreeIter iter, gboolean delete_root);
static void 		treebrowser_browse(gchar *directory, GtkTreeIter parent, gboolean isRoot, gint deep_limit);
static void 		fs_remove(gchar *root, gboolean delete_root);
static void 		treebrowser_track_path(gchar *directory);
static void 		treebrowser_chroot(gchar *directory);
static void 		showbars(gboolean state);
static void 		on_menu_showhide_bars(GtkMenuItem *menuitem, gpointer *user_data);
static void 		on_menu_external_open(GtkMenuItem *menuitem, gpointer *user_data);
static void 		on_menu_go_parent(GtkMenuItem *menuitem, gpointer *user_data);
static void			on_menu_set_as_root(GtkMenuItem *menuitem, gpointer *user_data);
static void 		on_menu_refresh(GtkMenuItem *menuitem, gpointer *user_data);
static void 		on_row_expanded(GtkWidget *widget, GtkTreeIter *iter, GtkTreePath *path, gpointer user_data);
static void 		on_row_collapsed(GtkWidget *widget, GtkTreeIter *iter, GtkTreePath *path, gpointer user_data);
static void 		on_addressbar_activate(GtkEntry *entry, gpointer user_data);
static void 		on_addressbar_icon (GtkEntry *entry, GtkEntryIconPosition icon_pos, GdkEvent *event, gpointer user_data);
static void 		on_filter_activate(GtkEntry *entry, gpointer user_data);
static gboolean* 	treebrowser_track_path_cb(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer user_data);
// end of prototypes


static void
msgbox(gchar *message)
{
	GtkWidget *window;
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new(GTK_WINDOW (window),
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_INFO,
				GTK_BUTTONS_OK,
				"%s",
				message);

	gtk_dialog_run(GTK_DIALOG (dialog));
	gtk_widget_destroy(dialog);
}


static gboolean
dialogs_yesno(gchar *message)
{
	GtkWidget *window, *dialog;
	dialog = gtk_message_dialog_new(GTK_WINDOW(window),
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_QUESTION,
				GTK_BUTTONS_YES_NO,
				"%s",
				message);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_YES)
	{
		gtk_widget_destroy(dialog);
		return TRUE;
	}
	gtk_widget_destroy(dialog);
	return FALSE;
}



static gboolean// get from filebrowser.c (with little modifications)
check_filtered(const gchar *base_name)
{
	if (gtk_entry_get_text_length(GTK_ENTRY(filter)) < 1)
		return TRUE;

	if (utils_str_equal(base_name, "*") || g_pattern_match_simple(gtk_entry_get_text(GTK_ENTRY(filter)), base_name))
		return TRUE;

	return FALSE;
}

static gchar*
get_default_dir(void)
{
	gchar 			*dir;
	GeanyProject 	*project 	= geany->app->project;
	GeanyDocument	*doc 		= document_get_current();

	if (doc != NULL && doc->file_name != NULL && ! g_path_is_absolute(doc->file_name))
		return g_path_get_dirname(doc->file_name);
	else
		if (project)
			return project->base_path;
		else
			if (geany->prefs)
				return geany->prefs->default_open_path;
			else
				return g_get_current_dir();
}

static void
on_renamed(GtkCellRenderer *renderer, const gchar *path_string, const gchar *newname, gpointer data)
{

	GtkTreeViewColumn 	*column;
	GList 				*renderers;
	GtkTreeIter 		iter;
	gchar 				*uri, *newuri;

	column 		= gtk_tree_view_get_column(GTK_TREE_VIEW(treeview), 0);
	renderers 	= gtk_cell_layout_get_cells(GTK_CELL_LAYOUT(column));
	renderer 	= g_list_nth_data(renderers, TREEBROWSER_RENDER_TEXT);

	g_object_set(G_OBJECT(renderer), "editable", FALSE, NULL);

	if (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL(treestore), &iter, path_string))
	{
		gtk_tree_model_get(GTK_TREE_MODEL(treestore), &iter, TREEBROWSER_COLUMN_URI, &uri, -1);
		if (uri)
		{
			newuri = g_strconcat(g_path_get_dirname(uri), G_DIR_SEPARATOR_S, newname, NULL);
			if (g_rename(uri, newuri) == 0)
				gtk_tree_store_set(treestore, &iter,
								TREEBROWSER_COLUMN_NAME, newname,
								TREEBROWSER_COLUMN_URI, newuri,
								-1);
		}
	}
}


/*
 * RIGHT CLICK MENU
 */

static void
on_menu_external_open(GtkMenuItem *menuitem, gpointer *user_data)
{

	GtkTreeSelection 	*selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	GtkTreeIter 		iter;
	GtkTreeModel 		*model;
	gchar 				*uri;

	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		gtk_tree_model_get(model, &iter, TREEBROWSER_COLUMN_URI, &uri, -1);
		if (g_file_test(uri, G_FILE_TEST_IS_DIR))
			utils_open_browser(uri);
	}
}

static void
on_menu_go_parent(GtkMenuItem *menuitem, gpointer *user_data)
{
	treebrowser_chroot(g_path_get_dirname(addressbar_last_address));
}


static void
on_menu_set_as_root(GtkMenuItem *menuitem, gpointer *user_data)
{

	GtkTreeSelection 	*selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	GtkTreeIter 		iter;
	GtkTreeModel 		*model;
	gchar 				*uri;

	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		gtk_tree_model_get(model, &iter, TREEBROWSER_COLUMN_URI, &uri, -1);
		if (g_file_test(uri, G_FILE_TEST_IS_DIR))
			treebrowser_chroot(uri);
	}
}


static void
on_menu_refresh(GtkMenuItem *menuitem, gpointer *user_data)
{
	treebrowser_chroot(addressbar_last_address);
}

static void
on_menu_create_new_directory(GtkMenuItem *menuitem, gpointer *user_data)
{
	GtkTreeSelection 	*selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	GtkTreeIter 		iter_new;
	gpointer 			iter = NULL;
	GtkTreeModel 		*model;
	gchar 				*uri, *newuri;

	if (gtk_tree_selection_get_selected(selection, &model, iter))
		gtk_tree_model_get(model, iter, TREEBROWSER_COLUMN_URI, &uri, -1);
	else
		uri = addressbar_last_address;

	newuri = g_strconcat(uri, G_DIR_SEPARATOR_S, _("NewDirectory"), NULL);

	while(g_file_test(newuri, G_FILE_TEST_EXISTS))
		newuri = g_strconcat(newuri, "_", NULL);

	if (g_mkdir(newuri, 0755) == 0)
	{
		gtk_tree_store_prepend(treestore, &iter_new, iter ? iter : NULL);
		gtk_tree_store_set(treestore, &iter_new,
							TREEBROWSER_COLUMN_ICON, GTK_STOCK_DIRECTORY,
							TREEBROWSER_COLUMN_NAME, g_path_get_basename(newuri),
							TREEBROWSER_COLUMN_URI, newuri,
							-1);
	}
}

static void
on_menu_create_new_file(GtkMenuItem *menuitem, gpointer *user_data)
{
	GtkTreeSelection 		*selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	GtkTreeIter 			iter_new;
	gpointer 				iter = NULL;
	GtkTreeModel 			*model;
	gchar 					*uri, *newuri;

	if (gtk_tree_selection_get_selected(selection, &model, iter))
		gtk_tree_model_get(model, iter, TREEBROWSER_COLUMN_URI, &uri, -1);
	else
		uri = addressbar_last_address;

	newuri = g_strconcat(uri, G_DIR_SEPARATOR_S, _("NewFile"), NULL);

	while(g_file_test(newuri, G_FILE_TEST_EXISTS))
		newuri = g_strconcat(newuri, "_", NULL);

	if (g_creat(newuri, 0755) != -1)
	{
		gtk_tree_store_prepend(GTK_TREE_STORE(treestore), &iter_new, iter ? iter : NULL);
		gtk_tree_store_set(GTK_TREE_STORE(treestore), &iter_new,
							TREEBROWSER_COLUMN_ICON, GTK_STOCK_FILE,
							TREEBROWSER_COLUMN_NAME, g_path_get_basename(newuri),
							TREEBROWSER_COLUMN_URI, newuri,
							-1);
	}
}

static void
on_menu_delete(GtkMenuItem *menuitem, gpointer *user_data)
{

	GtkTreeSelection 	*selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	GtkTreeIter 		iter;
	GtkTreeModel 		*model;
	gchar 				*uri, *msg;

	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		gtk_tree_model_get(model, &iter, TREEBROWSER_COLUMN_URI, &uri, -1);

		if (dialogs_yesno(g_strdup_printf(_("Do you want to remove '%s'?"), uri)))
		{
			fs_remove(uri, TRUE);
		}
	}
}

static void
on_menu_rename_activated(GtkMenuItem *menuitem, gpointer *user_data)
{
	GtkTreeSelection 	*selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	GtkTreeIter 		iter;
	GtkTreeModel 		*model;
	GtkTreeViewColumn 	*column;
	GtkCellRenderer 	*renderer;
	GtkTreePath 		*path;
	GList 				*renderers;

	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		path = gtk_tree_model_get_path(GTK_TREE_MODEL(treestore), &iter);
		if (G_LIKELY(path != NULL))
		{
			column 		= gtk_tree_view_get_column(GTK_TREE_VIEW (treeview), 0);
			renderers 	= gtk_cell_layout_get_cells(GTK_CELL_LAYOUT (column));
			renderer 	= g_list_nth_data(renderers, TREEBROWSER_RENDER_TEXT);

			g_object_set(G_OBJECT(renderer), "editable", TRUE, NULL);
			gtk_tree_view_set_cursor_on_cell(GTK_TREE_VIEW(treeview), path, column, renderer, TRUE);

			gtk_tree_path_free(path);
			g_list_free(renderers);
		}
	}
}


static void
on_menu_showhide_bars(GtkMenuItem *menuitem, gpointer *user_data)
{
	showbars(gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem)));
}

static GtkWidget*
create_popup_menu(gpointer *user_data)
{
	GtkWidget *item, *menu;

	menu = gtk_menu_new();

	item = ui_image_menu_item_new(GTK_STOCK_GO_UP, _("Go parent"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_go_parent), NULL);

	item = ui_image_menu_item_new(GTK_STOCK_OPEN, _("Open externally"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_external_open), NULL);

	item = ui_image_menu_item_new(GTK_STOCK_OPEN, _("Set as root"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_set_as_root), NULL);

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);

	item = ui_image_menu_item_new(GTK_STOCK_ADD, _("Create new directory"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_create_new_directory), NULL);

	item = ui_image_menu_item_new(GTK_STOCK_NEW, _("Create new file"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_create_new_file), NULL);

	item = ui_image_menu_item_new(GTK_STOCK_SAVE_AS, _("Rename"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_rename_activated), NULL);

	item = ui_image_menu_item_new(GTK_STOCK_DELETE, _("Delete"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_delete), NULL);

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);

	item = ui_image_menu_item_new(GTK_STOCK_REFRESH, _("Refresh root"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_refresh), NULL);

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);

	item = gtk_check_menu_item_new_with_mnemonic(_("Show bars"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), CONFIG_SHOW_BARS);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_showhide_bars), NULL);

	gtk_widget_show_all(menu);

	return menu;
}





// SIGNALS CALLBACK FUNCTIONS



static gboolean
on_button_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{

	/*if (event->button == 1 && event->type == GDK_2BUTTON_PRESS)
	{
		//on_open_clicked(NULL, NULL);
		return TRUE;
	}
	else */ if (event->button == 3)
	{
		static GtkWidget *popup_menu = NULL;

		if (popup_menu == NULL)
			popup_menu = create_popup_menu(user_data);

		gtk_menu_popup(GTK_MENU(popup_menu), NULL, NULL, NULL, NULL, event->button, event->time);
		/* don't return TRUE here, unless the selection won't be changed */
	}
	return FALSE;

}


static void
on_changed(GtkWidget *widget, gpointer user_data)
{
	GtkTreeIter 	iter;
	GtkTreeModel 	*model;
	gchar 			*uri;

	if (gtk_tree_selection_get_selected(GTK_TREE_SELECTION(widget), &model, &iter))
	{
		gtk_tree_model_get(GTK_TREE_MODEL(treestore), &iter,
							TREEBROWSER_COLUMN_URI, &uri,
							-1);
		treebrowser_browse(uri, iter, FALSE, CONFIG_DIRECTORY_DEEP);
	}
}

static void
on_row_activated (GtkWidget *widget, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data)
{
	GtkTreeIter 	iter;
	gchar 			*uri;

	gtk_tree_model_get_iter(GTK_TREE_MODEL(treestore), &iter, path);
	gtk_tree_model_get(GTK_TREE_MODEL(treestore), &iter,
							TREEBROWSER_COLUMN_URI,  &uri,
							-1);

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
}

static void
on_row_expanded(GtkWidget *widget, GtkTreeIter *iter, GtkTreePath *path, gpointer user_data)
{
	gtk_tree_store_set(treestore, iter, TREEBROWSER_COLUMN_ICON, GTK_STOCK_OPEN, -1);
}

static void
on_row_collapsed(GtkWidget *widget, GtkTreeIter *iter, GtkTreePath *path, gpointer user_data)
{
	gtk_tree_store_set(treestore, iter, TREEBROWSER_COLUMN_ICON, GTK_STOCK_DIRECTORY, -1);
}

static void
on_addressbar_activate(GtkEntry *entry, gpointer user_data)
{
	gchar *directory = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
	treebrowser_chroot(directory);
}

static void
on_addressbar_icon
(GtkEntry *entry, GtkEntryIconPosition icon_pos, GdkEvent *event, gpointer user_data)
{

	if (icon_pos == GTK_ENTRY_ICON_PRIMARY)
	{
		return;
	}

	if (icon_pos == GTK_ENTRY_ICON_SECONDARY)
	{
		//treebrowser_track_path(get_default_dir());
		treebrowser_chroot(get_default_dir());
		return;
	}
}

static void
on_filter_activate(GtkEntry *entry, gpointer user_data)
{
	treebrowser_chroot(addressbar_last_address);
}
// END OF SIGNALS


// SYSTEM FUNCTIONS


static void // recursive removing files
fs_remove(gchar *root, gboolean delete_root)
{

	if (! g_file_test(root, G_FILE_TEST_EXISTS))
		return;

	if (g_file_test(root, G_FILE_TEST_IS_DIR))
	{

		GDir *dir;
		const gchar *name;

		dir = g_dir_open (root, 0, NULL);

		if (!dir)
			return;

		name = g_dir_read_name (dir);
		while (name != NULL)
		{
			gchar *path;
			path = g_build_filename (root, name, NULL);
			if (g_file_test (path, G_FILE_TEST_IS_DIR))
				fs_remove(path, delete_root);
			g_remove(path);
			name = g_dir_read_name(dir);
			g_free (path);
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
	if (state) 	gtk_widget_show(vbox_bars);
	else 		gtk_widget_hide(vbox_bars);
	CONFIG_SHOW_BARS = state;
}

static int // recursive removing all nodes and subnodes in some node :)
gtk_tree_store_iter_clear_nodes(GtkTreeIter iter, gboolean delete_root)
{
	//return 0;
	GtkTreeIter 	i;

	while (gtk_tree_model_iter_children(GTK_TREE_MODEL(treestore), &i, &iter))
	{
		if (gtk_tree_store_iter_depth(treestore, &i) > 0)
			gtk_tree_store_iter_clear_nodes(i, TRUE);

		gtk_tree_store_remove(GTK_TREE_STORE(treestore), &i);
	}

	//if (delete_root)
	//	gtk_tree_store_remove(GTK_TREE_STORE(treestore), &iter);

	return 1;
}

static gboolean*
treebrowser_track_path_cb(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer user_data)
{
	gchar *path_str, *uri;

	gtk_tree_model_get(GTK_TREE_MODEL(treestore), iter, TREEBROWSER_COLUMN_URI, &uri, -1);

	path_str = gtk_tree_path_to_string(path);

	if (user_data == uri)
	{
		msgbox(uri);
		return FALSE;
	}

	//g_free(path_str);
	//g_free(uri);

	return FALSE;
}

static void
treebrowser_track_path(gchar *directory)
{
	//void                gtk_tree_view_column_set_expand     (GtkTreeViewColumn *tree_column,
    //                                                     gboolean expand);
	//gtk_tree_model_foreach(GTK_ENTRY(treestore), treebrowser_track_path_cb, directory);
}

static void
treebrowser_chroot(gchar *directory)
{
	GtkTreeIter toplevel_unused;

	gtk_entry_set_text(GTK_ENTRY(addressbar), directory);

	gtk_tree_store_clear(treestore);

	if (! g_file_test(directory, G_FILE_TEST_IS_DIR))
	{
		gtk_entry_set_icon_from_stock(GTK_ENTRY(addressbar), GTK_ENTRY_ICON_PRIMARY, GTK_STOCK_NO);
		return;
	}

	gtk_entry_set_icon_from_stock(GTK_ENTRY(addressbar), GTK_ENTRY_ICON_PRIMARY, GTK_STOCK_DIRECTORY);

	addressbar_last_address = directory;
	treebrowser_browse(directory, toplevel_unused, TRUE, CONFIG_DIRECTORY_DEEP);
}

static void
treebrowser_browse(gchar *directory, GtkTreeIter parent, gboolean isRoot, gint deep_limit)
{
	if (deep_limit < 1) return;
	deep_limit--;
	GtkTreeIter iter_new;
	GtkTreeIter *last_dir_iter = NULL;
	gchar *path, *display_name, *newpath;
	gboolean is_dir;
	GDir *dir;
	const gchar *name;
	const gchar *basename;
	directory = g_strconcat(directory, G_DIR_SEPARATOR_S, NULL);

	if (!isRoot)
		gtk_tree_store_iter_clear_nodes(parent, FALSE);

	gchar *utf8_dir, *utf8_name;
	GSList *list, *node;
	list = utils_get_file_list(directory, NULL, NULL);
	if (list != NULL)
	{

		foreach_slist_free(node, list)
		{
			gchar *fname = node->data;
			gchar *uri = g_strconcat(directory, fname, NULL);
			is_dir = g_file_test (uri, G_FILE_TEST_IS_DIR);
			utf8_name = utils_get_utf8_from_locale(fname);

			if (!(fname[0] == '.' && CONFIG_SHOW_HIDDEN_FILES == FALSE))
			{

				if (is_dir)
				{
					if (last_dir_iter == NULL)
						gtk_tree_store_prepend(treestore, &iter_new, isRoot ? NULL : &parent);
					else
					{
						gtk_tree_store_insert_after(treestore, &iter_new, isRoot ? NULL : &parent, last_dir_iter);
						gtk_tree_iter_free(last_dir_iter);
					}
					last_dir_iter = gtk_tree_iter_copy(&iter_new);
					gtk_tree_store_set(treestore, &iter_new, 0, is_dir ? GTK_STOCK_DIRECTORY : GTK_STOCK_FILE, 1, fname, 2, uri,-1);

					treebrowser_browse(uri, iter_new, FALSE, deep_limit);
				}
				else
				{
					if (check_filtered(utf8_name))
					{
						gtk_tree_store_append(treestore, &iter_new, isRoot ? NULL : &parent);
						gtk_tree_store_set(treestore, &iter_new, 0, is_dir ? GTK_STOCK_DIRECTORY : GTK_STOCK_FILE, 1, fname, 2, uri,-1);
					}
				}
			}
			g_free(fname);
			g_free(uri);
		}
	}

}


static GtkWidget *
create_view_and_model (void)
{

	GtkWidget 			*view;

	view 					= gtk_tree_view_new();
	treeview_column_icon	= gtk_tree_view_column_new();
	treeview_column_text	= gtk_tree_view_column_new();
	render_icon 			= gtk_cell_renderer_pixbuf_new();
	render_text 			= gtk_cell_renderer_text_new();

	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), FALSE);

	//gtk_tree_view_append_column(GTK_TREE_VIEW(view), treeview_column_icon);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), treeview_column_text);

	gtk_tree_view_column_pack_start(treeview_column_text, render_icon, FALSE);
	gtk_tree_view_column_set_attributes(treeview_column_text, render_icon, "stock-id", TREEBROWSER_RENDER_ICON, NULL);

	//gtk_tree_view_column_set_sort_order(treeview_column_text, GTK_SORT_ASCENDING);
	//gtk_tree_view_column_get_sort_column_id(treeview_column_text);
	//gtk_tree_view_column_get_clickable(treeview_column_text);

	gtk_tree_view_column_pack_start(treeview_column_text, render_text, TRUE);
	gtk_tree_view_column_add_attribute(treeview_column_text, render_text, "text", TREEBROWSER_RENDER_TEXT);

	treestore 	= gtk_tree_store_new(TREEBROWSER_COLUMNC,
										G_TYPE_STRING,
										G_TYPE_STRING,
										G_TYPE_STRING);

	gtk_tree_view_set_model(GTK_TREE_VIEW(view), GTK_TREE_MODEL(treestore));

	g_signal_connect(G_OBJECT(render_text), "edited", 	G_CALLBACK (on_renamed), view);

	return view;
}


void
plugin_init(GeanyData *data)
{
	GtkWidget 			*scrollwin;
	GtkTreeSelection 	*selection;

	treeview 			= create_view_and_model();
	vbox 				= gtk_vbox_new(FALSE, 0);
	vbox_bars 			= gtk_vbox_new(FALSE, 0);
	selection 			= gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	addressbar 			= gtk_entry_new();
	filter 				= gtk_entry_new();

	scrollwin 			= gtk_scrolled_window_new(NULL, NULL);

	page_number 		= gtk_notebook_append_page(GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook),
							vbox, gtk_label_new(_("Tree Browser")));

	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollwin), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	gtk_entry_set_icon_from_stock(GTK_ENTRY(filter), 			GTK_ENTRY_ICON_PRIMARY, 	GTK_STOCK_FIND);
	gtk_entry_set_icon_from_stock(GTK_ENTRY(addressbar), 		GTK_ENTRY_ICON_PRIMARY, 	GTK_STOCK_DIRECTORY);
	gtk_entry_set_icon_from_stock(GTK_ENTRY(addressbar), 		GTK_ENTRY_ICON_SECONDARY, 	GTK_STOCK_JUMP_TO);

	gtk_container_add(GTK_CONTAINER(scrollwin), 	treeview);

	gtk_box_pack_start(GTK_BOX(vbox_bars), 			filter, 			FALSE, TRUE,  1);
	gtk_box_pack_start(GTK_BOX(vbox_bars), 			addressbar, 		FALSE, TRUE,  1);

	gtk_box_pack_start(GTK_BOX(vbox), 				scrollwin, 			TRUE,  TRUE,  1);
	gtk_box_pack_start(GTK_BOX(vbox), 				vbox_bars, 			FALSE, TRUE,  1);

	g_signal_connect(selection, 		"changed", 				G_CALLBACK(on_changed), 				NULL);
	g_signal_connect(treeview, 			"button-press-event", 	G_CALLBACK(on_button_press), 			selection);
	g_signal_connect(treeview, 			"row-activated", 		G_CALLBACK(on_row_activated), 			NULL);
	g_signal_connect(treeview, 			"row-collapsed", 		G_CALLBACK(on_row_collapsed), 			NULL);
	g_signal_connect(treeview, 			"row-expanded", 		G_CALLBACK(on_row_expanded), 			NULL);
	g_signal_connect(addressbar, 		"activate", 			G_CALLBACK(on_addressbar_activate), 	NULL);
	g_signal_connect(addressbar, 		"icon-release",			G_CALLBACK(on_addressbar_icon), 		NULL);
	g_signal_connect(filter, 			"activate", 			G_CALLBACK(on_filter_activate), 		NULL);

	gtk_widget_show_all(vbox);

	//treebrowser_chroot("/tmp");
	treebrowser_chroot(get_default_dir());

	showbars(CONFIG_SHOW_BARS);
}


void
plugin_cleanup(void)
{
	gtk_widget_destroy(GTK_WIDGET(vbox));
}
