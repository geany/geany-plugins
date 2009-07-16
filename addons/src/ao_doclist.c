/*
 *      ao_doclist.c - this file is part of Addons, a Geany plugin
 *
 *      Copyright 2009 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
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

#include "geanyplugin.h"

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
	GtkToolItem *toolbar_doclist_button;
};

enum
{
	PROP_0,
	PROP_ENABLE_DOCLIST,
};
enum
{
	ACTION_CLOSE_OTHER = 1,
	ACTION_CLOSE_ALL
};

static void ao_doc_list_finalize  			(GObject *object);
static void ao_doclist_set_property(GObject *object, guint prop_id,
									const GValue *value, GParamSpec *pspec);

G_DEFINE_TYPE(AoDocList, ao_doc_list, G_TYPE_OBJECT);


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
}


static void ao_doc_list_finalize(GObject *object)
{
	AoDocListPrivate *priv = AO_DOC_LIST_GET_PRIVATE(object);

	if (priv->toolbar_doclist_button != NULL)
		gtk_widget_destroy(GTK_WIDGET(priv->toolbar_doclist_button));

	G_OBJECT_CLASS(ao_doc_list_parent_class)->finalize(object);
}


/* This function is taken from Midori's katze-utils.c, thanks to Christian. */
static void ao_popup_position_menu(GtkMenu *menu, gint *x, gint *y, gboolean *push_in, gpointer data)
{
	gint wx, wy;
	gint menu_width;
	GtkRequisition menu_req;
	GtkRequisition widget_req;
	GtkWidget *widget = data;
	gint widget_height;

	/* Retrieve size and position of both widget and menu */
	if (GTK_WIDGET_NO_WINDOW(widget))
	{
		gdk_window_get_position(widget->window, &wx, &wy);
		wx += widget->allocation.x;
		wy += widget->allocation.y;
	}
	else
		gdk_window_get_origin(widget->window, &wx, &wy);
	gtk_widget_size_request(GTK_WIDGET(menu), &menu_req);
	gtk_widget_size_request(widget, &widget_req);
	menu_width = menu_req.width;
	widget_height = widget_req.height; /* Better than allocation.height */

	/* Calculate menu position */
	*x = wx;
	*y = wy + widget_height;

	*push_in = TRUE;
}


static void ao_doclist_menu_item_activate_cb(GtkMenuItem *menuitem, gpointer data)
{
	GeanyDocument *doc = data;
	gchar *locale_filename;

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

	locale_filename = utils_get_locale_from_utf8(doc->file_name);
	/* Go the easy way and let document_open_file() handle finding the right document and
	 * switch the notebook page accordingly. */
	document_open_file(locale_filename, FALSE, NULL, NULL);

	g_free(locale_filename);
}


static void ao_toolbar_item_doclist_clicked_cb(GtkWidget *button, gpointer data)
{
	static GtkWidget *menu = NULL;
	GtkWidget *menu_item;
	GtkWidget *menu_item_label;
	guint i;
	gchar *base_name;
	const GdkColor *color;
	GeanyDocument *doc;
	GeanyDocument *current_doc = document_get_current();

	if (menu != NULL)
		gtk_widget_destroy(menu);

	menu = gtk_menu_new();

	for (i = 0; i < geany->documents_array->len; i++)
	{
		doc = document_index(i);
		if (! DOC_VALID(doc))
			continue;

		base_name = g_path_get_basename(DOC_FILENAME(doc));
		menu_item = gtk_menu_item_new_with_label(base_name);
		gtk_widget_show(menu_item);
		gtk_container_add(GTK_CONTAINER(menu), menu_item);
		g_signal_connect(menu_item, "activate", G_CALLBACK(ao_doclist_menu_item_activate_cb), doc);

		color = document_get_status_color(doc);
		menu_item_label = gtk_bin_get_child(GTK_BIN(menu_item));
		gtk_widget_modify_fg(menu_item_label, GTK_STATE_NORMAL, color);
		gtk_widget_modify_fg(menu_item_label, GTK_STATE_ACTIVE, color);
		if (doc == current_doc)
		{
			setptr(base_name, g_strconcat("<b>", base_name, "</b>", NULL));
			gtk_label_set_markup(GTK_LABEL(menu_item_label), base_name);
		}
		g_free(base_name);
	}
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
		ao_popup_position_menu, button, 0, gtk_get_current_event_time());
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
#if GTK_CHECK_VERSION(2, 12, 0)
			gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(priv->toolbar_doclist_button),
				_("Show Document List"));
#endif
			plugin_add_toolbar_item(geany_plugin, priv->toolbar_doclist_button);
			ui_add_document_sensitive(GTK_WIDGET(priv->toolbar_doclist_button));

			g_signal_connect(priv->toolbar_doclist_button, "clicked",
				G_CALLBACK(ao_toolbar_item_doclist_clicked_cb), NULL);
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
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
			break;
	}
}


static void ao_doc_list_init(AoDocList *self)
{
	AoDocListPrivate *priv = AO_DOC_LIST_GET_PRIVATE(self);

	priv->toolbar_doclist_button = NULL;
}


AoDocList *ao_doc_list_new(gboolean enable)
{
	return g_object_new(AO_DOC_LIST_TYPE, "enable-doclist", enable, NULL);
}


