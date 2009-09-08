/*
 *  geanyprj - Alternative project support for geany light IDE.
 *
 *  Copyright 2008 Yura Siamashka <yurand2@gmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <sys/time.h>
#include <gdk/gdkkeysyms.h>
#include <string.h>

#include "geany.h"
#include "support.h"
#include "prefs.h"
#include "plugindata.h"
#include "document.h"
#include "filetypes.h"
#include "keybindings.h"
#include "utils.h"
#include "ui_utils.h"
#include "geanyfunctions.h"

#include "project.h"

#include "geanyprj.h"

extern GeanyData *geany_data;
extern GeanyFunctions *geany_functions;

static GtkWidget *file_view_vbox;
static GtkWidget *file_view;
static GtkListStore *file_store;

enum
{
	FILEVIEW_COLUMN_NAME = 0,
	FILEVIEW_N_COLUMNS,
};

static struct
{
	GtkWidget *new_project;
	GtkWidget *delete_project;

	GtkWidget *add_file;
	GtkWidget *remove_files;

	GtkWidget *preferences;

	GtkWidget *find_in_files;
} popup_items;


/* Returns: the full filename in locale encoding. */
static gchar *
get_tree_path_filename(GtkTreePath * treepath)
{
	GtkTreeModel *model = GTK_TREE_MODEL(file_store);
	GtkTreeIter iter;
	gchar *name;

	gtk_tree_model_get_iter(model, &iter, treepath);
	gtk_tree_model_get(model, &iter, FILEVIEW_COLUMN_NAME, &name, -1);
	setptr(name, utils_get_locale_from_utf8(name));
	setptr(name, get_full_path(g_current_project->path, name));
	return name;
}

/* We use documents->open_files() as it's more efficient. */
static void
open_selected_files(GList * list)
{
	GSList *files = NULL;
	GList *item;

	for (item = list; item != NULL; item = g_list_next(item))
	{
		GtkTreePath *treepath = item->data;
		gchar *fname = get_tree_path_filename(treepath);
		files = g_slist_append(files, fname);
	}
	document_open_files(files, FALSE, NULL, NULL);
	g_slist_foreach(files, (GFunc) g_free, NULL);	// free filenames
	g_slist_free(files);
}

static void
on_open_clicked(G_GNUC_UNUSED GtkMenuItem * menuitem, G_GNUC_UNUSED gpointer user_data)
{
	GtkTreeSelection *treesel;
	GtkTreeModel *model;
	GList *list;

	treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(file_view));

	list = gtk_tree_selection_get_selected_rows(treesel, &model);
	open_selected_files(list);
	g_list_foreach(list, (GFunc) gtk_tree_path_free, NULL);
	g_list_free(list);
}

static gboolean
on_button_press(G_GNUC_UNUSED GtkWidget * widget, GdkEventButton * event,
		G_GNUC_UNUSED gpointer user_data)
{
	if (event->button == 1 && event->type == GDK_2BUTTON_PRESS)
		on_open_clicked(NULL, NULL);
	return FALSE;
}

static GtkWidget *
make_toolbar()
{
	GtkWidget *toolbar;

	toolbar = gtk_toolbar_new();
	gtk_toolbar_set_icon_size(GTK_TOOLBAR(toolbar), GTK_ICON_SIZE_MENU);
	gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);

	return toolbar;
}

static void
remove_selected_files(GList * list)
{
	GList *item;
	for (item = list; item != NULL; item = g_list_next(item))
	{
		GtkTreePath *treepath = item->data;
		gchar *fname = get_tree_path_filename(treepath);
		xproject_remove_file(fname);
		g_free(fname);
	}
}

static void
on_remove_files(G_GNUC_UNUSED GtkMenuItem * menuitem, G_GNUC_UNUSED gpointer user_data)
{
	GtkTreeSelection *treesel;
	GtkTreeModel *model;
	GList *list;

	treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(file_view));

	list = gtk_tree_selection_get_selected_rows(treesel, &model);
	remove_selected_files(list);
	g_list_foreach(list, (GFunc) gtk_tree_path_free, NULL);
	g_list_free(list);
}

static gboolean
on_key_press(G_GNUC_UNUSED GtkWidget * widget, GdkEventKey * event, G_GNUC_UNUSED gpointer data)
{
	if (event->keyval == GDK_Return
	    || event->keyval == GDK_ISO_Enter
	    || event->keyval == GDK_KP_Enter || event->keyval == GDK_space)
		on_open_clicked(NULL, NULL);
	return FALSE;
}

