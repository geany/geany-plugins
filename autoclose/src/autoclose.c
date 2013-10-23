/*
 *      autoclose.c
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

#define AC_STOP_ACTION TRUE
#define AC_CONTINUE_ACTION FALSE
#define SSM(s, m, w, l) scintilla_send_message(s, m, w, l)

GeanyPlugin	*geany_plugin;
GeanyData	*geany_data;
GeanyFunctions	*geany_functions;

PLUGIN_VERSION_CHECK(216)
PLUGIN_SET_TRANSLATABLE_INFO(
	LOCALEDIR,
	GETTEXT_PACKAGE,
	_("Auto-close"),
	_("Auto-close braces and brackets with lot of features"),
	"0.2",
	"Pavel Roschin <rpg89(at)post(dot)ru>")

/* avoid aggresive warnings */
#undef DOC_VALID
#define DOC_VALID(doc_ptr) (((doc_ptr) && (doc_ptr)->is_valid))

typedef struct {
	/* close chars */
	gboolean parenthesis;
	gboolean abracket;
	gboolean abracket_htmlonly;
	gboolean cbracket;
	gboolean sbracket;
	gboolean dquote;
	gboolean squote;
	gboolean backquote;
	gboolean backquote_bashonly;
	/* settings */
	gboolean delete_pairing_brace;
	gboolean suppress_doubling;
	gboolean enclose_selections;
	gboolean comments_ac_enable;
	gboolean comments_enclose;
	gboolean keep_selection;
	gboolean make_indent_for_cbracket;
	gboolean move_cursor_to_beginning;
	gboolean improved_cbracket_indent;
	gboolean close_functions;
	gboolean bcksp_remove_pair;
	gboolean jump_on_tab;
	/* others */
	gchar *config_file;
} AutocloseInfo;

static AutocloseInfo *ac_info = NULL;

typedef struct {
	/* used to place the caret after autoclosed items on tab (similar to eclipse) */
	gint jump_on_tab;
	/* used to reset jump_on_tab when needed */
	gint last_caret;
	/* used to reset jump_on_tab when needed */
	gint last_line;
	struct GeanyDocument *doc;
} AutocloseUserData;

static gint
get_indent(ScintillaObject *sci, gint line)
{
	return (gint) SSM(sci, SCI_GETLINEINDENTPOSITION, (uptr_t) line, 0);
}

static gchar
char_at(ScintillaObject *sci, gint pos)
{
	return sci_get_char_at(sci, pos);
}

static const gchar *
get_char_range(ScintillaObject *sci, gint start, gint length)
{
	return (const gchar *) SSM(sci, SCI_GETRANGEPOINTER, start, length);
}

static gboolean
blank_line(ScintillaObject *sci, gint line)
{
	return get_indent(sci, line) ==
		sci_get_line_end_position(sci, line);
}

static void
unindent_line(ScintillaObject *sci, gint line, gint indent_width)
{
	gint indent = sci_get_line_indentation(sci, line);
	sci_set_line_indentation(sci, line, indent > 0 ? indent - indent_width : 0);
}

static void
delete_line(ScintillaObject *sci, gint line)
{
	gint start = sci_get_position_from_line(sci, line);
	gint len = sci_get_line_length(sci, line);
	SSM(sci, SCI_DELETERANGE, start, len);
}

static gint
get_lines_selected(ScintillaObject *sci)
{
	gint start = (gint) SSM(sci, SCI_GETSELECTIONSTART, 0, 0);
	gint end = (gint) SSM(sci, SCI_GETSELECTIONEND, 0, 0);
	gint line_start;
	gint line_end;

	if (start == end)
		return 0; /* no selection */

	line_start = (gint) SSM(sci, SCI_LINEFROMPOSITION, (uptr_t) start, 0);
	line_end = (gint) SSM(sci, SCI_LINEFROMPOSITION, (uptr_t) end, 0);

	return line_end - line_start + 1;
}

static void
insert_text(ScintillaObject *sci, gint pos, const gchar *text)
{
	SSM(sci, SCI_INSERTTEXT, pos, (sptr_t) text);
}

static gint
get_selections(ScintillaObject *sci)
{
	return (gint) SSM(sci, SCI_GETSELECTIONS, 0, 0);
}

static gint
get_caret_pos(ScintillaObject *sci, gint selection)
{
	return (gint) SSM(sci, SCI_GETSELECTIONNCARET, selection, 0);
}

static gint
get_ancor_pos(ScintillaObject *sci, gint selection)
{
	return (gint) SSM(sci, SCI_GETSELECTIONNANCHOR, selection, 0);
}

static gboolean
char_is_quote(gchar ch)
{
	return '\'' == ch || '"' == ch;
}

static gboolean
char_is_curly_bracket(gchar ch)
{
	return '{' == ch || '}' == ch;
}

static gboolean
isspace_no_newline(gchar ch)
{
	return g_ascii_isspace(ch) && ch != '\n' && ch != '\r';
}

