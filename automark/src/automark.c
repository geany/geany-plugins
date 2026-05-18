/*
 *      automark.c
 *
 *      Copyright 2014 Pavel Roschin <rpg89(at)post(dot)ru>
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
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
	#include "config.h" /* for the gettext domain */
#endif

#include <string.h>
#ifdef HAVE_LOCALE_H
	#include <locale.h>
#endif

#include <gdk/gdkkeysyms.h>

#include <geanyplugin.h>
#include <geany.h>

#include "Scintilla.h"
#include "SciLexer.h"

#define SSM(s, m, w, l) scintilla_send_message(s, m, w, l)

GeanyPlugin	*geany_plugin;
GeanyData	*geany_data;


static gint source_id;

static const gint AUTOMARK_INDICATOR = GEANY_INDICATOR_SEARCH;

static void
search_mark_in_range(
	GeanyEditor *editor,
	gint         flags,
	struct       Sci_TextToFind *ttf)
{
	ScintillaObject *sci = editor->sci;

	while (SSM(sci, SCI_FINDTEXT, flags, (uptr_t)ttf) != -1)
	{
		gint start = ttf->chrgText.cpMin;
		gint end = ttf->chrgText.cpMax;

		if (end > ttf->chrg.cpMax)
			break;

		ttf->chrg.cpMin = ttf->chrgText.cpMax;
		if (end == start)
			continue;
		if(SSM(sci, SCI_INDICATORVALUEAT, AUTOMARK_INDICATOR, start))
			continue;
		SSM(sci, SCI_SETINDICATORCURRENT, AUTOMARK_INDICATOR, 0);
		SSM(sci, SCI_INDICATORFILLRANGE, start, end - start);
	}
}

/* based on editor_find_current_word_sciwc from editor.c */
static gchar *
get_current_word(ScintillaObject *sci)
{
	gint pos = sci_get_current_position(sci);
	gint start = SSM(sci, SCI_WORDSTARTPOSITION, pos, TRUE);
	gint end = SSM(sci, SCI_WORDENDPOSITION, pos, TRUE);

	if (end == start)
		return NULL;

	if ((guint)(end - start) >= GEANY_MAX_WORD_LENGTH)
		end = start + (GEANY_MAX_WORD_LENGTH - 1);
	return sci_get_contents_range(sci, start, end);
}

static gboolean
automark(gpointer user_data)
{
	GeanyDocument      *doc = (GeanyDocument *)user_data;
	GeanyEditor        *editor;
	static GeanyEditor *editor_cache = NULL;
	ScintillaObject    *sci;
	gchar              *text;
	static gchar        text_cache[GEANY_MAX_WORD_LENGTH] = {0};
	gint                match_flag = SCFIND_MATCHCASE | SCFIND_WHOLEWORD;
	struct              Sci_TextToFind ttf;

	source_id = 0;

	/* during timeout document could be destroyed so check everything again */
	if (!DOC_VALID(doc))
		return FALSE;

	editor = doc->editor;
	sci = editor->sci;

	/* Do not highlight while selecting text and allow other markers to work */
	if (sci_has_selection(sci))
	{
		editor_indicator_clear(editor, AUTOMARK_INDICATOR);
		return FALSE;
	}

	text = get_current_word(editor->sci);

	if (EMPTY(text))
	{
		editor_indicator_clear(editor, AUTOMARK_INDICATOR);
		g_free(text);
		return FALSE;
	}

	if (editor_cache != editor || strcmp(text, text_cache) != 0)
	{
		editor_indicator_clear(editor, AUTOMARK_INDICATOR);
		strcpy(text_cache, text);
		editor_cache = editor;
	}

	gint vis_first = SSM(sci, SCI_GETFIRSTVISIBLELINE, 0, 0);
	gint doc_first = SSM(sci, SCI_DOCLINEFROMVISIBLE, vis_first, 0);
	gint vis_last  = SSM(sci, SCI_LINESONSCREEN, 0, 0) + vis_first;
	gint doc_last  = SSM(sci, SCI_DOCLINEFROMVISIBLE, vis_last, 0);
	gint start     = SSM(sci, SCI_POSITIONFROMLINE,   doc_first, 0);
	gint end       = SSM(sci, SCI_GETLINEENDPOSITION, doc_last, 0);

	ttf.chrg.cpMin = start;
	ttf.chrg.cpMax = end;
	ttf.lpstrText  = text;

	search_mark_in_range(editor, match_flag, &ttf);

	g_free(text);

	return FALSE;
}

static gboolean
on_editor_notify(
	GObject        *obj,
	GeanyEditor    *editor,
	SCNotification *nt,
	gpointer        user_data)
{
	if (SCN_UPDATEUI == nt->nmhdr.code)
	{
		/* if events are too intensive - remove old callback */
		if (source_id)
			g_source_remove(source_id);
		source_id = g_idle_add(automark, editor->document);
	}
	return FALSE;
}

static PluginCallback plugin_automark_callbacks[] =
{
	{ "editor-notify",  (GCallback) &on_editor_notify, FALSE, NULL },
	{ NULL, NULL, FALSE, NULL }
};


static gboolean
plugin_automark_init(GeanyPlugin *plugin, G_GNUC_UNUSED gpointer pdata)
{
	geany_plugin = plugin;
	geany_data = plugin->geany_data;
	source_id = 0;
	return TRUE;
}


static void
plugin_automark_cleanup(G_GNUC_UNUSED GeanyPlugin *plugin, G_GNUC_UNUSED gpointer pdata)
{
	if (source_id)
		g_source_remove(source_id);
}


static void
plugin_automark_help (G_GNUC_UNUSED GeanyPlugin *plugin, G_GNUC_UNUSED gpointer pdata)
{
	utils_open_browser("http://plugins.geany.org/automark.html");
}


/* Load module */
G_MODULE_EXPORT
void geany_load_module(GeanyPlugin *plugin)
{
	/* Setup translation */
	main_locale_init(LOCALEDIR, GETTEXT_PACKAGE);

	/* Set metadata */
	plugin->info->name = _("Auto-mark");
	plugin->info->description = _("Auto-mark word under cursor");
	plugin->info->version = "0.1";
	plugin->info->author = "Pavel Roschin <rpg89(at)post(dot)ru>";

	/* Set functions */
	plugin->funcs->init = plugin_automark_init;
	plugin->funcs->cleanup = plugin_automark_cleanup;
	plugin->funcs->help = plugin_automark_help;
	plugin->funcs->callbacks = plugin_automark_callbacks;

	/* Register! */
	GEANY_PLUGIN_REGISTER(plugin, 226);
}