static GtkWidget *
create_popup_menu()
{
	GtkWidget *item, *menu, *image;

	menu = gtk_menu_new();

	image = gtk_image_new_from_stock(GTK_STOCK_NEW, GTK_ICON_SIZE_MENU);
	gtk_widget_show(image);
	item = gtk_image_menu_item_new_with_mnemonic(_("New Project"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_new_project), NULL);
	popup_items.new_project = item;

	image = gtk_image_new_from_stock(GTK_STOCK_DELETE, GTK_ICON_SIZE_MENU);
	gtk_widget_show(image);
	item = gtk_image_menu_item_new_with_mnemonic(_("Delete Project"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_delete_project), NULL);
	popup_items.delete_project = item;

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);

	image = gtk_image_new_from_stock(GTK_STOCK_ADD, GTK_ICON_SIZE_MENU);
	gtk_widget_show(image);
	item = gtk_image_menu_item_new_with_mnemonic(_("Add File"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_add_file), NULL);
	popup_items.add_file = item;

	image = gtk_image_new_from_stock(GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU);
	gtk_widget_show(image);
	item = gtk_image_menu_item_new_with_mnemonic(_("Remove File"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_remove_files), NULL);
	popup_items.remove_files = item;

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);

	image = gtk_image_new_from_stock(GTK_STOCK_PREFERENCES, GTK_ICON_SIZE_MENU);
	gtk_widget_show(image);
	item = gtk_image_menu_item_new_with_mnemonic(_("Preferences"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_preferences), NULL);
	popup_items.preferences = item;

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);

	image = gtk_image_new_from_stock(GTK_STOCK_FIND, GTK_ICON_SIZE_MENU);
	gtk_widget_show(image);
	item = gtk_image_menu_item_new_with_mnemonic(_("Find in Project"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_find_in_project), NULL);
	popup_items.find_in_files = item;

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);

	item = gtk_image_menu_item_new_with_mnemonic(_("H_ide Sidebar"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item),
				      gtk_image_new_from_stock("gtk-close", GTK_ICON_SIZE_MENU));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect_swapped((gpointer) item, "activate",
				 G_CALLBACK(keybindings_send_command),
				 GINT_TO_POINTER(GEANY_KEYS_VIEW_SIDEBAR));

	return menu;
}

static void
update_popup_menu(G_GNUC_UNUSED GtkWidget * popup_menu)
{
	gboolean cur_file_exists;
	gboolean badd_file;
	GeanyDocument *doc;

	doc = document_get_current();

	cur_file_exists = doc && doc->file_name != NULL && g_path_is_absolute(doc->file_name);

	badd_file = (g_current_project ? TRUE : FALSE) &&
		!g_current_project->regenerate &&
		cur_file_exists && !g_hash_table_lookup(g_current_project->tags, doc->file_name);

	GtkTreeSelection *treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(file_view));
	gboolean bremove_file = (g_current_project ? TRUE : FALSE) &&
		!g_current_project->regenerate &&
		(gtk_tree_selection_count_selected_rows(treesel) > 0);

	gtk_widget_set_sensitive(popup_items.new_project, TRUE);
	gtk_widget_set_sensitive(popup_items.delete_project, g_current_project ? TRUE : FALSE);

	gtk_widget_set_sensitive(popup_items.add_file, badd_file);
	gtk_widget_set_sensitive(popup_items.remove_files, bremove_file);

	gtk_widget_set_sensitive(popup_items.preferences, g_current_project ? TRUE : FALSE);

	gtk_widget_set_sensitive(popup_items.find_in_files, g_current_project ? TRUE : FALSE);
}

// delay updating popup menu until the selection has been set
static gboolean
on_button_release(G_GNUC_UNUSED GtkWidget * widget, GdkEventButton * event,
		  G_GNUC_UNUSED gpointer user_data)
{
	if (event->button == 3)
	{
		static GtkWidget *popup_menu = NULL;

		if (popup_menu == NULL)
			popup_menu = create_popup_menu();

		update_popup_menu(popup_menu);

		gtk_menu_popup(GTK_MENU(popup_menu), NULL, NULL, NULL, NULL,
			       event->button, event->time);
	}
	return FALSE;
}

