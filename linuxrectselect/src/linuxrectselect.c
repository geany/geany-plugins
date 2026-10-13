/*
 * linuxrectselect.c — Geany plugin: rectangular/block selection via Ctrl+Alt
 *
 * Scintilla's built-in rectangular selection modifier defaults to Alt, which
 * is grabbed by most Linux window managers for window-move.  This plugin
 * changes the modifier to Ctrl+Alt so that Alt+drag and Shift+Alt+arrows
 * become Ctrl+Alt+drag and Shift+Ctrl+Alt+arrows.
 *
 * Copyright 2025 teknopaul
 * License: GPL-2.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <geanyplugin.h>

GeanyPlugin  *geany_plugin;
GeanyData    *geany_data;

/* Message number changed from 2550 (old Scintilla) to 2598 (Scintilla 5.x / Geany 2.x) */
#ifndef SCI_SETRECTANGULARSELECTIONMODIFIER
# define SCI_SETRECTANGULARSELECTIONMODIFIER 2598
#endif
#ifndef SCMOD_CTRL
# define SCMOD_CTRL  2
#endif
#ifndef SCMOD_ALT
# define SCMOD_ALT   4
#endif

static void
apply_modifier(GeanyDocument *doc)
{
	if (doc && doc->is_valid)
		scintilla_send_message(doc->editor->sci,
		                       SCI_SETRECTANGULARSELECTIONMODIFIER,
		                       SCMOD_CTRL | SCMOD_ALT, 0);
}

static void
on_document_open(G_GNUC_UNUSED GObject *obj, GeanyDocument *doc,
                 G_GNUC_UNUSED gpointer data)
{
	apply_modifier(doc);
}

static gboolean
lrs_init(GeanyPlugin *plugin, G_GNUC_UNUSED gpointer data)
{
	geany_plugin = plugin;
	geany_data   = plugin->geany_data;

	/* Apply to all documents already open at load time */
	guint i;
	foreach_document(i)
		apply_modifier(documents[i]);

	plugin_signal_connect(plugin, geany->object, "document-open",  FALSE,
	                      G_CALLBACK(on_document_open), NULL);
	plugin_signal_connect(plugin, geany->object, "document-new",   FALSE,
	                      G_CALLBACK(on_document_open), NULL);

	return TRUE;
}

static void
lrs_cleanup(G_GNUC_UNUSED GeanyPlugin *plugin, G_GNUC_UNUSED gpointer data)
{
}

G_MODULE_EXPORT void
geany_load_module(GeanyPlugin *plugin)
{
	plugin->info->name        = "Linux Rectangular Select";
	plugin->info->description = "Changes the rectangular selection modifier from Alt "
	                            "to Ctrl+Alt so block selection works on Linux desktops "
	                            "that grab Alt for window management. "
	                            "Use Ctrl+Alt+drag or Shift+Ctrl+Alt+arrows.";
	plugin->info->version     = "0.1";
	plugin->info->author      = "teknopaul";

	plugin->funcs->init    = lrs_init;
	plugin->funcs->cleanup = lrs_cleanup;

	GEANY_PLUGIN_REGISTER(plugin, 235);
}
