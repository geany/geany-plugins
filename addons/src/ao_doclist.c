/*
 *      ao_doclist.c - this file is part of Addons, a Geany plugin
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
#include "ao_doclist.h"


typedef struct _AoDocListPrivate			AoDocListPrivate;

#define AO_DOC_LIST_GET_PRIVATE(obj)		(G_TYPE_INSTANCE_GET_PRIVATE((obj),\
			AO_DOC_LIST_TYPE, AoDocListPrivate))

struct _AoDocList
{
	GObject parent;
};

struct _AoDocListClass
{
	GObjectClass parent_class;
};

struct _AoDocListPrivate
{
	gboolean enable_doclist;
	DocListSortMode sort_mode;
	GtkToolItem *toolbar_doclist_button;
	gboolean in_overflow_menu;
	GtkWidget *overflow_menu_item;
};

enum
{
	PROP_0,
	PROP_ENABLE_DOCLIST,
	PROP_SORT_MODE
};
enum
{
	ACTION_CLOSE_OTHER = 1,
	ACTION_CLOSE_ALL
};

static void ao_doc_list_finalize  			(GObject *object);
static void ao_doclist_set_property(GObject *object, guint prop_id,
									const GValue *value, GParamSpec *pspec);

G_DEFINE_TYPE(AoDocList, ao_doc_list, G_TYPE_OBJECT)


static void ao_doc_list_class_init(AoDocListClass *klass)
{
	GObjectClass *g_object_class;

	g_object_class = G_OBJECT_CLASS(klass);
	g_object_class->finalize = ao_doc_list_finalize;
	g_object_class->set_property = ao_doclist_set_property;

	g_type_class_add_private(klass, sizeof(AoDocListPrivate));

	g_object_class_install_property(g_object_class,
									PROP_ENABLE_DOCLIST,
									g_param_spec_boolean(
									"enable-doclist",
									"enable-doclist",
									"Whether to show a toolbar item to open a document list",
									TRUE,
									G_PARAM_WRITABLE));

	g_object_class_install_property(g_object_class,
									PROP_SORT_MODE,
									g_param_spec_int(
									"sort-mode",
									"sort-mode",
									"How to sort the documents in the list",
									0,
									G_MAXINT,
									DOCLIST_SORT_BY_TAB_ORDER,
									G_PARAM_WRITABLE));
}


static void ao_doc_list_finalize(GObject *object)
{
	AoDocListPrivate *priv = AO_DOC_LIST_GET_PRIVATE(object);

	if (priv->toolbar_doclist_button != NULL)
		gtk_widget_destroy(GTK_WIDGET(priv->toolbar_doclist_button));
	if (priv->overflow_menu_item != NULL)
		gtk_widget_destroy(priv->overflow_menu_item);

	G_OBJECT_CLASS(ao_doc_list_parent_class)->finalize(object);
}


/* This function is taken from Midori's katze-utils.c, thanks to Christian. */
static void ao_popup_position_menu(GtkMenu *menu, gint *x, gint *y, gboolean *push_in, gpointer data)
{
	AoDocListPrivate *priv = AO_DOC_LIST_GET_PRIVATE(data);

	gint wx, wy;
	GtkRequisition widget_req;
	GtkWidget *widget = GTK_WIDGET(priv->toolbar_doclist_button);
	GdkWindow *window;
	gint widget_height;

	if (priv->in_overflow_menu) {
		/* The button was added to the toolbar overflow menu (since the toolbar
		 * isn't wide enough to contain the button), so use the toolbar window
		 * instead
		 */

		/* Get the root coordinates of the toolbar's window */
		int wx_root, wy_root, width;
		window = gtk_widget_get_window (gtk_widget_get_ancestor(widget, GTK_TYPE_TOOLBAR));
		gdk_window_get_geometry(window, &wx, &wy, &width, NULL);
		gtk_widget_get_preferred_size(widget, &widget_req, NULL);
		gdk_window_get_root_coords(window, wx, wy, &wx_root, &wy_root);

		/* Approximate the horizontal location of the overflow menu button */
		/* TODO: See if there's a way to find the exact location */
		wx = wx_root + width - (int) (widget_req.width * 1.5);
		wy = wy_root;

		/* This will be set TRUE if the overflow menu is open again and includes
		 * the doclist menu item
		 */
		priv->in_overflow_menu = FALSE;
	} else {
		/* Retrieve size and position of both widget and menu */
		window = gtk_widget_get_window(widget);
		if (! gtk_widget_get_has_window(widget))
		{
			GtkAllocation allocation;
			gdk_window_get_position(window, &wx, &wy);
			gtk_widget_get_allocation(widget, &allocation);
			wx += allocation.x;
			wy += allocation.y;
		}
		else
			gdk_window_get_origin(window, &wx, &wy);
	}
	gtk_widget_get_preferred_size(widget, &widget_req, NULL);
	widget_height = widget_req.height; /* Better than allocation.height */

	/* Calculate menu position */
	*x = wx;
	*y = wy + widget_height;

	*push_in = TRUE;
}


static void ao_doclist_menu_item_activate_cb(GtkMenuItem *menuitem, gpointer data)
{
	GeanyDocument *doc = data;

	if (GPOINTER_TO_INT(data) == ACTION_CLOSE_OTHER)
	{
		GtkWidget *w = ui_lookup_widget(geany->main_widgets->window, "close_other_documents1");
		g_signal_emit_by_name(w, "activate");
		return;
	}
	else if (GPOINTER_TO_INT(data) == ACTION_CLOSE_ALL)
	{
		GtkWidget *w = ui_lookup_widget(geany->main_widgets->window, "menu_close_all1");
		g_signal_emit_by_name(w, "activate");
		return;
	}

	if (! DOC_VALID(doc))
		return;

	gtk_notebook_set_current_page(GTK_NOTEBOOK(geany->main_widgets->notebook),
		document_get_notebook_page(doc));
}


