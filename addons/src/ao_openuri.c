/*
 *      ao_openuri.c - this file is part of Addons, a Geany plugin
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

#include <glib-object.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif
#include <geanyplugin.h>

#include "addons.h"
#include "ao_openuri.h"


typedef struct _AoOpenUriPrivate			AoOpenUriPrivate;

#define AO_OPEN_URI_GET_PRIVATE(obj)		(G_TYPE_INSTANCE_GET_PRIVATE((obj),\
			AO_OPEN_URI_TYPE, AoOpenUriPrivate))

struct _AoOpenUri
{
	GObject parent;
};

struct _AoOpenUriClass
{
	GObjectClass parent_class;
};

struct _AoOpenUriPrivate
{
	gboolean	 enable_openuri;
	gchar 		*uri;

	GtkWidget	*menu_item_open;
	GtkWidget	*menu_item_copy;
	GtkWidget	*menu_item_sep;
};

enum
{
	PROP_0,
	PROP_ENABLE_OPENURI
};


static void ao_open_uri_finalize  			(GObject *object);
static void ao_open_uri_set_property		(GObject *object, guint prop_id,
											 const GValue *value, GParamSpec *pspec);


G_DEFINE_TYPE(AoOpenUri, ao_open_uri, G_TYPE_OBJECT)



static void ao_open_uri_class_init(AoOpenUriClass *klass)
{
	GObjectClass *g_object_class;

	g_object_class = G_OBJECT_CLASS(klass);

	g_object_class->finalize = ao_open_uri_finalize;
	g_object_class->set_property = ao_open_uri_set_property;

	g_type_class_add_private(klass, sizeof(AoOpenUriPrivate));

	g_object_class_install_property(g_object_class,
									PROP_ENABLE_OPENURI,
									g_param_spec_boolean(
									"enable-openuri",
									"enable-openuri",
									"Whether to show a menu item in the editor menu to open URIs",
									FALSE,
									G_PARAM_WRITABLE));
}


static void ao_open_uri_set_property(GObject *object, guint prop_id,
									 const GValue *value, GParamSpec *pspec)
{
	AoOpenUriPrivate *priv = AO_OPEN_URI_GET_PRIVATE(object);

	switch (prop_id)
	{
		case PROP_ENABLE_OPENURI:
			priv->enable_openuri = g_value_get_boolean(value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
			break;
	}
}


static void ao_open_uri_finalize(GObject *object)
{
	AoOpenUriPrivate *priv = AO_OPEN_URI_GET_PRIVATE(object);

	g_free(priv->uri);
	gtk_widget_destroy(priv->menu_item_open);
	gtk_widget_destroy(priv->menu_item_copy);
	gtk_widget_destroy(priv->menu_item_sep);

	G_OBJECT_CLASS(ao_open_uri_parent_class)->finalize(object);
}


static void ao_menu_open_activate_cb(GtkMenuItem *item, AoOpenUri *self)
{
	AoOpenUriPrivate *priv = AO_OPEN_URI_GET_PRIVATE(self);

	if (!EMPTY(priv->uri))
		utils_open_browser(priv->uri);

}


static void ao_menu_copy_activate_cb(GtkMenuItem *item, AoOpenUri *self)
{
	AoOpenUriPrivate *priv = AO_OPEN_URI_GET_PRIVATE(self);

	if (!EMPTY(priv->uri))
		gtk_clipboard_set_text(gtk_clipboard_get(gdk_atom_intern("CLIPBOARD", FALSE)), priv->uri, -1);
}


static const gchar *ao_find_icon_name(const gchar *request, const gchar *fallback)
{
	GtkIconTheme *theme = gtk_icon_theme_get_default();

	if (gtk_icon_theme_has_icon(theme, request))
		return request;
	else
		return fallback;
}


static void ao_open_uri_init(AoOpenUri *self)
{
	AoOpenUriPrivate *priv = AO_OPEN_URI_GET_PRIVATE(self);

	priv->uri = NULL;

	priv->menu_item_open = ao_image_menu_item_new(
		ao_find_icon_name("text-html", "document-new"), _("Open URI"));
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->editor_menu), priv->menu_item_open);
	gtk_menu_reorder_child(GTK_MENU(geany->main_widgets->editor_menu), priv->menu_item_open, 0);
	gtk_widget_hide(priv->menu_item_open);
	g_signal_connect(priv->menu_item_open, "activate", G_CALLBACK(ao_menu_open_activate_cb), self);

	priv->menu_item_copy = ao_image_menu_item_new("edit-copy", _("Copy URI"));
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->editor_menu), priv->menu_item_copy);
	gtk_menu_reorder_child(GTK_MENU(geany->main_widgets->editor_menu), priv->menu_item_copy, 1);
	gtk_widget_hide(priv->menu_item_copy);
	g_signal_connect(priv->menu_item_copy, "activate", G_CALLBACK(ao_menu_copy_activate_cb), self);

	priv->menu_item_sep = gtk_separator_menu_item_new();
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->editor_menu), priv->menu_item_sep);
	gtk_menu_reorder_child(GTK_MENU(geany->main_widgets->editor_menu), priv->menu_item_sep, 2);

}


AoOpenUri *ao_open_uri_new(gboolean enable)
{
	return g_object_new(AO_OPEN_URI_TYPE, "enable-openuri", enable, NULL);
}


static gboolean ao_uri_is_link(const gchar  *uri)
{
	gchar *dot;

	g_return_val_if_fail (uri != NULL, FALSE);

	if ((dot = strchr(uri, '.')) && *dot != '\0')
	{	/* we require two dots and don't allow any spaces (www.domain.tld)
		 * unless we get too many matches */
		return (strchr(dot + 1, '.') && ! strchr(uri, ' '));
	}
	return FALSE;
}


