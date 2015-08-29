/*
 *      ao_bookmarklist.c - this file is part of Addons, a Geany plugin
 *
 *      Copyright 2009-2011 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
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
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * $Id$
 */


#include <gtk/gtk.h>
#include <glib-object.h>

#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif
#include <geanyplugin.h>

#include "addons.h"
#include "ao_bookmarklist.h"

#include <gdk/gdkkeysyms.h>


typedef struct _AoBookmarkListPrivate			AoBookmarkListPrivate;

#define AO_BOOKMARK_LIST_GET_PRIVATE(obj)		(G_TYPE_INSTANCE_GET_PRIVATE((obj),\
			AO_BOOKMARK_LIST_TYPE, AoBookmarkListPrivate))

struct _AoBookmarkList
{
	GObject parent;
};

struct _AoBookmarkListClass
{
	GObjectClass parent_class;
};

struct _AoBookmarkListPrivate
{
	gboolean enable_bookmarklist;

	GtkWidget *page;
	GtkWidget *popup_menu;

	GtkListStore *store;
	GtkWidget *tree;

	gint		 search_line;
	GtkTreeIter	*search_iter;
};

enum
{
	PROP_0,
	PROP_ENABLE_BOOKMARKLIST
};

enum
{
	BMLIST_COL_LINE,
	BMLIST_COL_NAME,
	BMLIST_COL_TOOLTIP,
	BMLIST_COL_MAX
};

static void ao_bookmark_list_finalize  			(GObject *object);
static void ao_bookmark_list_show				(AoBookmarkList *bm);
static void ao_bookmark_list_hide				(AoBookmarkList *bm);

G_DEFINE_TYPE(AoBookmarkList, ao_bookmark_list, G_TYPE_OBJECT)


static void ao_bookmark_list_set_property(GObject *object, guint prop_id,
										  const GValue *value, GParamSpec *pspec)
{
	AoBookmarkListPrivate *priv = AO_BOOKMARK_LIST_GET_PRIVATE(object);

	switch (prop_id)
	{
		case PROP_ENABLE_BOOKMARKLIST:
		{
			gboolean new_val = g_value_get_boolean(value);
			if (new_val && ! priv->enable_bookmarklist)
				ao_bookmark_list_show(AO_BOOKMARK_LIST(object));
			if (! new_val && priv->enable_bookmarklist)
				ao_bookmark_list_hide(AO_BOOKMARK_LIST(object));

			priv->enable_bookmarklist = new_val;
			break;
		}
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
			break;
	}
}


static void ao_bookmark_list_class_init(AoBookmarkListClass *klass)
{
	GObjectClass *g_object_class;

	g_object_class = G_OBJECT_CLASS(klass);
	g_object_class->finalize = ao_bookmark_list_finalize;
	g_object_class->set_property = ao_bookmark_list_set_property;
	g_type_class_add_private(klass, sizeof(AoBookmarkListPrivate));

	g_object_class_install_property(g_object_class,
									PROP_ENABLE_BOOKMARKLIST,
									g_param_spec_boolean(
									"enable-bookmarklist",
									"enable-bookmarklist",
									"Whether to show a sidebar listing set bookmarks in the current doc",
									TRUE,
									G_PARAM_WRITABLE));
}


static void ao_bookmark_list_finalize(GObject *object)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_AO_BOOKMARK_LIST(object));

	ao_bookmark_list_hide(AO_BOOKMARK_LIST(object));

	G_OBJECT_CLASS(ao_bookmark_list_parent_class)->finalize(object);
}


static gboolean tree_model_foreach(GtkTreeModel *model, GtkTreePath *path,
								   GtkTreeIter *iter, gpointer data)
{
	gint x;
	AoBookmarkListPrivate *priv = AO_BOOKMARK_LIST_GET_PRIVATE(data);

	gtk_tree_model_get(model, iter, BMLIST_COL_LINE, &x, -1);
	if (x == priv->search_line)
	{
		priv->search_iter = gtk_tree_iter_copy(iter);
		return TRUE;
	}
	return FALSE;
}


static void delete_line(AoBookmarkList *bm, gint line_nr)
{
	AoBookmarkListPrivate *priv = AO_BOOKMARK_LIST_GET_PRIVATE(bm);

	priv->search_line = line_nr + 1;
	priv->search_iter = NULL;
	gtk_tree_model_foreach(GTK_TREE_MODEL(priv->store), tree_model_foreach, bm);
	if (priv->search_iter != NULL)
	{
		gtk_list_store_remove(priv->store, priv->search_iter);
		gtk_tree_iter_free(priv->search_iter);
	}
}