static void ao_toolbar_item_doclist_clicked_cb(GtkWidget *button, gpointer data)
{
	static GtkWidget *menu = NULL;
	GtkWidget *menu_item;
	GeanyDocument *current_doc = document_get_current();
	GCompareFunc compare_func;
	AoDocListPrivate *priv = AO_DOC_LIST_GET_PRIVATE(data);

	if (menu != NULL)
		gtk_widget_destroy(menu);

	menu = gtk_menu_new();

	switch (priv->sort_mode)
	{
		case DOCLIST_SORT_BY_NAME:
			compare_func = document_compare_by_display_name;
			break;
		case DOCLIST_SORT_BY_TAB_ORDER_REVERSE:
			compare_func = document_compare_by_tab_order_reverse;
			break;
		case DOCLIST_SORT_BY_TAB_ORDER:
		default:
			compare_func = document_compare_by_tab_order;
			break;
	}

	ui_menu_add_document_items_sorted(GTK_MENU(menu), current_doc,
		G_CALLBACK(ao_doclist_menu_item_activate_cb), compare_func);

	menu_item = gtk_separator_menu_item_new();
	gtk_widget_show(menu_item);
	gtk_container_add(GTK_CONTAINER(menu), menu_item);

	menu_item = ui_image_menu_item_new(GTK_STOCK_CLOSE, _("Close Ot_her Documents"));
	gtk_widget_show(menu_item);
	gtk_container_add(GTK_CONTAINER(menu), menu_item);
	g_signal_connect(menu_item, "activate", G_CALLBACK(ao_doclist_menu_item_activate_cb),
		GINT_TO_POINTER(ACTION_CLOSE_OTHER));
	menu_item = ui_image_menu_item_new(GTK_STOCK_CLOSE, _("C_lose All"));
	gtk_widget_show(menu_item);
	gtk_container_add(GTK_CONTAINER(menu), menu_item);
	g_signal_connect(menu_item, "activate", G_CALLBACK(ao_doclist_menu_item_activate_cb),
		GINT_TO_POINTER(ACTION_CLOSE_ALL));

	gtk_menu_popup(GTK_MENU(menu), NULL, NULL,
		ao_popup_position_menu, data, 0, gtk_get_current_event_time());
}


static gboolean ao_create_proxy_menu_cb(GtkToolItem *widget, gpointer data)
{
	AoDocListPrivate *priv = AO_DOC_LIST_GET_PRIVATE(data);

	/* Create the menu item if needed */
	if (priv->overflow_menu_item == NULL)
	{
		priv->overflow_menu_item = gtk_menu_item_new_with_label("Document List");
		g_signal_connect(priv->overflow_menu_item, "activate",
			G_CALLBACK(ao_toolbar_item_doclist_clicked_cb), data);
	}

	gtk_tool_item_set_proxy_menu_item(priv->toolbar_doclist_button, "doc-list-menu-item",
		priv->overflow_menu_item);
	priv->in_overflow_menu = TRUE;

	return TRUE;
}


static void ao_toolbar_update(AoDocList *self)
{
	AoDocListPrivate *priv = AO_DOC_LIST_GET_PRIVATE(self);

	/* toolbar item is not requested, so remove the item if it exists */
	if (! priv->enable_doclist)
	{
		if (priv->toolbar_doclist_button != NULL)
		{
			gtk_widget_hide(GTK_WIDGET(priv->toolbar_doclist_button));
		}
	}
	else
	{
		if (priv->toolbar_doclist_button == NULL)
		{
			priv->toolbar_doclist_button = gtk_tool_button_new_from_stock(GTK_STOCK_INDEX);
			gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(priv->toolbar_doclist_button),
				_("Show Document List"));
			plugin_add_toolbar_item(geany_plugin, priv->toolbar_doclist_button);
			ui_add_document_sensitive(GTK_WIDGET(priv->toolbar_doclist_button));

			g_signal_connect(priv->toolbar_doclist_button, "clicked",
				G_CALLBACK(ao_toolbar_item_doclist_clicked_cb), self);

			g_signal_connect(priv->toolbar_doclist_button, "create-menu-proxy",
			    G_CALLBACK(ao_create_proxy_menu_cb), self);

		}
		gtk_widget_show(GTK_WIDGET(priv->toolbar_doclist_button));
	}
}


static void ao_doclist_set_property(GObject *object, guint prop_id,
									const GValue *value, GParamSpec *pspec)
{
	AoDocListPrivate *priv = AO_DOC_LIST_GET_PRIVATE(object);

	switch (prop_id)
	{
		case PROP_ENABLE_DOCLIST:
			priv->enable_doclist = g_value_get_boolean(value);
			ao_toolbar_update(AO_DOC_LIST(object));
			break;
		case PROP_SORT_MODE:
			priv->sort_mode = g_value_get_int(value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
			break;
	}
}


static void ao_doc_list_init(AoDocList *self)
{
	AoDocListPrivate *priv = AO_DOC_LIST_GET_PRIVATE(self);

	priv->toolbar_doclist_button = NULL;
	priv->in_overflow_menu = FALSE;
	priv->overflow_menu_item = NULL;
}


AoDocList *ao_doc_list_new(gboolean enable, DocListSortMode sort_mode)
{
	return g_object_new(AO_DOC_LIST_TYPE, "enable-doclist", enable, "sort-mode", sort_mode, NULL);
}


