/*      defineformat.c
 *
 *      Copyright 2013 Pavel Roschin <rpg89(at)post(dot)ru>
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

/* #define DEBUG */

#ifdef DEBUG
#define dprintf(arg...) fprintf(stderr, arg)
#else
#define dprintf(...);
#endif

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
	"Pavel Roschin <rpg89(at)post(dot)ru>")

static GArray *lines_stack = NULL;

static gint
get_line_end(ScintillaObject *sci, gint line)
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

static const gchar *
get_char_range(ScintillaObject *sci, gint start, gint length)
{
	return (const gchar *) SSM(sci, SCI_GETRANGEPOINTER, start, length);
}

static gboolean
inside_define(ScintillaObject *sci, gint line, gboolean newline)
{
	gint    lexer;
	gint    start_pos;
	gint    end_pos;
	gchar   end_char;

	lexer = sci_get_lexer(sci);
	if(lexer != SCLEX_CPP)
		return FALSE;

	end_pos = get_line_end(sci, line);
	end_char = sci_get_char_at(sci, end_pos - 1);
	if(end_char != '\\')
	{
		dprintf("End char is not \\, exit\n");
		return FALSE;
	}
	if(newline)
		line--;
	do {
		line--;
		end_pos = get_line_end(sci, line);
		end_char = sci_get_char_at(sci, end_pos - 1);
	} while(end_char == '\\' && line >= 0);
	line++;
	dprintf("Expecting define on line %d\n", line + 1);
	start_pos = (gint)SSM(sci, SCI_GETLINEINDENTPOSITION, (uptr_t)line, 0);
	end_pos = sci_get_line_end_position(sci, line);
	if(start_pos == end_pos)
	{
		dprintf("line empty, exit\n");
		return FALSE;
	}
	const gchar *start_line = get_char_range(sci, start_pos, 7);
	g_return_val_if_fail(NULL != start_line, FALSE);
	if(0 != strncmp(start_line, "#define ", strlen("#define ")))
	{
		dprintf("Start line is not \"#define\", exit\n");
		return FALSE;
	}
	return TRUE;
}

static void
define_format_newline(ScintillaObject *sci)
{
	gint    end_pos;
	gint    line;

	line = sci_get_current_line(sci);
	if(!inside_define(sci, line, TRUE))
		return;
	line--;
	end_pos = sci_get_line_end_position(sci, line);
	sci_insert_text(sci, end_pos,  "\\");
	line += 2;
	dprintf("Added line: %d\n", line);
	g_array_append_val(lines_stack, line);
}

static void
define_format_line(ScintillaObject *sci, gint current_line)
{
	gint    length;
	gint    first_line;
	gint    first_end;
	gint    max = geany_data->editor_prefs->long_line_column;

	if(!inside_define(sci, current_line, FALSE))
		return;

	first_line = current_line;
	first_end = get_line_end(sci, first_line);
	for (first_end--; sci_get_char_at(sci, first_end - 1) == ' '; first_end--) {}
	SSM(sci, SCI_DELETERANGE, first_end, sci_get_line_end_position(sci, first_line) - first_end);
	length = first_end - get_indent_pos(sci, first_line) + sci_get_line_indentation(sci, first_line);
	for(; length < max - 1; length++, first_end++)
		sci_insert_text(sci, first_end, " ");
	sci_insert_text(sci, first_end, "\\");
}

static gboolean
editor_notify_cb(GObject *object, GeanyEditor *editor, SCNotification *nt, gpointer data)
{
	gint i = 0, val;
	gint old_position = 0;
	gint old_lposition = 0;
	gint old_line = 0;
	gint pos;
	if(NULL == editor || NULL == editor->sci)
		return FALSE;
	if(nt->nmhdr.code == SCN_CHARADDED)
	{
		if('\n' == nt->ch)
			define_format_newline(editor->sci);
	}
	if(nt->nmhdr.code == SCN_UPDATEUI)
	{
		if(g_array_index(lines_stack, gint, 0))
		{
			/* save current position */
			old_line = sci_get_current_line(editor->sci);
			old_lposition = sci_get_line_end_position(editor->sci, old_line) - sci_get_line_length(editor->sci, old_line);
			old_position = sci_get_current_position(editor->sci);
			sci_start_undo_action(editor->sci);
		}
		while((val = g_array_index(lines_stack, gint, i)))
		{
			i++;
			define_format_line(editor->sci, val - 1);
			dprintf("Removed from stack: %d\n", val);
		}
		if(i > 0)
		{
			sci_end_undo_action(editor->sci);
			g_array_remove_range(lines_stack, 0, i);
			/* restore current position */
			pos = sci_get_line_end_position(editor->sci, old_line) - sci_get_line_length(editor->sci, old_line);
			sci_set_current_position(editor->sci, old_position + pos - old_lposition, FALSE);
		}
	}
	if(nt->nmhdr.code == SCN_MODIFIED)
	{
		if(nt->modificationType & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT))
		{
			if(nt->modificationType & (SC_PERFORMED_UNDO | SC_PERFORMED_REDO))
				return FALSE;
			gint line = sci_get_line_from_position(editor->sci, nt->position) + 1;
			if(sci_get_char_at(editor->sci, get_line_end(editor->sci, line - 1) - 1) == '\\')
			{
				gboolean found = FALSE;
				while((val = g_array_index(lines_stack, gint, i)))
				{
					if(val == line)
					{
						found = TRUE;
						break;
					}
					i++;
				}
				if(!found)
				{
					dprintf("Added line: %d\n", line);
					g_array_append_val(lines_stack, line);
				}
			}
		}
	}
	return FALSE;
}

PluginCallback plugin_callbacks[] =
{
	{ "editor-notify", (GCallback) &editor_notify_cb, FALSE, NULL },
	{ NULL, NULL, FALSE, NULL }
};

void plugin_init(GeanyData *data)
{
	lines_stack = g_array_new (TRUE, FALSE, sizeof(gint));
}

void plugin_cleanup(void)
{
	g_array_free(lines_stack, TRUE);
}

void
plugin_help(void)
{
	utils_open_browser("http://plugins.geany.org/defineformat.html");
}
