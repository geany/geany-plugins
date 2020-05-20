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
#include "keybindings.h"
#include <geany.h>
#include "Scintilla.h"	/* for the SCNotification struct */
#include "stdio.h"
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <assert.h>
//#undef geany
/* text to be shown in the plugin dialog */
static GeanyPlugin* g_plugin = NULL;
static gboolean recording;
static GdkEventKey** recorded_pattern;
static guint recorded_size;
int CAPACITY = 2;
GeanyKeyBinding *record, *play;
static gboolean is_record_key(GdkEventKey *event)
{
	return event->keyval == record->key
		&& (event->state & record->mods) == record->mods;
}

static gboolean is_play_key(GdkEventKey *event)
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
		GdkEventKey* new_event = gdk_event_copy(event);
		if (recorded_size == CAPACITY)
		{
			tmp = g_new0(GdkEventKey*, CAPACITY * 2);
			for (i = 0; i < recorded_size; i++)
			   tmp[i] = recorded_pattern[i];
			g_free(recorded_pattern);
			recorded_pattern = tmp;
			CAPACITY *= 2;
		}
		assert(recorded_size < CAPACITY);
		if (recorded_pattern[(recorded_size)] != NULL)
		   gdk_event_free(recorded_pattern[recorded_size]);
		recorded_pattern[recorded_size++] = new_event;
	}
	
	return FALSE;
}

static void
on_document_open(GObject *obj, GeanyDocument *doc, gpointer user_data)
{
	GeanyDocument *data;
	ScintillaObject   *sci;
	g_return_if_fail(DOC_VALID(doc));
	sci = doc->editor->sci;
	data = g_new0(GeanyDocument, 1);
	*data = *doc;
	/* This will free the data when the sci is destroyed */
	g_object_set_data_full(G_OBJECT(sci), "keyrecord-userdata", data, g_free);
}

static PluginCallback keyrecord_callbacks[] =
{
	/* Set 'after' (third field) to TRUE to run the callback @a after the default handler.
	 * If 'after' is FALSE, the callback is run @a before the default handler, so the plugin
	 * can prevent Geany from processing the notification. Use this with care. */
	{ "document-open",  (GCallback) &on_document_open, FALSE, NULL },
	{ "document-new",   (GCallback) &on_document_open, FALSE, NULL },
	{ "key-press", (GCallback) &on_key_press, FALSE, NULL },
	//{ "editor-notify", (GCallback) &on_editor_notify, FALSE, NULL },
	{ NULL, NULL, FALSE, NULL }
};

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
}


static void
on_play (guint key_id)
{
	if (!recording)
	{
	//fprintf(stderr, "play: %d\n", key_id);
    	guint i;
////	sci_start_undo_action(doc->editor->sci);
		for (i = 0; i < (recorded_size); i++)
			gtk_main_do_event(recorded_pattern[i]);
	}
//	sci_end_undo_action(doc->editor->sci);
}

/* Called by Geany to initialize the plugin */
static gboolean keyrecord_init(GeanyPlugin *plugin, gpointer data)
{
	GeanyKeyGroup *group;
  
	group = plugin_set_key_group (plugin, "keyrecord", 2, NULL);
	record = keybindings_set_item (group, 0, on_record,
                        0, 0, "record", _("Start/Stop record"), NULL);
    play = keybindings_set_item (group, 1, on_play,
                        0, 0, "play", _("Play"), NULL);
    
    GeanyData* geany_data = plugin->geany_data;
    recorded_pattern = g_new0(GdkEventKey*, CAPACITY);

	guint i = 0;
	foreach_document(i) {
		 on_document_open(NULL, documents[i], NULL);
	}
	recording = FALSE;
	recorded_size = 0;
	
	geany_plugin_set_data(plugin, plugin, NULL);
	
	g_plugin = plugin;
	
	return TRUE;
}


/* Called by Geany before unloading the plugin.
 * Here any UI changes should be removed, memory freed and any other finalization done.
 * Be sure to leave Geany as it was before demo_init(). */
static void keyrecord_cleanup(GeanyPlugin *plugin, gpointer _data)
{
	GeanyData* geany_data = plugin->geany_data;
    guint i;
    for (i = 0; i < CAPACITY; i++)
        if (recorded_pattern[i] != NULL)
			gdk_event_free(recorded_pattern[i]);
    g_free(recorded_pattern);
   
    foreach_document(i)
	{
		gpointer data;
		ScintillaObject *sci;

		sci = documents[i]->editor->sci;
		data = g_object_steal_data(G_OBJECT(sci), "keyrecord-userdata");
		g_free(data);
	}

}

void geany_load_module(GeanyPlugin *plugin)
{
	/* main_locale_init() must be called for your package before any localization can be done */
	main_locale_init(LOCALEDIR, GETTEXT_PACKAGE);
	plugin->info->name = _("Keystrokes recorder");
	plugin->info->description = _("Allows to record some sequence of keystrokes and replay it");
	plugin->info->version = "0.12";
	plugin->info->author =  _("tunyash");

	plugin->funcs->init = keyrecord_init;
	plugin->funcs->configure = NULL;
	plugin->funcs->help = NULL; /* This demo has no help but it is an option */
	plugin->funcs->cleanup = keyrecord_cleanup;
	plugin->funcs->callbacks = keyrecord_callbacks;

	// API 237, Geany 1.34, PR #1829, added "key-press" signal.
	GEANY_PLUGIN_REGISTER(plugin, 237);
}
