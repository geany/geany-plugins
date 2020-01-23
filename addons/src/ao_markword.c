/*
 *      ao_markword.c - this file is part of Addons, a Geany plugin
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

#include <geanyplugin.h>

#include "addons.h"
#include "ao_markword.h"


typedef struct _AoMarkWordPrivate			AoMarkWordPrivate;

#define DOUBLE_CLICK_DELAY 50


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
	gboolean enable_single_click_deselect;

	guint double_click_timer_id;
};

enum
{
	PROP_0,
	PROP_ENABLE_MARKWORD,
	PROP_ENABLE_MARKWORD_SINGLE_CLICK_DESELECT
};


static void ao_mark_word_finalize  			(GObject *object);
static void connect_documents_button_press_signal_handler(AoMarkWord *mw);

G_DEFINE_TYPE(AoMarkWord, ao_mark_word, G_TYPE_OBJECT)


static void ao_mark_word_set_property(GObject *object, guint prop_id,
										  const GValue *value, GParamSpec *pspec)
{
	AoMarkWordPrivate *priv = AO_MARKWORD_GET_PRIVATE(object);

	switch (prop_id)
	{
		case PROP_ENABLE_MARKWORD:
			priv->enable_markword = g_value_get_boolean(value);
			/* if the plugin is loaded while Geany is already running, we need to connect the
			 * button press signal for open documents, if Geany is just booting,
			 * it happens automatically */
			if (priv->enable_markword && main_is_realized())
			{
				connect_documents_button_press_signal_handler(AO_MARKWORD(object));
			}
			break;
		case PROP_ENABLE_MARKWORD_SINGLE_CLICK_DESELECT:
			priv->enable_single_click_deselect = g_value_get_boolean(value);
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

	g_object_class_install_property(g_object_class,
									PROP_ENABLE_MARKWORD_SINGLE_CLICK_DESELECT,
									g_param_spec_boolean(
									"enable-single-click-deselect",
									"enable-single-click-deselect",
									"Enable deselecting a previous highlight by single click",
									TRUE,
									G_PARAM_WRITABLE));
}


static void ao_mark_word_finalize(GObject *object)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_AO_MARKWORD(object));

	G_OBJECT_CLASS(ao_mark_word_parent_class)->finalize(object);
}


static void clear_marker(void)
{
	/* There might be a small risk that the current document is not the one we want
	 * but this is very unlikely. */
	GeanyDocument *document = document_get_current();
	/* clear current search indicators */
	if (DOC_VALID(document))
		editor_indicator_clear(document->editor, GEANY_INDICATOR_SEARCH);
}


static gboolean mark_word(gpointer bm)
{
	AoMarkWordPrivate *priv = AO_MARKWORD_GET_PRIVATE(bm);
	keybindings_send_command(GEANY_KEY_GROUP_SEARCH, GEANY_KEYS_SEARCH_MARKALL);
	/* unset and remove myself */
	priv->double_click_timer_id = 0;
	return FALSE;
}


static gboolean on_editor_button_press_event(GtkWidget *widget, GdkEventButton *event,
											 AoMarkWord *bm)
{
	if (event->button == 1)
	{
		AoMarkWordPrivate *priv = AO_MARKWORD_GET_PRIVATE(bm);
		if (! priv->enable_markword)
			return FALSE;

		if (event->type == GDK_BUTTON_PRESS)
		{
			if (priv->enable_single_click_deselect)
				clear_marker();
		}
		else if (event->type == GDK_2BUTTON_PRESS)
		{
			if (priv->double_click_timer_id == 0)
				priv->double_click_timer_id = g_timeout_add(DOUBLE_CLICK_DELAY, mark_word, bm);
		}
	}
	return FALSE;
}

void ao_mark_editor_notify(AoMarkWord *mw, GeanyEditor *editor, SCNotification *nt)
{
	// If something is about to be deleted and there is selected text clear the markers
	if(nt->nmhdr.code == SCN_MODIFIED &&
		((nt->modificationType & SC_MOD_BEFOREDELETE) == SC_MOD_BEFOREDELETE) &&
		sci_has_selection(editor->sci))
	{
		AoMarkWordPrivate *priv = AO_MARKWORD_GET_PRIVATE(mw);

		if(priv->enable_markword && priv->enable_single_click_deselect)
			clear_marker();
	}

	// In single click deselect mode, clear the markers when the cursor moves
	else if(nt->nmhdr.code == SCN_UPDATEUI &&
		nt->updated == SC_UPDATE_SELECTION &&
		!sci_has_selection(editor->sci))
	{
		AoMarkWordPrivate *priv = AO_MARKWORD_GET_PRIVATE(mw);

		if(priv->enable_markword && priv->enable_single_click_deselect)
			clear_marker();
	}
}


static void connect_document_button_press_signal_handler(AoMarkWord *mw, GeanyDocument *document)
{
	g_return_if_fail(DOC_VALID(document));

	plugin_signal_connect(
		geany_plugin,
		G_OBJECT(document->editor->sci),
		"button-press-event",
		FALSE,
		G_CALLBACK(on_editor_button_press_event),
		mw);
}


static void connect_documents_button_press_signal_handler(AoMarkWord *mw)
{
	guint i = 0;
	/* connect the button-press event for all open documents */
	foreach_document(i)
	{
		connect_document_button_press_signal_handler(mw, documents[i]);
	}
}


void ao_mark_document_new(AoMarkWord *mw, GeanyDocument *document)
{
	connect_document_button_press_signal_handler(mw, document);
}


void ao_mark_document_open(AoMarkWord *mw, GeanyDocument *document)
{
	connect_document_button_press_signal_handler(mw, document);
}


void ao_mark_document_close(AoMarkWord *mw, GeanyDocument *document)
{
	g_return_if_fail(DOC_VALID(document));

	g_signal_handlers_disconnect_by_func(document->editor->sci, on_editor_button_press_event, mw);
}


static void ao_mark_word_init(AoMarkWord *self)
{
	AoMarkWordPrivate *priv = AO_MARKWORD_GET_PRIVATE(self);
	priv->double_click_timer_id = 0;
}


AoMarkWord *ao_mark_word_new(gboolean enable, gboolean single_click_deselect)
{
	return g_object_new(
		AO_MARKWORD_TYPE,
		"enable-markword", enable,
		"enable-single-click-deselect", single_click_deselect,
		NULL);
}