static void add_line(AoBookmarkList *bm, ScintillaObject *sci, gint line_nr)
{
	gchar *line, *tooltip;
	AoBookmarkListPrivate *priv = AO_BOOKMARK_LIST_GET_PRIVATE(bm);

	line = g_strstrip(sci_get_line(sci, line_nr));
	if (EMPTY(line))
		line = g_strdup(_("(Empty Line)"));
	tooltip = g_markup_escape_text(line, -1);

	gtk_list_store_insert_with_values(priv->store, NULL, -1,
		BMLIST_COL_LINE, line_nr + 1,
		BMLIST_COL_NAME, line,
		BMLIST_COL_TOOLTIP, tooltip,
		-1);
	g_free(line);
	g_free(tooltip);
}


static gboolean ao_selection_changed_cb(gpointer widget)
{
	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
	GtkTreeIter iter;
	GtkTreeModel *model;

	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		gint line;
		GeanyDocument *doc = document_get_current();
		if (DOC_VALID(doc))
		{
			gint pos;

			gtk_tree_model_get(model, &iter, BMLIST_COL_LINE, &line, -1);
			pos = sci_get_position_from_line(doc->editor->sci, line - 1);

			editor_goto_pos(doc->editor, pos, FALSE);
			gtk_widget_grab_focus(GTK_WIDGET(doc->editor->sci));
		}
	}
	return FALSE;
}


static gboolean ao_button_press_cb(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	if (event->button == 1)
	{	/* allow reclicking of a treeview item */
		g_idle_add(ao_selection_changed_cb, widget);
	}
	else if (event->button == 3)
	{
		AoBookmarkListPrivate *priv = AO_BOOKMARK_LIST_GET_PRIVATE(data);
		GtkTreeSelection *treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->tree));
		if (gtk_tree_selection_get_selected(treesel, NULL, NULL))
		{
			gtk_menu_popup(GTK_MENU(priv->popup_menu), NULL, NULL, NULL, NULL,
				event->button, event->time);
		}
		/* don't return TRUE here, unless the selection won't be changed */
	}
	return FALSE;
}


static gboolean ao_key_press_cb(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (event->keyval == GDK_Return ||
		event->keyval == GDK_ISO_Enter ||
		event->keyval == GDK_KP_Enter ||
		event->keyval == GDK_space)
	{
		g_idle_add(ao_selection_changed_cb, widget);
	}

	if ((event->keyval == GDK_F10 && event->state & GDK_SHIFT_MASK) || event->keyval == GDK_Menu)
	{
		GdkEventButton button_event;

		button_event.time = event->time;
		button_event.button = 3;

		ao_button_press_cb(widget, &button_event, data);
		return TRUE;
	}
	return FALSE;
}


static void ao_bookmark_list_hide(AoBookmarkList *bm)
{
	AoBookmarkListPrivate *priv = AO_BOOKMARK_LIST_GET_PRIVATE(bm);

	if (priv->page)
	{
		gtk_widget_destroy(priv->page);
		priv->page = NULL;
	}
	if (priv->popup_menu)
	{
		gtk_widget_destroy(priv->popup_menu);
		priv->popup_menu = NULL;
	}
}


static void popup_item_click_cb(GtkWidget *button, gpointer data)
{
	GtkTreeSelection *treesel;
	GtkTreeModel *model;
	GtkTreeIter iter;
	AoBookmarkListPrivate *priv = AO_BOOKMARK_LIST_GET_PRIVATE(data);

	treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->tree));

	if (gtk_tree_selection_get_selected(treesel, &model, &iter))
	{
		GeanyDocument *doc = document_get_current();
		gint line_nr;
		gtk_tree_model_get(model, &iter, BMLIST_COL_LINE, &line_nr, -1);
		sci_delete_marker_at_line(doc->editor->sci, line_nr - 1, 1);
	}
}


static GtkWidget *
gtk3_ui_image_menu_item_new(const gchar *icon_name, const gchar *label)
{
	GtkWidget *item = gtk_menu_item_new_with_mnemonic(label);
	GtkWidget *image = gtk_image_new_from_icon_name(icon_name, GTK_ICON_SIZE_MENU);

	gtk_container_add(GTK_CONTAINER(item), image);
	gtk_widget_show(image);
	return item;
}


