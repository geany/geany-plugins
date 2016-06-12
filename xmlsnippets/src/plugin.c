/*
 * plugin.c - this file is part of XMLSnippets, a Geany plugin
 *
 * Copyright 2010 Eugene Arshinov <earshinov(at)gmail(dot)com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#ifndef TEST

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "plugin.h"
#include "xmlsnippets.h"
#include <SciLexer.h>


static gboolean editor_notify_cb(GObject *object, GeanyEditor *editor,
	SCNotification *nt, gpointer data);


GeanyData *geany_data;
GeanyPlugin *geany_plugin;

PLUGIN_VERSION_CHECK(224)
PLUGIN_SET_TRANSLATABLE_INFO(
  LOCALEDIR,
  GETTEXT_PACKAGE,
  _("XML Snippets"),
  _("Autocompletes XML/HTML tags using snippets."),
  VERSION,
  "Eugene Arshinov")

PluginCallback plugin_callbacks[] =
{
	{ "editor-notify", (GCallback) &editor_notify_cb, FALSE, NULL },
	{ NULL, NULL, FALSE, NULL }
};

void plugin_init(GeanyData *data)
{
}

void plugin_cleanup(void)
{
}


static gboolean editor_notify_cb(GObject *object, GeanyEditor *editor,
								SCNotification *nt, gpointer data)
{
	gint lexer, pos, style, min, size;
	gboolean handled = FALSE;

	if (nt->nmhdr.code == SCN_CHARADDED && nt->ch == '>')
	{
		lexer = sci_get_lexer(editor->sci);
		if (lexer == SCLEX_XML || lexer == SCLEX_HTML)
		{
			pos = sci_get_current_position(editor->sci);
			style = sci_get_style_at(editor->sci, pos);

			if ((style <= SCE_H_XCCOMMENT || highlighting_is_string_style(lexer, style)) &&
				!highlighting_is_comment_style(lexer, style))
			{
				CompletionInfo c;
				InputInfo i;
				gchar *sel;

				/* Grab the last 512 characters or so */
				min = pos - 512;
				if (min < 0) min = 0;
				size = pos - min;

				sel = sci_get_contents_range(editor->sci, min, pos);

				if (get_completion(editor, sel, size, &c, &i))
				{
					/* Remove typed opening tag */
					sci_set_selection_start(editor->sci, min + i.tag_start);
					sci_set_selection_end(editor->sci, pos);
					sci_replace_sel(editor->sci, "");
					pos -= (size - i.tag_start); /* pos has changed while deleting */

					/* Insert the completion */
					editor_insert_snippet(editor, pos, c.completion);
					sci_scroll_caret(editor->sci);

					g_free((gchar *)c.completion);
					handled = TRUE;
				}

				g_free(sel);
			}
		}
	}
	return handled;
}

#endif