static void
prepare_file_view()
{
	GtkCellRenderer *text_renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;
	PangoFontDescription *pfd;

	file_store = gtk_list_store_new(FILEVIEW_N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING);

	gtk_tree_view_set_model(GTK_TREE_VIEW(file_view), GTK_TREE_MODEL(file_store));

	text_renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_pack_start(column, text_renderer, TRUE);
	gtk_tree_view_column_set_attributes(column, text_renderer, "text", FILEVIEW_COLUMN_NAME,
					    NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(file_view), column);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(file_view), FALSE);

	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(file_view), TRUE);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(file_view), FILEVIEW_COLUMN_NAME);

	pfd = pango_font_description_from_string(geany_data->interface_prefs->tagbar_font);
	gtk_widget_modify_font(file_view, pfd);
	pango_font_description_free(pfd);

	// selection handling
	select = gtk_tree_view_get_selection(GTK_TREE_VIEW(file_view));
	gtk_tree_selection_set_mode(select, GTK_SELECTION_SINGLE);

	g_signal_connect(G_OBJECT(file_view), "button-release-event",
			 G_CALLBACK(on_button_release), NULL);
	g_signal_connect(G_OBJECT(file_view), "button-press-event",
			 G_CALLBACK(on_button_press), NULL);

	g_signal_connect(G_OBJECT(file_view), "key-press-event", G_CALLBACK(on_key_press), NULL);
}

void
sidebar_clear()
{
	gtk_list_store_clear(file_store);
}

gint
mycmp(const gchar * a, const gchar * b)
{
	const gchar *p1 = a;
	const gchar *p2 = b;

	gint cnt1 = 0;
	gint cnt2 = 0;

	while (*p1)
	{
		if (*p1 == G_DIR_SEPARATOR_S[0])
			cnt1++;
		p1++;
	}

	while (*p2)
	{
		if (*p2 == G_DIR_SEPARATOR_S[0])
			cnt2++;
		p2++;
	}

	if (cnt1 != cnt2)
		return cnt2 - cnt1;

	p1 = a;
	p2 = b;

	while (*p1 && *p2)
	{
		if (*p1 != *p2)
		{
			if (*p1 == G_DIR_SEPARATOR_S[0])
				return -1;
			else if (*p2 == G_DIR_SEPARATOR_S[0])
				return 1;
			return *p1 - *p2;
		}
		p1++;
		p2++;
	}
	if (*p1 == 0 && *p2 == 0)
		return 0;
	else if (*p1)
		return 1;
	return -1;
}

static void
add_item(gpointer name, G_GNUC_UNUSED gpointer value, gpointer user_data)
{
	gchar *item;
	GSList **lst = (GSList **) user_data;

	item = get_relative_path(g_current_project->path, name);
	*lst = g_slist_prepend(*lst, item);
}

// recreate the tree model from current_dir.
void
sidebar_refresh()
{
	GtkTreeIter iter;
	GSList *lst = NULL;
	GSList *tmp;

	sidebar_clear();

	if (!g_current_project)
		return;

	g_hash_table_foreach(g_current_project->tags, add_item, &lst);
	lst = g_slist_sort(lst, (GCompareFunc) strcmp);
	for (tmp = lst; tmp != NULL; tmp = g_slist_next(tmp))
	{
		gtk_list_store_append(file_store, &iter);
		gtk_list_store_set(file_store, &iter, FILEVIEW_COLUMN_NAME, tmp->data, -1);
	}
	g_slist_foreach(lst, (GFunc) g_free, NULL);
	g_slist_free(lst);
}

void
create_sidebar()
{
	GtkWidget *scrollwin, *toolbar;

	file_view_vbox = gtk_vbox_new(FALSE, 0);
	toolbar = make_toolbar();
	gtk_box_pack_start(GTK_BOX(file_view_vbox), toolbar, FALSE, FALSE, 0);

	file_view = gtk_tree_view_new();
	prepare_file_view();

	scrollwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollwin),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(scrollwin), file_view);
	gtk_container_add(GTK_CONTAINER(file_view_vbox), scrollwin);

	gtk_widget_show_all(file_view_vbox);
	gtk_notebook_append_page(GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook),
				 file_view_vbox, gtk_label_new(_("Project")));
}

void
destroy_sidebar()
{
	gtk_widget_destroy(file_view_vbox);
}