/**
 * This function is based on Geany's source but has different meaning: check
 * ability to enclose selection. Calls only for selected text so using
 * sci_get_selection_start/end is ok here.
 * */
static gboolean
lexer_has_braces(ScintillaObject *sci, gint lexer)
{
	gint sel_start;
	switch (lexer)
	{
		case SCLEX_CPP:
		case SCLEX_D:
		case SCLEX_PASCAL:
		case SCLEX_TCL:
		case SCLEX_CSS:
			return TRUE;
		case SCLEX_HTML:	/* for PHP & JS */
		case SCLEX_PERL:
		case SCLEX_BASH:
			/* PHP, Perl, bash has vars like ${var} */
			if (get_lines_selected(sci) > 1)
				return TRUE;
			sel_start = sci_get_selection_start(sci);
			if ('$' == char_at(sci, sel_start - 1))
				return FALSE;
			return TRUE;
		default:
			return FALSE;
	}
}

static gboolean
lexer_cpp_like(gint lexer, gint style)
{
	if (lexer == SCLEX_CPP && style == SCE_C_IDENTIFIER)
		return TRUE;
	return FALSE;
}

static gboolean
filetype_c_or_cpp(gint type)
{
	return type == GEANY_FILETYPES_C || type == GEANY_FILETYPES_CPP;
}

static gboolean
filetype_cpp(gint type)
{
	return type == GEANY_FILETYPES_CPP;
}

static gint
get_end_pos(ScintillaObject *sci, gint line)
{
	gint end;
	gchar ch;
	end = sci_get_line_end_position(sci, line);
	ch = char_at(sci, end - 1);
	/* ignore spaces and "}" */
	while(isspace_no_newline(ch) || '}' == ch)
	{
		end--;
		ch = char_at(sci, end - 1);
	}
	return end;
}

static gboolean
check_chars(
	ScintillaObject *sci,
	gint             ch,
	gchar           *chars_left,
	gchar           *chars_right)
{
	switch (ch)
	{
		case '(':
		case ')':
			if (!ac_info->parenthesis)
				return FALSE;
			*chars_left = '(';
			*chars_right = ')';
			break;
		case ';':
			if (!ac_info->close_functions)
				return FALSE;
			break;
		case '{':
		case '}':
			if (!ac_info->cbracket)
				return FALSE;
			*chars_left = '{';
			*chars_right = '}';
			break;
		case '[':
		case ']':
			if (!ac_info->sbracket)
				return FALSE;
			*chars_left = '[';
			*chars_right = ']';
			break;
		case '<':
		case '>':
			if (!ac_info->abracket)
				return FALSE;
			if (ac_info->abracket_htmlonly &&
					sci_get_lexer(sci) != SCLEX_HTML)
				return FALSE;
			*chars_left = '<';
			*chars_right = '>';
			break;
		case '\'':
			if (!ac_info->squote)
				return FALSE;
			*chars_left = *chars_right = ch;
			break;
		case '"':
			if (!ac_info->dquote)
				return FALSE;
			*chars_left = *chars_right = ch;
			break;
		case '`':
			if (!ac_info->backquote)
				return FALSE;
			if (ac_info->backquote_bashonly &&
					sci_get_lexer(sci) != SCLEX_BASH)
				return FALSE;
			*chars_left = *chars_right = ch;
			break;
		default:
			return FALSE;
	}
	return TRUE;
}

static gboolean
improve_indent(
	ScintillaObject *sci,
	GeanyEditor     *editor,
	gint             pos)
{
	gint ch, ch_next;
	gint line;
	gint indent, indent_width;
	gint end_pos;
	if (!ac_info->improved_cbracket_indent)
		return AC_CONTINUE_ACTION;
	ch = char_at(sci, pos - 1);
	if (ch != '{')
		return AC_CONTINUE_ACTION;
	/* if curly bracket completion is enabled - just make indents
	 * but ensure that second "}" exists. If disabled - make indent
	 * and complete second curly bracket */
	ch_next = char_at(sci, pos);
	if (ac_info->cbracket && ch_next != '}')
		return AC_CONTINUE_ACTION;
	line = sci_get_line_from_position(sci, pos);
	indent = sci_get_line_indentation(sci, line);
	indent_width = editor_get_indent_prefs(editor)->width;
	sci_start_undo_action(sci);
	if (ac_info->cbracket)
		SSM(sci, SCI_ADDTEXT, 2, (sptr_t)"\n\n");
	else
		SSM(sci, SCI_ADDTEXT, 3, (sptr_t)"\n\n}");
	sci_set_line_indentation(sci, line + 1, indent + indent_width);
	sci_set_line_indentation(sci, line + 2, indent);
	/* move to the end of added line */
	end_pos = sci_get_line_end_position(sci, line + 1);
	sci_set_current_position(sci, end_pos, TRUE);
	sci_end_undo_action(sci);
	/* do not alow internal auto-indenter to do the work */
	return AC_STOP_ACTION;
}

