/*
 *      keyrecord.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2007-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2007-2012 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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
 *      You should have received a copy of the GNU General Public License along
 *      with this program; if not, write to the Free Software Foundation, Inc.,
 *      51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */


/**
 * Keystrokes recorder plugin - plugin for recording sequences of keystrokes and replaying it
 */


#include <geanyplugin.h>	/* plugin API, always comes first */
#include <geany.h>
#include "keybindings.h"
#include <assert.h>


static gboolean recording;
static GdkEventKey** recorded_pattern;
static guint recorded_size;
static guint recorded_capacity = 2;
static GeanyKeyBinding *record, *play;
static GtkLabel *keyrecord_state_label;


static void
update_status(void)
{
	if (recording)
		gtk_widget_show(GTK_WIDGET(keyrecord_state_label));
	else
		gtk_widget_hide(GTK_WIDGET(keyrecord_state_label));
}


static gboolean
is_record_key(GdkEventKey *event)
{
	return event->keyval == record->key
		&& (event->state & record->mods) == record->mods;
}


static gboolean
is_play_key(GdkEventKey *event)
{
	return event->keyval == play->key
		&& (event->state & play->mods) == play->mods;
}


static gboolean
on_key_press(GObject *object, GdkEventKey *event, gpointer user_data)
{
	guint i;
	GdkEventKey** tmp = NULL;

	if ((recording) && !(is_record_key(event)||is_play_key(event)))
	{
		GdkEventKey* new_event = (GdkEventKey*)gdk_event_copy((GdkEvent*)event);
		if (recorded_size == recorded_capacity)
		{
			tmp = g_new0(GdkEventKey*, recorded_capacity * 2);
			for (i = 0; i < recorded_size; i++)
				tmp[i] = recorded_pattern[i];

			g_free(recorded_pattern);
			recorded_pattern = tmp;
			recorded_capacity *= 2;
		}
		assert(recorded_size < recorded_capacity);
		if (recorded_pattern[(recorded_size)] != NULL)
			gdk_event_free((GdkEvent*)recorded_pattern[recorded_size]);

		recorded_pattern[recorded_size++] = new_event;
	}

	return FALSE;
}


static void
on_document_open(GObject *obj, GeanyDocument *doc, gpointer user_data)
{
	GeanyDocument *data;
	ScintillaObject *sci;

	g_return_if_fail(DOC_VALID(doc));
	sci = doc->editor->sci;
	data = g_new0(GeanyDocument, 1);
	*data = *doc;
	/* This will free the data when the sci is destroyed */
	g_object_set_data_full(G_OBJECT(sci), "keyrecord-userdata", data, g_free);
}


static void
on_record (guint key_id)
{
	if (!recording)
	{
		recording = TRUE;
		recorded_size = 0;
	}
	else
	{
		recording = FALSE;
	}
	update_status();
}


static void
on_play (guint key_id)
{
	if (!recording)
	{
		guint i;
		for (i = 0; i < (recorded_size); i++)
			gtk_main_do_event((GdkEvent*)recorded_pattern[i]);
	}
}


static void
recording_status_init(GeanyPlugin *plugin)
{
	GeanyData* geany_data = plugin->geany_data;
	GtkStatusbar *geany_statusbar;
	GtkHBox *geany_statusbar_box;

	geany_statusbar = (GtkStatusbar*)ui_lookup_widget(geany->main_widgets->window, "statusbar");
	geany_statusbar_box = (GtkHBox*)gtk_widget_get_parent(GTK_WIDGET(geany_statusbar));
	keyrecord_state_label = (GtkLabel*)gtk_label_new(NULL);
	gtk_label_set_markup(keyrecord_state_label,
		"<span foreground='red' weight='bold'>REC</span>");
	gtk_box_pack_end(GTK_BOX(geany_statusbar_box),
		GTK_WIDGET(keyrecord_state_label), FALSE, FALSE, 10);
}


static PluginCallback
keyrecord_callbacks[] =
{
	/* Set 'after' (third field) to TRUE to run the callback
	 * @a after the default handler.  If 'after' is FALSE,
	 * the callback is run @a before the default handler,
	 * so the plugin can prevent Geany from processing the
	 * notification. Use this with care. */
	{ "document-open", (GCallback) &on_document_open, FALSE, NULL },
	{ "document-new", (GCallback) &on_document_open, FALSE, NULL },
	{ "key-press", (GCallback) &on_key_press, FALSE, NULL },
	{ NULL, NULL, FALSE, NULL }
};


/* Called by Geany to initialize the plugin */
static gboolean
keyrecord_init(GeanyPlugin *plugin, gpointer data)
{
	GeanyKeyGroup *group;

	group = plugin_set_key_group (plugin, "keyrecord", 2, NULL);
	record = keybindings_set_item (group, 0, on_record,
		0, 0, "record", _("Start/Stop record"), NULL);
	play = keybindings_set_item (group, 1, on_play,
		0, 0, "play", _("Play"), NULL);

	GeanyData* geany_data = plugin->geany_data;
	recorded_pattern = g_new0(GdkEventKey*, recorded_capacity);

	guint i = 0;
	foreach_document(i)
//	{
		 on_document_open(NULL, documents[i], NULL);
//	}
	recording = FALSE;
	recorded_size = 0;

	recording_status_init(plugin);
	update_status();

	geany_plugin_set_data(plugin, plugin, NULL);

	return TRUE;
}


/* Called by Geany before unloading the plugin.
 * Here any UI changes should be removed,
 * memory freed and any other finalization done.
 * Be sure to leave Geany as it was before demo_init(). */
static void
keyrecord_cleanup(GeanyPlugin *plugin, gpointer _data)
{
	GeanyData* geany_data = plugin->geany_data;
	guint i;

	for (i = 0; i < recorded_capacity; i++)
		if (recorded_pattern[i] != NULL)
			gdk_event_free((GdkEvent*)recorded_pattern[i]);

	g_free(recorded_pattern);

	foreach_document(i)
	{
		gpointer data;
		ScintillaObject *sci;

		sci = documents[i]->editor->sci;
		data = g_object_steal_data(G_OBJECT(sci), "keyrecord-userdata");
		g_free(data);
	}

	gtk_widget_destroy(GTK_WIDGET(keyrecord_state_label));
}


void
geany_load_module(GeanyPlugin *plugin)
{
	/* main_locale_init(), must be called for your package
	 * before any localization can be done */
	main_locale_init(LOCALEDIR, GETTEXT_PACKAGE);

	plugin->info->name = _("Keystrokes recorder");
	plugin->info->description =
		_("Allows user to record some sequence of keystrokes and replay it");
	plugin->info->version = "0.12";
	plugin->info->author = _("tunyash");

	plugin->funcs->init = keyrecord_init;
	plugin->funcs->configure = NULL;
	plugin->funcs->help = NULL;
	plugin->funcs->cleanup = keyrecord_cleanup;
	plugin->funcs->callbacks = keyrecord_callbacks;

	/* API 237, Geany 1.34, PR #1829, added "key-press" signal */
	GEANY_PLUGIN_REGISTER(plugin, 237);
}
