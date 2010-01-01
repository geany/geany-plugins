/*
 *      ao_markword.c - this file is part of Addons, a Geany plugin
 *
 *      Copyright 2009-2010 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
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
#include "ao_markword.h"


typedef struct _AoMarkWordPrivate			AoMarkWordPrivate;

#define AO_MARKWORD_GET_PRIVATE(obj)		(G_TYPE_INSTANCE_GET_PRIVATE((obj),\
			AO_MARKWORD_TYPE, AoMarkWordPrivate))

struct _AoMarkWord
{
	GObject parent;
};

struct _AoMarkWordClass
{
	GObjectClass parent_class;
};

struct _AoMarkWordPrivate
{
	gboolean enable_markword;
};

enum
{
	PROP_0,
	PROP_ENABLE_MARKWORD
};


static void ao_mark_word_finalize  			(GObject *object);

G_DEFINE_TYPE(AoMarkWord, ao_mark_word, G_TYPE_OBJECT);


static void ao_mark_word_set_property(GObject *object, guint prop_id,
										  const GValue *value, GParamSpec *pspec)
{
	AoMarkWordPrivate *priv = AO_MARKWORD_GET_PRIVATE(object);

	switch (prop_id)
	{
		case PROP_ENABLE_MARKWORD:
			priv->enable_markword = g_value_get_boolean(value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
			break;
	}
}


static void ao_mark_word_class_init(AoMarkWordClass *klass)
{
	GObjectClass *g_object_class;

	g_object_class = G_OBJECT_CLASS(klass);
	g_object_class->finalize = ao_mark_word_finalize;
	g_object_class->set_property = ao_mark_word_set_property;
	g_type_class_add_private(klass, sizeof(AoMarkWordPrivate));

	g_object_class_install_property(g_object_class,
									PROP_ENABLE_MARKWORD,
									g_param_spec_boolean(
									"enable-markword",
									"enable-markword",
									"Whether to mark all occurrences of a word when double-clicking it",
									TRUE,
									G_PARAM_WRITABLE));
}


static void ao_mark_word_finalize(GObject *object)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_AO_MARKWORD(object));

	G_OBJECT_CLASS(ao_mark_word_parent_class)->finalize(object);
}


void ao_mark_word_check(AoMarkWord *bm, GeanyEditor *editor, SCNotification *nt)
{
	AoMarkWordPrivate *priv = AO_MARKWORD_GET_PRIVATE(bm);

	if (priv->enable_markword)
	{
		switch (nt->nmhdr.code)
		{
			case SCN_DOUBLECLICK:
				keybindings_send_command(GEANY_KEY_GROUP_SEARCH, GEANY_KEYS_SEARCH_MARKALL);
				break;
		}
	}
}


static void ao_mark_word_init(AoMarkWord *self)
{
}


AoMarkWord *ao_mark_word_new(gboolean enable)
{
	return g_object_new(AO_MARKWORD_TYPE, "enable-markword", enable, NULL);
}