static gboolean
handle_backspace(
	AutocloseUserData *data,
	ScintillaObject   *sci,
	gchar              ch,
	gchar             *ch_left,
	gchar             *ch_right,
	GdkEventKey       *event,
	gint               indent_width)
{
	gint pos = sci_get_current_position(sci);
	gint end_pos;
	gint line_start, line_end, line;
	gint i;
	if (!ac_info->delete_pairing_brace)
		return AC_CONTINUE_ACTION;
	ch = char_at(sci, pos - 1);

	if (!check_chars(sci, ch, ch_left, ch_right))
		return AC_CONTINUE_ACTION;

	if (event->state & GDK_SHIFT_MASK)
	{
		if ((ch_left[0] == ch || ch_right[0] == ch) &&
				ac_info->bcksp_remove_pair)
		{
			end_pos = sci_find_matching_brace(sci, pos - 1);
			if (-1 == end_pos)
				return AC_CONTINUE_ACTION;
			sci_start_undo_action(sci);
			line_start = sci_get_line_from_position(sci, pos);
			line_end = sci_get_line_from_position(sci, end_pos);
			SSM(sci, SCI_DELETERANGE, end_pos, 1);
			if (end_pos < pos)
				pos--;
			SSM(sci, SCI_DELETERANGE, pos - 1, 1);
			/* remove indentation magick */
			if (char_is_curly_bracket(ch))
			{
				if (line_start == line_end)
					return AC_CONTINUE_ACTION;
				if (line_start > line_end)
				{
					line = line_end;
					line_end = line_start;
					line_start = line;
				}
				if (blank_line(sci, line_start))
				{
					delete_line(sci, line_start);
					line_end--;
				}
				else
					line_start++;
				if (blank_line(sci, line_end))
					delete_line(sci, line_end);
				line_end--;
				/* unindent */
				for (i = line_start; i <= line_end; i++)
				{
					unindent_line(sci, i, indent_width);
				}
			}
			sci_end_undo_action(sci);
			return AC_STOP_ACTION;
		}
	}

	/* handle \'|' situation */
	if (char_is_quote(ch) && char_at(sci, pos - 2) == '\\')
		return AC_CONTINUE_ACTION;

	if (ch_left[0] == ch && ch_right[0] == char_at(sci, pos))
	{
		SSM(sci, SCI_DELETERANGE, pos, 1);
		data->jump_on_tab = 0;
		return AC_CONTINUE_ACTION;
	}
	return AC_CONTINUE_ACTION;
}


static gboolean
enclose_selection(
	AutocloseUserData *data,
	ScintillaObject   *sci,
	gchar              ch,
	gint               lexer,
	gint               style,
	gchar             *chars_left,
	gchar             *chars_right,
	GeanyEditor       *editor)
{
	gint       i;
	gint       start, end;
	gboolean   in_comment;
	gint       start_line, start_pos, end_line, text_end_pos;
	gint       start_indent, indent_width, current_indent;

	start = sci_get_selection_start(sci);
	end = sci_get_selection_end(sci);

	/* case if selection covers mixed style */
	if (highlighting_is_code_style(lexer, sci_get_style_at(sci, start)) !=
		 highlighting_is_code_style(lexer, sci_get_style_at(sci, end)))
		in_comment = FALSE;
	else
		in_comment = !highlighting_is_code_style(lexer, style);
	if (!ac_info->comments_enclose && in_comment)
		return AC_CONTINUE_ACTION;

	sci_start_undo_action(sci);


	/* Insert {} block - special case: make indents, move cursor to beginning */
	if (char_is_curly_bracket(ch) && lexer_has_braces(sci, lexer) &&
			ac_info->make_indent_for_cbracket && !in_comment)
	{
		start_line = sci_get_line_from_position(sci, start);
		start_pos = SSM(sci, SCI_GETLINEINDENTPOSITION, (uptr_t)start_line, 0);
		insert_text(sci, start_pos, "{\n");

		end_line = sci_get_line_from_position(sci, end);
		start_indent = sci_get_line_indentation(sci, start_line);
		indent_width = editor_get_indent_prefs(editor)->width;
		sci_set_line_indentation(sci, start_line, start_indent);
		sci_set_line_indentation(sci, start_line + 1, start_indent + indent_width);
		for(i = start_line + 2; i <= end_line; i++)
		{
			current_indent = sci_get_line_indentation(sci, i);
			sci_set_line_indentation(sci, i, current_indent + indent_width);
		}
		text_end_pos = sci_get_line_end_position(sci, i - 1);
		sci_set_current_position(sci, text_end_pos, FALSE);
		SSM(sci, SCI_ADDTEXT, 2, (sptr_t)"\n}");
		sci_set_line_indentation(sci, i, start_indent);
		if (ac_info->move_cursor_to_beginning)
			sci_set_current_position(sci, start_pos, TRUE);
	}
	else
	{
		gint selections = get_selections(sci);
		/* specially handle rectangular selection */
		if (selections > 1)
		{
			gint *sels_left = g_malloc(selections * sizeof(gint));
			gint *sels_right = g_malloc(selections * sizeof(gint));
			gint caret = get_caret_pos(sci, 0);
			gint anchor = get_ancor_pos(sci, 0);
			gboolean caret_is_left = caret < anchor;
			gint pos_first = get_caret_pos(sci, 0);
			gint pos_second = get_caret_pos(sci, 1);
			gboolean selection_is_up_down = pos_first < pos_second;
			gint line;
			/* looks like a forward loop but actually lines processed in reverse order */
			for (i = 0; i < selections; i++)
			{
				if(selection_is_up_down)
					line = selections - i - 1;
				else
					line = i;
				if (caret_is_left)
				{
					sels_left[i]  = get_caret_pos(sci, line);
					sels_right[i] = get_ancor_pos(sci, line) + 1;
				}
				else
				{
					sels_right[i] = get_caret_pos(sci, line) + 1;
					sels_left[i]  = get_ancor_pos(sci, line);
				}
			}
			for (i = 0; i < selections; i++)
			{
				insert_text(sci, sels_left[i],  chars_left);
				insert_text(sci, sels_right[i], chars_right);
				sels_left[i]  += (selections - i - 1) * 2 + 1;
				sels_right[i] += (selections - i - 1) * 2 + 1;
			}
			if (ac_info->keep_selection)
			{
				i = 0;
				SSM(sci, SCI_SETSELECTION, sels_left[i], sels_right[i] - 1);
				for (i = 1; i < selections; i++)
					SSM(sci, SCI_ADDSELECTION, sels_left[i], sels_right[i] - 1);
			}
			g_free(sels_left);
			g_free(sels_right);
		}
		else /* normal selection */
		{
			insert_text(sci, start, chars_left);
			insert_text(sci, end + 1, chars_right);
			sci_set_current_position(sci, end + 1, TRUE);
			data->jump_on_tab += strlen(chars_right);
			data->last_caret = end + 1;
			data->last_line = sci_get_current_line(sci);
			if (ac_info->keep_selection)
			{
				sci_set_selection_start(sci, start + 1);
				sci_set_selection_end(sci, end + 1);
			}
		}
	}
	sci_end_undo_action(sci);
	return AC_STOP_ACTION;
}