static GtkWidget *create_popup_menu(AoBookmarkList *bm)
{
	GtkWidget *item, *menu;

	menu = gtk_menu_new();

	item = gtk3_ui_image_menu_item_new("edit-delete", _("_Remove Bookmark"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(popup_item_click_cb), bm);

	return menu;
}


static void ao_bookmark_list_show(AoBookmarkList *bm)
{
	GtkCellRenderer *text_renderer;
	GtkTreeViewColumn *column;
	GtkTreeView *tree;
	GtkListStore *store;
	GtkWidget *scrollwin;
	GtkTreeSortable *sortable;
	GeanyDocument *doc;
	AoBookmarkListPrivate *priv = AO_BOOKMARK_LIST_GET_PRIVATE(bm);

	tree = GTK_TREE_VIEW(gtk_tree_view_new());
	store = gtk_list_store_new(BMLIST_COL_MAX, G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING);
	gtk_tree_view_set_model(tree, GTK_TREE_MODEL(store));

	text_renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new();
	// Translators: Number is meant at this point.
	gtk_tree_view_column_set_title(column, _("No."));
	gtk_tree_view_column_pack_start(column, text_renderer, TRUE);
	gtk_tree_view_column_set_attributes(column, text_renderer, "text", BMLIST_COL_LINE, NULL);
	gtk_tree_view_append_column(tree, column);

	text_renderer = gtk_cell_renderer_text_new();
	g_object_set(text_renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("Contents"));
	gtk_tree_view_column_pack_start(column, text_renderer, TRUE);
	gtk_tree_view_column_set_attributes(column, text_renderer, "text", BMLIST_COL_NAME, NULL);
	gtk_tree_view_append_column(tree, column);
	gtk_tree_view_set_headers_visible(tree, TRUE);

	gtk_tree_view_set_search_column(tree, BMLIST_COL_NAME);

	/* sorting */
	sortable = GTK_TREE_SORTABLE(GTK_TREE_MODEL(store));
	gtk_tree_sortable_set_sort_column_id(sortable, BMLIST_COL_LINE, GTK_SORT_ASCENDING);

	ui_widget_modify_font_from_string(GTK_WIDGET(tree), geany->interface_prefs->tagbar_font);

	/* GTK 2.12 tooltips */
	if (gtk_check_version(2, 12, 0) == NULL)
		g_object_set(tree, "has-tooltip", TRUE, "tooltip-column", BMLIST_COL_TOOLTIP, NULL);

	/* selection handling */
	g_signal_connect(tree, "button-press-event", G_CALLBACK(ao_button_press_cb), bm);
	g_signal_connect(tree, "key-press-event", G_CALLBACK(ao_key_press_cb), bm);

	/* scrolled window */
	scrollwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollwin),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	gtk_container_add(GTK_CONTAINER(scrollwin), GTK_WIDGET(tree));

	gtk_widget_show_all(scrollwin);
	gtk_notebook_append_page(
		GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook),
		scrollwin,
		gtk_label_new(_("Bookmarks")));

	priv->popup_menu = create_popup_menu(bm);
	priv->store = store;
	priv->tree = GTK_WIDGET(tree);
	priv->page = scrollwin;

	/* initial update */
	doc = document_get_current();
	if (doc != NULL)
		ao_bookmark_list_update(bm, doc);
}


void ao_bookmark_list_activate(AoBookmarkList *bm)
{
	AoBookmarkListPrivate *priv = AO_BOOKMARK_LIST_GET_PRIVATE(bm);

	if (priv->enable_bookmarklist)
	{
		GtkNotebook *notebook = GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook);
		gint page_number = gtk_notebook_page_num(notebook, priv->page);

		gtk_notebook_set_current_page(notebook, page_number);
		gtk_widget_grab_focus(priv->tree);
	}
}


void ao_bookmark_list_update(AoBookmarkList *bm, GeanyDocument *doc)
{
	gint line_nr = 0;
	gint mask = 1 << 1;
	ScintillaObject *sci = doc->editor->sci;
	AoBookmarkListPrivate *priv = AO_BOOKMARK_LIST_GET_PRIVATE(bm);

	if (priv->enable_bookmarklist)
	{
		gtk_list_store_clear(priv->store);
		while ((line_nr = scintilla_send_message(sci, SCI_MARKERNEXT, line_nr, mask)) != -1)
		{
			add_line(bm, sci, line_nr);
			line_nr++;
		}
	}
}


void ao_bookmark_list_update_marker(AoBookmarkList *bm, GeanyEditor *editor, SCNotification *nt)
{
	AoBookmarkListPrivate *priv = AO_BOOKMARK_LIST_GET_PRIVATE(bm);

	if (priv->enable_bookmarklist &&
		nt->nmhdr.code == SCN_MODIFIED && nt->modificationType == SC_MOD_CHANGEMARKER)
	{
		if (sci_is_marker_set_at_line(editor->sci, nt->line, 1))
		{
			add_line(bm, editor->sci, nt->line);
		}
		else
		{
			delete_line(bm, nt->line);
		}
	}
}


static void ao_bookmark_list_init(AoBookmarkList *self)
{
	AoBookmarkListPrivate *priv = AO_BOOKMARK_LIST_GET_PRIVATE(self);

	priv->page = NULL;
}


AoBookmarkList *ao_bookmark_list_new(gboolean enable)
{
	return g_object_new(AO_BOOKMARK_LIST_TYPE, "enable-bookmarklist", enable, NULL);
}
