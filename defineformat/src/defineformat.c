/*
 *      defineformat.c
 *
 *      Copyright 2013 Pavel Roschin <rpg89@post.ru>
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

GeanyPlugin		*geany_plugin;
GeanyData		*geany_data;
GeanyFunctions		*geany_functions;

PLUGIN_VERSION_CHECK(216)
PLUGIN_SET_TRANSLATABLE_INFO(
	LOCALEDIR,
	GETTEXT_PACKAGE,
	_("Define formatter"),
	_("Automatically align backslash in multi-line defines"),
	"0.1",
	"Pavel Roschin <rpg89@post.ru>")

static gint
get_end_line(ScintillaObject *sci, gint line)
{
	gint end;
	gchar ch;
	end = sci_get_line_end_position(sci, line);
	ch = sci_get_char_at(sci, end - 1);
	while(ch == ' ' || ch == '\t')
	{
		end--;
		ch = sci_get_char_at(sci, end - 1);
	}
	return end;
}

static gint
get_indent_pos(ScintillaObject *sci, gint line)
{
	return (gint)SSM(sci, SCI_GETLINEINDENTPOSITION, (uptr_t)line, 0);
}

static gboolean
define_format(ScintillaObject *sci, gboolean newline)
{
	gint             lexer;
	gint             start_pos;
	gint             end_pos;
	gint             length;
	gint             line;
	gint             first_line;
	gint             first_end;
	gchar           *start_line;
	gchar            end_char;
	gint             max = geany_data->editor_prefs->long_line_column;

	lexer = sci_get_lexer(sci);
	if (lexer != SCLEX_CPP)
		return FALSE;

	first_line = line = sci_get_current_line(sci);
	first_end = end_pos = get_end_line(sci, line);
	end_char = sci_get_char_at(sci, end_pos - 1);
	if(end_char != '\\')
		return FALSE;
	if(newline) /* small hack to pass the test */
		line--;
	do {
		line--;
		end_pos = get_end_line(sci, line);
		end_char = sci_get_char_at(sci, end_pos - 1);
	} while(end_char == '\\' && line >= 0);
	line++;
	start_pos = (gint)SSM(sci, SCI_GETLINEINDENTPOSITION, (uptr_t)line, 0);
	end_pos = sci_get_line_end_position(sci, line);
	if(start_pos == end_pos)
		return FALSE;
	start_line = sci_get_contents_range(sci, start_pos, end_pos);
	if(NULL == start_line)
		return FALSE;
	if(strncmp(start_line, "#define ", sizeof("#define ") - 1))
		goto out;
	sci_start_undo_action(sci);
	/* This is special newline handler - it continues multiline define */
	if(newline)
	{
		sci_add_text(sci, "\n");
		line = sci_get_current_line(sci) - 1;
		end_pos = sci_get_line_end_position(sci, line);
		length = end_pos - get_indent_pos(sci, line) + sci_get_line_indentation(sci, line);
		for(; length < max - 1; length++, end_pos++)
			sci_insert_text(sci, end_pos, " ");
		sci_insert_text(sci, end_pos,  "\\");
		first_line = line + 1;
		first_end = get_end_line(sci, first_line);
	}
	for (first_end--; sci_get_char_at(sci, first_end - 1) == ' '; first_end--) {}
	SSM(sci, SCI_DELETERANGE, first_end, sci_get_line_end_position(sci, first_line) - first_end);
	length = first_end - get_indent_pos(sci, first_line) + sci_get_line_indentation(sci, first_line);
	for(; length < max - 1; length++, first_end++)
		sci_insert_text(sci, first_end, " ");
	sci_insert_text(sci, first_end, "\\");
	sci_end_undo_action(sci);
out:
	g_free(start_line);
	return newline;
}

static gboolean
on_key_release(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	GeanyDocument *doc = user_data;
	if(NULL == doc || NULL == doc->editor || NULL == doc->editor->sci)
		return FALSE;
	if(event->keyval == GDK_BackSpace || event->keyval == GDK_Delete || event->keyval == GDK_Tab)
		define_format(doc->editor->sci, FALSE);
	return FALSE;
}

static gboolean
on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	GeanyDocument *doc = user_data;
	if(NULL == doc || NULL == doc->editor || NULL == doc->editor->sci)
		return FALSE;
	if(event->keyval == GDK_Return)
	{
		return define_format(doc->editor->sci, TRUE);
	}
	return FALSE;
}

static gboolean
editor_notify_cb(GObject *object, GeanyEditor *editor,
								SCNotification *nt, gpointer data)
{
	if(NULL == editor || NULL == editor->sci)
		return FALSE;
	if(nt->nmhdr.code == SCN_CHARADDED)
		define_format(editor->sci, FALSE);
	return FALSE;
}
static void
on_document_activate(GObject *obj, GeanyDocument *doc, gpointer user_data)
{
	ScintillaObject *sci = NULL;
	if(NULL == doc || NULL == doc->editor)
		return;
	sci = doc->editor->sci;
	if(NULL == sci)
		return;
	plugin_signal_connect(geany_plugin, G_OBJECT(sci), "key-release-event",
			FALSE, G_CALLBACK(on_key_release), doc);
	plugin_signal_connect(geany_plugin, G_OBJECT(sci), "key-press-event",
			FALSE, G_CALLBACK(on_key_press), doc);
}

/* FIXME: I need to use these dirty hacks because native API doesn't provide modification events */
PluginCallback plugin_callbacks[] =
{
	{ "document-open", (GCallback) &on_document_activate, FALSE, NULL },
	{ "document-new",  (GCallback) &on_document_activate, FALSE, NULL },
	{ "editor-notify", (GCallback) &editor_notify_cb,     FALSE, NULL },
	{ NULL, NULL, FALSE, NULL }
};

void plugin_init(GeanyData *data)
{
}

void plugin_cleanup(void)
{
}