/* based on g_uri_parse_scheme() but only checks for a scheme, doesn't return it */
static gboolean ao_uri_has_scheme(const gchar  *uri)
{
	const gchar *p;
	gchar c;

	g_return_val_if_fail (uri != NULL, FALSE);

	p = uri;

	if (! g_ascii_isalpha(*p))
		return FALSE;

	while (1)
	{
		c = *p++;

		if (c == ':' && strncmp(p, "//", 2) == 0)
			return TRUE;

		if (! (g_ascii_isalnum(c) || c == '+' || c == '-' || c == '.'))
			return FALSE;
	}
	return FALSE;
}


void ao_open_uri_update_menu(AoOpenUri *openuri, GeanyDocument *doc, gint pos)
{
	gchar *text;
	AoOpenUriPrivate *priv;

	g_return_if_fail(openuri != NULL);
	g_return_if_fail(doc != NULL);

	priv = AO_OPEN_URI_GET_PRIVATE(openuri);

	if (! priv->enable_openuri)
		return;

	/* if we have a selection, prefer it over the current word */
	if (sci_has_selection(doc->editor->sci))
	{
		gint len = sci_get_selected_text_length(doc->editor->sci);
		text = g_malloc0((guint)len + 1);
		sci_get_selected_text(doc->editor->sci, text);
	}
	else
		text = editor_get_word_at_pos(doc->editor, pos, GEANY_WORDCHARS"@.://-?&%#=~+,;");

	/* TODO be more restrictive when handling selections as there are too many hits by now */
	if (text != NULL && (ao_uri_has_scheme(text) || ao_uri_is_link(text)))
	{
		gsize len = strlen(text);
		/* remove trailing dots and colons */
		if (text[len - 1] == '.' || text[len - 1] == ':')
			text[len - 1] = '\0';

		setptr(priv->uri, text);

		gtk_widget_show(priv->menu_item_open);
		gtk_widget_show(priv->menu_item_copy);
		gtk_widget_show(priv->menu_item_sep);
	}
	else
	{
		g_free(text);

		gtk_widget_hide(priv->menu_item_open);
		gtk_widget_hide(priv->menu_item_copy);
		gtk_widget_hide(priv->menu_item_sep);
	}
}