static gboolean
check_struct(
	ScintillaObject *sci,
	gint             pos,
	const gchar     *str)
{
	gchar ch;
	gint line, len;
	ch = char_at(sci, pos - 1);
	while(g_ascii_isspace(ch))
	{
		pos--;
		ch = char_at(sci, pos - 1);
	}
	line = sci_get_line_from_position(sci, pos);
	len = strlen(str);
	const gchar *sci_buf = get_char_range(sci, get_indent(sci, line), len);
	g_return_val_if_fail(sci_buf, FALSE);
	if (strncmp(sci_buf, str, len) == 0)
		return TRUE;
	return FALSE;
}

static void
struct_semicolon(
	ScintillaObject *sci,
	gint             pos,
	gchar           *chars_right,
	gint             filetype)
{
	if (filetype_c_or_cpp(filetype) && 
	   (check_struct(sci, pos, "struct") || check_struct(sci, pos, "typedef struct")))
	{
		chars_right[1] = ';';
		return;
	}
	if (filetype_cpp(filetype) && check_struct(sci, pos, "class"))
	{
		chars_right[1] = ';';
		return;
	}
}

static gboolean
check_define(
	ScintillaObject *sci,
	gint             line)
{
	const gchar* sci_buf = get_char_range(sci, get_indent(sci, line), 7);
	g_return_val_if_fail(sci_buf, FALSE);
	if (strncmp(sci_buf, "#define", 7) == 0)
		return TRUE;
	return FALSE;
}

