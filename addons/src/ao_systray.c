/*
 *      ao_systray.c - this file is part of Addons, a Geany plugin
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
 */


#include "geany.h"
#include "support.h"

#include "ui_utils.h"

#include "plugindata.h"
#include "geanyfunctions.h"

#include "addons.h"
#include "ao_systray.h"


typedef struct _AoSystrayPrivate			AoSystrayPrivate;

#define AO_SYSTRAY_GET_PRIVATE(obj)		(G_TYPE_INSTANCE_GET_PRIVATE((obj),\
			AO_SYSTRAY_TYPE, AoSystrayPrivate))

struct _AoSystray
{
	GObject parent;
};

struct _AoSystrayClass
{
	GObjectClass parent_class;
};

struct _AoSystrayPrivate
{
	gboolean enable_systray;

	GtkWidget *popup_menu;
#if GTK_CHECK_VERSION(2, 10, 0)
	GtkStatusIcon *icon;
#endif
};

enum
{
	PROP_0,
	PROP_ENABLE_SYSTRAY,
};

G_DEFINE_TYPE(AoSystray, ao_systray, G_TYPE_OBJECT);


static void ao_systray_finalize(GObject *object)
{
#if GTK_CHECK_VERSION(2, 10, 0)
	AoSystrayPrivate *priv = AO_SYSTRAY_GET_PRIVATE(object);

	g_object_unref(priv->icon);
	gtk_widget_destroy(priv->popup_menu);
#endif

	G_OBJECT_CLASS(ao_systray_parent_class)->finalize(object);
}


static void ao_systray_set_property(GObject *object, guint prop_id,
									const GValue *value, GParamSpec *pspec)
{
	AoSystrayPrivate *priv = AO_SYSTRAY_GET_PRIVATE(object);

	switch (prop_id)
	{
		case PROP_ENABLE_SYSTRAY:
			priv->enable_systray = g_value_get_boolean(value);
#if GTK_CHECK_VERSION(2, 10, 0)
			gtk_status_icon_set_visible(priv->icon, priv->enable_systray);
#endif
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
			break;
	}
}


static void ao_systray_class_init(AoSystrayClass *klass)
{
	GObjectClass *g_object_class;

	g_object_class = G_OBJECT_CLASS(klass);

	g_object_class->finalize = ao_systray_finalize;
	g_object_class->set_property = ao_systray_set_property;

	g_type_class_add_private(klass, sizeof(AoSystrayPrivate));

	g_object_class_install_property(g_object_class,
									PROP_ENABLE_SYSTRAY,
									g_param_spec_boolean(
									"enable-systray",
									"enable-systray",
									"Whether to show an icon in the notification area",
									TRUE,
									G_PARAM_WRITABLE));
}


#if GTK_CHECK_VERSION(2, 10, 0)
static void icon_activate_cb(GtkStatusIcon *status_icon, gpointer data)
{
	if (gtk_window_is_active(GTK_WINDOW(geany->main_widgets->window)))
		gtk_widget_hide(geany->main_widgets->window);
	else
		gtk_window_present(GTK_WINDOW(geany->main_widgets->window));
}


static void icon_popup_menu_cmd_clicked_cb(GtkMenuItem *item, gpointer data)
{
	g_signal_emit_by_name(ui_lookup_widget(geany->main_widgets->window, data), "activate");
}


static gboolean icon_quit_cb(gpointer data)
{
	g_signal_emit_by_name(geany->main_widgets->window, "delete-event", NULL);

	return FALSE;
}


static void icon_popup_quit_clicked_cb(GtkMenuItem *item, gpointer data)
{
	/* We need to delay emitting the "delete-event" signal a bit to give GTK a chance to finish
	 * processing this signal otherwise we would crash. */
	g_idle_add(icon_quit_cb, NULL);
}


static void icon_popup_menu_cb(GtkStatusIcon *status_icon, guint button, guint activate_time,
							   gpointer data)
{
	AoSystrayPrivate *priv = AO_SYSTRAY_GET_PRIVATE(data);

	if (button == 3)
		gtk_menu_popup(GTK_MENU(priv->popup_menu), NULL, NULL, NULL, NULL, button, activate_time);
}
#endif


static void ao_systray_init(AoSystray *self)
{
#if GTK_CHECK_VERSION(2, 10, 0)
	AoSystrayPrivate *priv = AO_SYSTRAY_GET_PRIVATE(self);
	GtkWidget *item;

	priv->icon = gtk_status_icon_new_from_pixbuf(gtk_window_get_icon(
		GTK_WINDOW(geany->main_widgets->window)));

#if GTK_CHECK_VERSION(2, 16, 0)
	gtk_status_icon_set_tooltip_text(priv->icon, "Geany");
#else
	gtk_status_icon_set_tooltip(priv->icon, "Geany");
#endif

	priv->popup_menu = gtk_menu_new();

	item = gtk_image_menu_item_new_from_stock(GTK_STOCK_OPEN, NULL);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(priv->popup_menu), item);
	g_signal_connect(item, "activate",
		G_CALLBACK(icon_popup_menu_cmd_clicked_cb), "menu_open1");

	item = gtk_image_menu_item_new_from_stock(GEANY_STOCK_SAVE_ALL, NULL);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(priv->popup_menu), item);
	g_signal_connect(item, "activate",
		G_CALLBACK(icon_popup_menu_cmd_clicked_cb), "menu_save_all1");

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(priv->popup_menu), item);

	item = gtk_image_menu_item_new_from_stock(GTK_STOCK_PREFERENCES, NULL);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(priv->popup_menu), item);
	g_signal_connect(item, "activate",
		G_CALLBACK(icon_popup_menu_cmd_clicked_cb), "preferences1");

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(priv->popup_menu), item);

	item = gtk_image_menu_item_new_from_stock(GTK_STOCK_QUIT, NULL);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(priv->popup_menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(icon_popup_quit_clicked_cb), NULL);

	g_signal_connect(priv->icon, "activate", G_CALLBACK(icon_activate_cb), NULL);
	g_signal_connect(priv->icon, "popup-menu", G_CALLBACK(icon_popup_menu_cb), self);
#endif
}


AoSystray *ao_systray_new(gboolean enable)
{
	return g_object_new(AO_SYSTRAY_TYPE, "enable-systray", enable, NULL);
}

