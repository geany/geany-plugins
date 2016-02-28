/*
 *      ao_copyfilepath.c - this file is part of Addons, a Geany plugin
 *
 *      Copyright 2015 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
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
 */

#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif
#include <geanyplugin.h>

#include "addons.h"
#include "ao_copyfilepath.h"


typedef struct _AoCopyFilePathPrivate			AoCopyFilePathPrivate;

#define AO_COPY_FILE_PATH_GET_PRIVATE(obj)		(G_TYPE_INSTANCE_GET_PRIVATE((obj),\
			AO_COPY_FILE_PATH_TYPE, AoCopyFilePathPrivate))

struct _AoCopyFilePath
{
	GObject parent;
};

struct _AoCopyFilePathClass
{
	GObjectClass parent_class;
};

struct _AoCopyFilePathPrivate
{
	GtkWidget *tools_menu_item;
};



static void ao_copy_file_path_finalize(GObject *object);


G_DEFINE_TYPE(AoCopyFilePath, ao_copy_file_path, G_TYPE_OBJECT)



static void ao_copy_file_path_class_init(AoCopyFilePathClass *klass)
{
	GObjectClass *g_object_class;

	g_object_class = G_OBJECT_CLASS(klass);

	g_object_class->finalize = ao_copy_file_path_finalize;

	g_type_class_add_private(klass, sizeof(AoCopyFilePathPrivate));
}


static void ao_copy_file_path_finalize(GObject *object)
{
	AoCopyFilePathPrivate *priv = AO_COPY_FILE_PATH_GET_PRIVATE(object);

	gtk_widget_destroy(priv->tools_menu_item);

	G_OBJECT_CLASS(ao_copy_file_path_parent_class)->finalize(object);
}


GtkWidget* ao_copy_file_path_get_menu_item(AoCopyFilePath *self)
{
	AoCopyFilePathPrivate *priv = AO_COPY_FILE_PATH_GET_PRIVATE(self);

	return priv->tools_menu_item;
}


void ao_copy_file_path_copy(AoCopyFilePath *self)
{
	GeanyDocument *doc;
	GtkClipboard *clipboard, *primary;

	doc = document_get_current();
	if (doc != NULL)
	{
		gchar *file_name = DOC_FILENAME(doc);
		clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
		primary = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
		gtk_clipboard_set_text(clipboard, file_name, -1);
		gtk_clipboard_set_text(primary, file_name, -1);

		ui_set_statusbar(TRUE, _("File path \"%s\" copied to clipboard."), file_name);
	}
}


static void copy_file_path_activated_cb(GtkMenuItem *item, AoCopyFilePath *self)
{
	ao_copy_file_path_copy(self);
}


static void ao_copy_file_path_init(AoCopyFilePath *self)
{
	AoCopyFilePathPrivate *priv = AO_COPY_FILE_PATH_GET_PRIVATE(self);

	priv->tools_menu_item = ui_image_menu_item_new(GTK_STOCK_COPY, _("Copy File Path"));
	gtk_widget_set_tooltip_text(
		priv->tools_menu_item,
		_("Copy the file path of the current document to the clipboard"));
	gtk_widget_show(priv->tools_menu_item);
	g_signal_connect(
		priv->tools_menu_item,
		"activate",
		G_CALLBACK(copy_file_path_activated_cb),
		self);
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu), priv->tools_menu_item);

	ui_add_document_sensitive(priv->tools_menu_item);
}


AoCopyFilePath *ao_copy_file_path_new(void)
{
	return g_object_new(AO_COPY_FILE_PATH_TYPE, NULL);
}