static gboolean
auto_close_chars(
	AutocloseUserData *data,
	GdkEventKey       *event)
{
	ScintillaObject *sci;
	GeanyEditor     *editor;
	GeanyDocument   *doc;
	gint             ch, ch_next, ch_buf;
	gchar            chars_left[2]  = {0, 0};
	gchar            chars_right[3] = {0, 0, 0};
	gint             lexer, style;
	gint             pos, line, lex_offset;
	gboolean         has_sel;
	gint             filetype = 0;

	g_return_val_if_fail(data, AC_CONTINUE_ACTION);
	doc = data->doc;
	g_return_val_if_fail(DOC_VALID(doc), AC_CONTINUE_ACTION);
	editor = doc->editor;
	g_return_val_if_fail(editor, AC_CONTINUE_ACTION);
	sci = editor->sci;
	g_return_val_if_fail(sci, AC_CONTINUE_ACTION);

	if (doc->file_type)
		filetype = doc->file_type->id;

	pos = sci_get_current_position(sci);
	line = sci_get_current_line(sci);
	ch = event->keyval;

	if (ch == GDK_BackSpace)
	{
		return handle_backspace(data, sci, ch, chars_left, chars_right,
		                        event, editor_get_indent_prefs(editor)->width);
	}
	else if (ch == GDK_Return)
	{
		return improve_indent(sci, editor, pos);
	}
	else if (ch == GDK_Tab && ac_info->jump_on_tab)
	{
		/* jump behind inserted "); */
		if (data->jump_on_tab == 0)
			return AC_CONTINUE_ACTION;
		sci_set_current_position(sci, pos + data->jump_on_tab, FALSE);
		data->jump_on_tab = 0;
		return AC_STOP_ACTION;
	}

	/* set up completion chars */
	if (!check_chars(sci, ch, chars_left, chars_right))
		return AC_CONTINUE_ACTION;

	has_sel = sci_has_selection(sci);

	/* do not suppress/complete in case: '\|' */
	if (char_is_quote(ch) && char_at(sci, pos - 1) == '\\' && !has_sel)
		return AC_CONTINUE_ACTION;

	lexer = sci_get_lexer(sci);

	/* in C-like languages - complete functions with ; */
	lex_offset = -1;
	ch_buf = char_at(sci, pos + lex_offset);
	while (g_ascii_isspace(ch_buf))
	{
		--lex_offset;
		ch_buf = char_at(sci, pos + lex_offset);
	}

	style = sci_get_style_at(sci, pos + lex_offset);

	/* add ; after functions */
	if (lexer_cpp_like(lexer, style) &&
			chars_left[0] == '(' &&
		 !has_sel &&
			ac_info->close_functions &&
			pos == get_end_pos(sci, line) &&
			sci_get_line_indentation(sci, line) != 0 &&
		 !check_define(sci, line))
		chars_right[1] = ';';

	style = sci_get_style_at(sci, pos);

	/* suppress double completion symbols */
	ch_next = char_at(sci, pos);
	if (ch == ch_next && !has_sel && ac_info->suppress_doubling &&
	  !(chars_left[0] != chars_right[0] && ch == chars_left[0]))
	{
		/* jump_on_data may be 2 (due to autoclosing ");"). Need to decrement if ")" is pressed */
		if (data->jump_on_tab > 0)
			data->jump_on_tab -= 1;
		if ((!ac_info->comments_ac_enable && !highlighting_is_code_style(lexer, style)) &&
		      ch != '"' && ch != '\'')
			return AC_CONTINUE_ACTION;

		/* suppress ; only at end of line */
		if (ch == ';' && pos + 1 != get_end_pos(sci, line))
			return AC_CONTINUE_ACTION;
		SSM(sci, SCI_DELETERANGE, pos, 1);
		return AC_CONTINUE_ACTION;
	}

	if (ch == ';')
		return AC_CONTINUE_ACTION;

	/* If we have selected text */
	if (has_sel && ac_info->enclose_selections)
		return enclose_selection(data, sci, ch, lexer, style, chars_left, chars_right, editor);

	/* disable autocompletion inside comments and strings */
	if (!ac_info->comments_ac_enable && !highlighting_is_code_style(lexer, style))
		return AC_CONTINUE_ACTION;

	if (ch == chars_right[0] && chars_left[0] != chars_right[0])
		return AC_CONTINUE_ACTION;

	/* add ; after struct */
	struct_semicolon(sci, pos, chars_right, filetype);

	/* just close char */
	SSM(sci, SCI_INSERTTEXT, pos, (sptr_t)chars_right);
	sci_set_current_position(sci, pos, TRUE);
	data->jump_on_tab += strlen(chars_right);
	data->last_caret = pos;
	data->last_line = sci_get_current_line(sci);
	return AC_CONTINUE_ACTION;
}

static gboolean
on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	AutocloseUserData *data = user_data;
	g_return_val_if_fail(data && DOC_VALID(data->doc), AC_CONTINUE_ACTION);
	return auto_close_chars(data, event);
}

static void
on_sci_notify(GObject *obj, gint scn, SCNotification *nt, gpointer user_data)
{
	AutocloseUserData *data = user_data;

	if (!ac_info->jump_on_tab)
		return;
	g_return_if_fail(data);
	g_return_if_fail(DOC_VALID(data->doc));

	ScintillaObject *sci = data->doc->editor->sci;
	/* reset jump_on_tab state when user clicked away */
	gboolean updated_sel  = nt->updated & SC_UPDATE_SELECTION;
	gboolean updated_text = nt->updated & SC_UPDATE_CONTENT;
	gint new_caret = sci_get_current_position(sci);
	gint new_line  = sci_get_current_line(sci);
	if (updated_sel && !updated_text)
	{
		gint delta = data->last_caret -  new_caret;
		gint delta_l = data->last_line - new_line;
		if (delta_l == 0 && data->jump_on_tab)
			data->jump_on_tab += delta;
		else
			data->jump_on_tab = 0;
	}
	data->last_caret = new_caret;
	data->last_line = new_line;
}

#define AC_GOBJECT_KEY "autoclose-userdata"

static void
on_document_open(GObject *obj, GeanyDocument *doc, gpointer user_data)
{
	AutocloseUserData *data;
	ScintillaObject   *sci;
	g_return_if_fail(DOC_VALID(doc));

	sci = doc->editor->sci;
	data = g_new0(AutocloseUserData, 1);
	data->doc = doc;
	plugin_signal_connect(geany_plugin, G_OBJECT(sci), "sci-notify",
		FALSE, G_CALLBACK(on_sci_notify), data);
	plugin_signal_connect(geany_plugin, G_OBJECT(sci), "key-press-event",
			FALSE, G_CALLBACK(on_key_press), data);
	/* save data pointer via GObject too for on_document_close() */
	g_object_set_data(G_OBJECT(sci), AC_GOBJECT_KEY, data);
}

static void
on_document_close(GObject *obj, GeanyDocument *doc, gpointer user_data)
{
	/* free the AutocloseUserData instance and disconnect the handler */
	ScintillaObject   *sci = doc->editor->sci;
	AutocloseUserData *data = g_object_steal_data(G_OBJECT(sci), AC_GOBJECT_KEY);
	/* no plugin_signal_disconnect() ?? */
	g_signal_handlers_disconnect_by_func(G_OBJECT(sci), G_CALLBACK(on_sci_notify), data);
	g_free(data);
}

PluginCallback plugin_callbacks[] =
{
	{ "document-open",  (GCallback) &on_document_open, FALSE, NULL },
	{ "document-new",   (GCallback) &on_document_open, FALSE, NULL },
	{ "document-close", (GCallback) &on_document_close,    FALSE, NULL },
	{ NULL, NULL, FALSE, NULL }
};

static void
configure_response_cb(GtkDialog *dialog, gint response, gpointer user_data)
{
	if (response != GTK_RESPONSE_OK && response != GTK_RESPONSE_APPLY)
		return;
	GKeyFile *config = g_key_file_new();
	gchar    *data;
	gchar    *config_dir = g_path_get_dirname(ac_info->config_file);

	g_key_file_load_from_file(config, ac_info->config_file, G_KEY_FILE_NONE, NULL);

#define SAVE_CONF_BOOL(name) do {                                              \
    ac_info->name = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(            \
    g_object_get_data(G_OBJECT(dialog), "check_" #name)));                     \
    g_key_file_set_boolean(config, "autoclose", #name, ac_info->name);         \
} while (0)

	SAVE_CONF_BOOL(parenthesis);
	SAVE_CONF_BOOL(abracket);
	SAVE_CONF_BOOL(abracket_htmlonly);
	SAVE_CONF_BOOL(cbracket);
	SAVE_CONF_BOOL(sbracket);
	SAVE_CONF_BOOL(dquote);
	SAVE_CONF_BOOL(squote);
	SAVE_CONF_BOOL(backquote);
	SAVE_CONF_BOOL(backquote_bashonly);
	SAVE_CONF_BOOL(comments_ac_enable);
	SAVE_CONF_BOOL(delete_pairing_brace);
	SAVE_CONF_BOOL(suppress_doubling);
	SAVE_CONF_BOOL(enclose_selections);
	SAVE_CONF_BOOL(comments_enclose);
	SAVE_CONF_BOOL(keep_selection);
	SAVE_CONF_BOOL(make_indent_for_cbracket);
	SAVE_CONF_BOOL(move_cursor_to_beginning);
	SAVE_CONF_BOOL(improved_cbracket_indent);
	SAVE_CONF_BOOL(close_functions);
	SAVE_CONF_BOOL(bcksp_remove_pair);
	SAVE_CONF_BOOL(jump_on_tab);

#undef SAVE_CONF_BOOL

	if (!g_file_test(config_dir, G_FILE_TEST_IS_DIR) && utils_mkdir(config_dir, TRUE) != 0)
	{
		dialogs_show_msgbox(GTK_MESSAGE_ERROR,
			_("Plugin configuration directory could not be created."));
	}
	else
	{
		/* write config to file */
		data = g_key_file_to_data(config, NULL, NULL);
		utils_write_file(ac_info->config_file, data);
		g_free(data);
	}
	g_free(config_dir);
	g_key_file_free(config);
}

/* Called by Geany to initialize the plugin */
void
plugin_init(G_GNUC_UNUSED GeanyData *data)
{
	int i;
	foreach_document(i)
	{
		on_document_open(NULL, documents[i], NULL);
	}
	GKeyFile *config = g_key_file_new();

	ac_info = g_new0(AutocloseInfo, 1);

	ac_info->config_file = g_strconcat(geany->app->configdir, G_DIR_SEPARATOR_S,
		"plugins", G_DIR_SEPARATOR_S, "autoclose", G_DIR_SEPARATOR_S, "autoclose.conf", NULL);

	g_key_file_load_from_file(config, ac_info->config_file, G_KEY_FILE_NONE, NULL);

#define GET_CONF_BOOL(name, def) ac_info->name = utils_get_setting_boolean(config, "autoclose", #name, def)

	GET_CONF_BOOL(parenthesis, TRUE);
	/* Angular bracket conflicts with conditional statements, enable only for HTML by default */
	GET_CONF_BOOL(abracket, TRUE);
	GET_CONF_BOOL(abracket_htmlonly, TRUE);
	GET_CONF_BOOL(cbracket, TRUE);
	GET_CONF_BOOL(sbracket, TRUE);
	GET_CONF_BOOL(dquote, TRUE);
	GET_CONF_BOOL(squote, TRUE);
	GET_CONF_BOOL(backquote, TRUE);
	GET_CONF_BOOL(backquote_bashonly, TRUE);
	GET_CONF_BOOL(comments_ac_enable, FALSE);
	GET_CONF_BOOL(delete_pairing_brace, TRUE);
	GET_CONF_BOOL(suppress_doubling, TRUE);
	GET_CONF_BOOL(enclose_selections, TRUE);
	GET_CONF_BOOL(comments_enclose, FALSE);
	GET_CONF_BOOL(keep_selection, TRUE);
	GET_CONF_BOOL(make_indent_for_cbracket, TRUE);
	GET_CONF_BOOL(move_cursor_to_beginning, TRUE);
	GET_CONF_BOOL(improved_cbracket_indent, TRUE);
	GET_CONF_BOOL(close_functions, TRUE);
	GET_CONF_BOOL(bcksp_remove_pair, FALSE);
	GET_CONF_BOOL(jump_on_tab, TRUE);

#undef GET_CONF_BOOL

	g_key_file_free(config);
}

#define GET_CHECKBOX_ACTIVE(name) gboolean sens = gtk_toggle_button_get_active(\
           GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(data), "check_" #name)))

#define SET_SENS(name) gtk_widget_set_sensitive(                               \
                        g_object_get_data(G_OBJECT(data), "check_" #name), sens)

static void
ac_make_indent_for_cbracket_cb(GtkToggleButton *togglebutton, gpointer data)
{
	GET_CHECKBOX_ACTIVE(make_indent_for_cbracket);
	SET_SENS(move_cursor_to_beginning);
}

static void
ac_parenthesis_cb(GtkToggleButton *togglebutton, gpointer data)
{
	GET_CHECKBOX_ACTIVE(parenthesis);
	SET_SENS(close_functions);
}

static void
ac_cbracket_cb(GtkToggleButton *togglebutton, gpointer data)
{
	GET_CHECKBOX_ACTIVE(cbracket);
	SET_SENS(make_indent_for_cbracket);
	SET_SENS(move_cursor_to_beginning);
}

static void
ac_abracket_htmlonly_cb(GtkToggleButton *togglebutton, gpointer data)
{
	GET_CHECKBOX_ACTIVE(abracket);
	SET_SENS(abracket_htmlonly);
}

static void
ac_backquote_bashonly_cb(GtkToggleButton *togglebutton, gpointer data)
{
	GET_CHECKBOX_ACTIVE(backquote);
	SET_SENS(backquote_bashonly);
}

static void
ac_enclose_selections_cb(GtkToggleButton *togglebutton, gpointer data)
{
	GET_CHECKBOX_ACTIVE(enclose_selections);
	SET_SENS(keep_selection);
	SET_SENS(comments_enclose);
}

static void
ac_delete_pairing_brace_cb(GtkToggleButton *togglebutton, gpointer data)
{
	GET_CHECKBOX_ACTIVE(delete_pairing_brace);
	SET_SENS(bcksp_remove_pair);
}

GtkWidget *
plugin_configure(GtkDialog *dialog)
{
	GtkWidget *widget, *vbox, *frame, *container, *scrollbox;
	vbox = gtk_vbox_new(FALSE, 0);
	scrollbox = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrollbox), vbox);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollbox),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

#define WIDGET_FRAME(description) do {                                         \
    container = gtk_vbox_new(FALSE, 0);                                        \
    frame = gtk_frame_new(NULL);                                               \
    gtk_frame_set_label(GTK_FRAME(frame), _(description));                     \
    gtk_container_add(GTK_CONTAINER(frame), container);                        \
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 3);                 \
} while (0)

#define WIDGET_CONF_BOOL(name, description, tooltip) do {                      \
    widget = gtk_check_button_new_with_label(_(description));                  \
    if (tooltip) gtk_widget_set_tooltip_text(widget, _(tooltip));              \
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), ac_info->name);    \
    gtk_box_pack_start(GTK_BOX(container), widget, FALSE, FALSE, 3);           \
    g_object_set_data(G_OBJECT(dialog), "check_" #name, widget);               \
} while (0)

	WIDGET_FRAME("Auto-close quotes and brackets");
	WIDGET_CONF_BOOL(parenthesis, "Parenthesis ( )",
		"Auto-close parenthesis \"(\" -> \"(|)\"");
	g_signal_connect(widget, "toggled", G_CALLBACK(ac_parenthesis_cb), dialog);
	WIDGET_CONF_BOOL(cbracket, "Curly brackets { }",
		"Auto-close curly brackets \"{\" -> \"{|}\"");
	g_signal_connect(widget, "toggled", G_CALLBACK(ac_cbracket_cb), dialog);
	WIDGET_CONF_BOOL(sbracket, "Square brackets [ ]",
		"Auto-close square brackets \"[\" -> \"[|]\"");
	WIDGET_CONF_BOOL(abracket, "Angular brackets < >",
		"Auto-close angular brackets \"<\" -> \"<|>\"");
	g_signal_connect(widget, "toggled", G_CALLBACK(ac_abracket_htmlonly_cb), dialog);
	WIDGET_CONF_BOOL(abracket_htmlonly, "\tOnly for HTML",
		"Auto-close angular brackets only in HTML documents");
	WIDGET_CONF_BOOL(dquote, "Double quotes \" \"",
		"Auto-close double quotes \" -> \"|\"");
	WIDGET_CONF_BOOL(squote, "Single quotes \' \'",
		"Auto-close single quotes ' -> '|'");
	WIDGET_CONF_BOOL(backquote, "Backquote ` `",
		"Auto-close backquote ` -> `|`");
	g_signal_connect(widget, "toggled", G_CALLBACK(ac_backquote_bashonly_cb), dialog);
	WIDGET_CONF_BOOL(backquote_bashonly, "\tOnly for Bash",
		"Auto-close backquote only in Bash");

	WIDGET_FRAME("Improve curly brackets completion");
	WIDGET_CONF_BOOL(make_indent_for_cbracket, "Indent when enclosing",
		"If you select some text and press \"{\" or \"}\", plugin "
		"will auto-close selected lines and make new block with indent."
		"\nYou do not need to select block precisely - block enclosing "
		"takes into account only lines.");
	g_signal_connect(widget, "toggled", G_CALLBACK(ac_make_indent_for_cbracket_cb), dialog);
	WIDGET_CONF_BOOL(move_cursor_to_beginning, "Move cursor to beginning",
		"If you checked \"Indent when enclosing\", moving cursor "
		"to beginning may be useful: usually you make new block "
		"and need to create new statement before this block.");
	WIDGET_CONF_BOOL(improved_cbracket_indent, "Improved auto-indentation",
		"Improved auto-indent for curly brackets: type \"{\" "
		"and then press Enter - plugin will create full indented block. "
		"Works without \"auto-close { }\" checkbox.");

	container = vbox;
	WIDGET_CONF_BOOL(delete_pairing_brace, "Delete pairing character while backspacing first",
		"Check if you want to delete pairing bracket by pressing BackSpace.");
	g_signal_connect(widget, "toggled", G_CALLBACK(ac_delete_pairing_brace_cb), dialog);
	WIDGET_CONF_BOOL(suppress_doubling, "Suppress double-completion",
		"Check if you want to allow editor automatically fix mistypes "
		"with brackets: if you type \"{}\" you will get \"{}\", not \"{}}\".");
	WIDGET_CONF_BOOL(enclose_selections, "Enclose selections",
		"Automatically enclose selected text by pressing just one bracket key.");
	g_signal_connect(widget, "toggled", G_CALLBACK(ac_enclose_selections_cb), dialog);
	WIDGET_CONF_BOOL(keep_selection, "Keep selection when enclosing",
		"Keep your previously selected text after enclosing.");

	WIDGET_FRAME("Behaviour inside comments and strings");
	WIDGET_CONF_BOOL(comments_ac_enable, "Allow auto-closing in strings and comments",
		"Check if you wan to keep auto-closing inside strings and comments too.");
	WIDGET_CONF_BOOL(comments_enclose, "Enclose selections in strings and comments",
		"Check if you wan to enclose selections inside strings and comments too.");

	container = vbox;
	WIDGET_CONF_BOOL(close_functions, "Auto-complete \";\" for functions",
		"Full function auto-closing (works only for C/C++): type \"sin(\" "
		"and you will get \"sin(|);\".");
	WIDGET_CONF_BOOL(bcksp_remove_pair, "Shift+BackSpace removes pairing brace too",
		"Remove left and right brace while pressing Shift+BackSpace.\nTip: "
		"to completely remove indented block just Shift+BackSpace first \"{\" "
		"or last \"}\".");
	WIDGET_CONF_BOOL(jump_on_tab, "Jump on Tab to enclosed char",
		"Jump behind autoclosed items on Tab press.");

#undef WIDGET_CONF_BOOL
#undef WIDGET_FRAME

	ac_make_indent_for_cbracket_cb(NULL, dialog);
	ac_cbracket_cb(NULL, dialog);
	ac_enclose_selections_cb(NULL, dialog);
	ac_parenthesis_cb(NULL, dialog);
	ac_abracket_htmlonly_cb(NULL, dialog);
	ac_delete_pairing_brace_cb(NULL, dialog);
	g_signal_connect(dialog, "response", G_CALLBACK(configure_response_cb), NULL);
	gtk_widget_show_all(scrollbox);
	return scrollbox;
}

/* Called by Geany before unloading the plugin. */
void
plugin_cleanup(void)
{
	g_free(ac_info->config_file);
	g_free(ac_info);
}

void
plugin_help(void)
{
	utils_open_browser("http://plugins.geany.org/autoclose.html");
}
