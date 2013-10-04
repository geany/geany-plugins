/*
 *      autoclose.c
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

#define AC_STOP_ACTION TRUE
#define AC_CONTINUE_ACTION FALSE
#define SSM(s, m, w, l) scintilla_send_message(s, m, w, l)

GeanyPlugin		*geany_plugin;
GeanyData		*geany_data;
GeanyFunctions		*geany_functions;

PLUGIN_VERSION_CHECK(216)
PLUGIN_SET_TRANSLATABLE_INFO(
	LOCALEDIR,
	GETTEXT_PACKAGE,
	_("Auto-close"),
	_("Auto-close braces and brackets with lot of features"),
	"0.1",
	"Pavel Roschin <rpg89@post.ru>")

typedef struct {
	/* close chars */
	gboolean parenthesis;
	gboolean abracket;
	gboolean cbracket;
	gboolean sbracket;
	gboolean dquote;
	gboolean squote;
	gboolean backquote;
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
	/* others */
	gchar *config_file;
} AutocloseInfo;

static AutocloseInfo *ac_info = NULL;

static gboolean
lexer_has_braces(ScintillaObject *sci)
{
	gint lexer = sci_get_lexer(sci);

	switch (lexer)
	{
		case SCLEX_CPP:
		case SCLEX_D:
		case SCLEX_HTML:	/* for PHP & JS */
		case SCLEX_PASCAL:	/* for multiline comments? */
		case SCLEX_PERL:
		case SCLEX_TCL:
		case SCLEX_CSS:		/* also useful for CSS */
			return TRUE;
		default:
			return FALSE;
	}
}

static gboolean
lexer_cpp_like(gint lexer, gint style)
{
	if(lexer == SCLEX_CPP && style == SCE_C_IDENTIFIER)
		return TRUE;
	return FALSE;
}

static gboolean
check_chars(gint ch, gchar *chars_left, gchar *chars_right)
{
	switch(ch)
	{
		case '(':
		case ')':
			if(!ac_info->parenthesis)
				return FALSE;
			*chars_left = '(';
			*chars_right = ')';
			break;
		case ';':
			if(!ac_info->close_functions)
				return FALSE;
			break;
		case '{':
		case '}':
			if(!ac_info->cbracket)
				return FALSE;
			*chars_left = '{';
			*chars_right = '}';
			break;
		case '[':
		case ']':
			if(!ac_info->sbracket)
				return FALSE;
			*chars_left = '[';
			*chars_right = ']';
			break;
		case '<':
		case '>':
			if(!ac_info->abracket)
				return FALSE;
			*chars_left = '<';
			*chars_right = '>';
			break;
		case '\'':
			if(!ac_info->squote)
				return FALSE;
			*chars_left = *chars_right = ch;
			break;
		case '"':
			if(!ac_info->dquote)
				return FALSE;
			*chars_left = *chars_right = ch;
			break;
		case '`':
			if(!ac_info->backquote)
				return FALSE;
			*chars_left = *chars_right = ch;
			break;
		default:
			return FALSE;
	}
	return TRUE;
}

static gboolean
improve_indent(ScintillaObject *sci, GeanyEditor *editor, gint pos)
{
	gint ch, ch_next;
	gint line;
	gint indent, indent_width;
	gint end_pos;
	if(!ac_info->improved_cbracket_indent)
		return AC_CONTINUE_ACTION;
	ch = sci_get_char_at(sci, pos - 1);
	if(ch != '{')
		return AC_CONTINUE_ACTION;
	/* if curly bracket completion is enabled - just make indents
	 * but ensure that second "}" exists. If disabled - make indent
	 * and complete second curly bracket */
	ch_next = sci_get_char_at(sci, pos);
	if(ac_info->cbracket && ch_next != '}')
		return AC_CONTINUE_ACTION;
	line = sci_get_line_from_position(sci, pos);
	indent = sci_get_line_indentation(sci, line);
	indent_width = editor_get_indent_prefs(editor)->width;
	sci_start_undo_action(sci);
	if(ac_info->cbracket)
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
auto_close_chars(GeanyDocument *doc, GdkEventKey *event)
{
	ScintillaObject *sci;
	GeanyEditor     *editor;
	gint             ch, ch_next, ch_buf;
	gchar            chars_left[2]  = {0, 0};
	gchar            chars_right[3] = {0, 0, 0};
	gint             i;
	gint             lexer, style;
	gint             start_line, start_pos, end_line;
	gint             start_indent, indent_width, current_indent;
	gint             selection_start, selection_end;
	gint             pos, text_end_pos, line_end_pos, pos_lex_offset;
	gboolean         has_sel;
	gboolean         in_comment;
	gint             file_type_id = 0;

	if(NULL == doc)
		return AC_CONTINUE_ACTION;
	editor = doc->editor;
	if(NULL == editor)
		return AC_CONTINUE_ACTION;
	sci = editor->sci;
	if(NULL == sci)
		return AC_CONTINUE_ACTION;
	if(doc->file_type)
		file_type_id = doc->file_type->id;

	pos = sci_get_current_position(sci);
	ch = event->keyval;

	if(ch == GDK_BackSpace)
	{
		if(!ac_info->delete_pairing_brace)
			return AC_CONTINUE_ACTION;
		ch = sci_get_char_at(sci, pos - 1);
		if(check_chars(ch, chars_left, chars_right))
		{
			if(chars_left [0] == ch &&
			   chars_right[0] == sci_get_char_at(sci, pos))
			{
				SSM(sci, SCI_DELETERANGE, pos - 1, 2);
				return AC_STOP_ACTION;
			}
			return AC_CONTINUE_ACTION;
		}
		else
			return AC_CONTINUE_ACTION;
	}
	else if(ch == GDK_Return)
	{
		return improve_indent(sci, editor, pos);
	}

	/* set up completion chars */
	if(!check_chars(ch, chars_left, chars_right))
		return AC_CONTINUE_ACTION;

	has_sel = sci_has_selection(sci);

	lexer = sci_get_lexer(sci);

	/* in C-like languages - complete functions with ; */

	pos_lex_offset = -1;
	ch_buf = sci_get_char_at(sci, pos + pos_lex_offset);
	while('\t' == ch_buf || ' ' == ch_buf)
	{
		--pos_lex_offset;
		ch_buf = sci_get_char_at(sci, pos + pos_lex_offset);
	}
	style = sci_get_style_at(sci, pos + pos_lex_offset);
	line_end_pos = sci_get_line_end_position(sci, sci_get_current_line(sci));

	if(lexer_cpp_like(lexer, style) &&
	   chars_left[0] == '(' &&
	  !has_sel && ac_info->close_functions &&
	   pos == line_end_pos)
	{
		chars_right[1] = ';';
	}

	style = sci_get_style_at(sci, pos);

	/* suppress double completion symbols */
	ch_next = sci_get_char_at(sci, pos);
	if(ch == ch_next && !has_sel && ac_info->suppress_doubling)
	{
		/*if(sci_get_char_at(sci, pos - 1) != '\\') maybe...*/
		/* puts("Suppress double completion"); */
		if((!highlighting_is_code_style(lexer, style)) && ch != '"' && ch != '\'')
			return AC_CONTINUE_ACTION;

		/* suppress ; only at end of line */
		if(ch == ';' && pos + 1 != line_end_pos)
			return AC_CONTINUE_ACTION;

		sci_set_current_position(sci, pos + 1, TRUE);
		return AC_STOP_ACTION;
	}

	if(ch == ';')
		return AC_CONTINUE_ACTION;

	/* If we have selected text */
	if(has_sel && ac_info->enclose_selections)
	{
		selection_start = sci_get_selection_start(sci);
		selection_end = sci_get_selection_end(sci);

		/* case if selection covers mixed style */
		if(highlighting_is_code_style(lexer, sci_get_style_at(sci, selection_start)) !=
		   highlighting_is_code_style(lexer, sci_get_style_at(sci, selection_end)))
			in_comment = FALSE;
		else
			in_comment = !highlighting_is_code_style(lexer, style);
		if(!ac_info->comments_enclose && in_comment)
			return AC_CONTINUE_ACTION;

		sci_start_undo_action(sci);
		/* Insert {} block - special case: make indents, move cursor to beginning */
		if((ch == '{' || ch == '}') && lexer_has_braces(sci) &&
		    ac_info->make_indent_for_cbracket && !in_comment)
		{
			start_line = sci_get_line_from_position(sci, selection_start);
			start_pos = SSM(sci, SCI_GETLINEINDENTPOSITION, (uptr_t)start_line, 0);
			SSM(sci, SCI_INSERTTEXT, start_pos, (sptr_t)"{\n");

			end_line = sci_get_line_from_position(sci, selection_end);
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
			if(ac_info->move_cursor_to_beginning)
				sci_set_current_position(sci, start_pos, TRUE);
		}
		else
		{
			SSM(sci, SCI_INSERTTEXT, selection_start, (sptr_t) chars_left);
			SSM(sci, SCI_INSERTTEXT, selection_end + 1, (sptr_t) chars_right);
			sci_set_current_position(sci, selection_end + 1, TRUE);
			if(ac_info->keep_selection)
			{
				sci_set_selection_start(sci, selection_start + 1);
				sci_set_selection_end(sci, selection_end + 1);
			}
		}
		sci_end_undo_action(sci);
		return AC_STOP_ACTION;
	}

	/* disable autocompletion inside comments and strings */
	if(!ac_info->comments_ac_enable && !highlighting_is_code_style(lexer, style))
		return AC_CONTINUE_ACTION;

	if(ch == '}' || ch == ']' || ch ==')')
		return AC_CONTINUE_ACTION;

	/* just close char */
	SSM(sci, SCI_INSERTTEXT, pos, (sptr_t)chars_right);
	sci_set_current_position(sci, pos, TRUE);
	return AC_CONTINUE_ACTION;
}

static gboolean
on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	GeanyDocument *doc = user_data;
	if(NULL == doc)
		return AC_CONTINUE_ACTION;
	return auto_close_chars(doc, event);
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
	plugin_signal_connect(geany_plugin, G_OBJECT(sci), "key-press-event",
			FALSE, G_CALLBACK(on_key_press), doc);
}

PluginCallback plugin_callbacks[] =
{
	{ "document-open", (GCallback) &on_document_activate, FALSE, NULL },
	{ "document-new", (GCallback) &on_document_activate, FALSE, NULL },
	{ NULL, NULL, FALSE, NULL }
};

static void
configure_response_cb(GtkDialog *dialog, gint response, gpointer user_data)
{
	if(response != GTK_RESPONSE_OK && response != GTK_RESPONSE_APPLY)
		return;
	GKeyFile *config = g_key_file_new();
	gchar *data;
	gchar *config_dir = g_path_get_dirname(ac_info->config_file);

	g_key_file_load_from_file(config, ac_info->config_file, G_KEY_FILE_NONE, NULL);

	/* just to improve readability */
	#define SAVE_CONF_BOOL(name) do {\
		ac_info->name = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dialog), "check_" #name)));\
		g_key_file_set_boolean(config, "autoclose", #name, ac_info->name);\
	} while(0)
	SAVE_CONF_BOOL(parenthesis);
	SAVE_CONF_BOOL(abracket);
	SAVE_CONF_BOOL(cbracket);
	SAVE_CONF_BOOL(sbracket);
	SAVE_CONF_BOOL(dquote);
	SAVE_CONF_BOOL(squote);
	SAVE_CONF_BOOL(backquote);
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
	#undef SAVE_CONF_BOOL

	if(!g_file_test(config_dir, G_FILE_TEST_IS_DIR) && utils_mkdir(config_dir, TRUE) != 0)
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
		on_document_activate(NULL, documents[i], NULL);
	}
	GKeyFile *config = g_key_file_new();

	ac_info = g_new0(AutocloseInfo, 1);

	ac_info->config_file = g_strconcat(geany->app->configdir, G_DIR_SEPARATOR_S,
		"plugins", G_DIR_SEPARATOR_S, "autoclose", G_DIR_SEPARATOR_S, "autoclose.conf", NULL);

	g_key_file_load_from_file(config, ac_info->config_file, G_KEY_FILE_NONE, NULL);

	/* just to improve readability */
	#define GET_CONF_BOOL(name, def) ac_info->name = utils_get_setting_boolean(config, "autoclose", #name, def)
	GET_CONF_BOOL(parenthesis, TRUE);
	/* Angular bracket conflicts with conditional statements */
	GET_CONF_BOOL(abracket, FALSE);
	GET_CONF_BOOL(cbracket, TRUE);
	GET_CONF_BOOL(sbracket, TRUE);
	GET_CONF_BOOL(dquote, TRUE);
	GET_CONF_BOOL(squote, TRUE);
	GET_CONF_BOOL(backquote, TRUE);
	GET_CONF_BOOL(comments_ac_enable, FALSE);
	GET_CONF_BOOL(delete_pairing_brace, TRUE);
	GET_CONF_BOOL(suppress_doubling, TRUE);
	GET_CONF_BOOL(enclose_selections, TRUE);
	GET_CONF_BOOL(comments_enclose, FALSE);
	GET_CONF_BOOL(keep_selection, TRUE);
	GET_CONF_BOOL(make_indent_for_cbracket, TRUE);
	GET_CONF_BOOL(move_cursor_to_beginning, TRUE);
	GET_CONF_BOOL(improved_cbracket_indent, TRUE);
	GET_CONF_BOOL(close_functions, FALSE);
	#undef GET_CONF_BOOL

	g_key_file_free(config);
}

/* for easy refactoring */
#define GET_CHECKBOX_ACTIVE(name)  gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(data), "check_" #name)))
#define SET_SENS(name)             gtk_widget_set_sensitive(g_object_get_data(G_OBJECT(data), "check_" #name), sens)

static void
ac_make_indent_for_cbracket_cb(GtkToggleButton *togglebutton, gpointer data)
{
	gboolean sens = GET_CHECKBOX_ACTIVE(make_indent_for_cbracket);
	SET_SENS(move_cursor_to_beginning);
}

static void
ac_parenthesis_cb(GtkToggleButton *togglebutton, gpointer data)
{
	gboolean sens = GET_CHECKBOX_ACTIVE(parenthesis);
	SET_SENS(close_functions);
}

static void
ac_cbracket_cb(GtkToggleButton *togglebutton, gpointer data)
{
	gboolean sens = GET_CHECKBOX_ACTIVE(cbracket);
	SET_SENS(make_indent_for_cbracket);
	SET_SENS(move_cursor_to_beginning);
}

static void
ac_enclose_selections_cb(GtkToggleButton *togglebutton, gpointer data)
{
	gboolean sens = GET_CHECKBOX_ACTIVE(enclose_selections);
	SET_SENS(keep_selection);
	SET_SENS(comments_enclose);
}

GtkWidget *
plugin_configure(GtkDialog *dialog)
{
	GtkWidget *widget, *vbox, *frame, *container;

	vbox = gtk_vbox_new(FALSE, 0);

	#define WIDGET_FRAME(description) do{\
		container = gtk_vbox_new(FALSE, 0);\
		frame = gtk_frame_new(NULL);\
		gtk_frame_set_label(GTK_FRAME(frame), _(description));\
		gtk_container_add(GTK_CONTAINER(frame), container);\
		gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 3);\
	} while(0)

	#define WIDGET_CONF_BOOL(name, description, tooltip) do { \
		widget = gtk_check_button_new_with_label(_(description)); \
		if(tooltip) gtk_widget_set_tooltip_text(widget, _(tooltip));\
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), ac_info->name); \
		gtk_box_pack_start(GTK_BOX(container), widget, FALSE, FALSE, 3); \
		g_object_set_data(G_OBJECT(dialog), "check_" #name, widget); \
	} while(0)

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
	WIDGET_CONF_BOOL(dquote, "Double quotes \" \"",
		"Auto-close double quotes \" -> \"|\"");
	WIDGET_CONF_BOOL(squote, "Single quotes \' \'",
		"Auto-close single quotes ' -> '|'");
	WIDGET_CONF_BOOL(backquote, "Backquote ` `",
		"Auto-close backquote ` -> `|`");

	WIDGET_FRAME("Improve curly brackets completion");
	WIDGET_CONF_BOOL(make_indent_for_cbracket, "Indent when enclosing",
		"If you select some text and press \"{\" or \"}\", plugin "
		"will auto-close selected lines and make new block with indent."
		"\nYou do not need to select block precisely - block enclosing "
		"takes into account only lines.");
	g_signal_connect(widget, "toggled", G_CALLBACK(ac_make_indent_for_cbracket_cb), dialog);
	WIDGET_CONF_BOOL(move_cursor_to_beginning, "Move cursor to beginning",
		"If you chacked \"Indent when enclosing\", moving cursor "
		"to beginning may be useful: usually you make new block "
		"and need to create new statement before this block.");
	WIDGET_CONF_BOOL(improved_cbracket_indent, "Improved auto-indentation",
		"Improved auto-indent for curly brackets: type \"{\" "
		"and then press Enter - plugin will create full indented block. "
		"Works without \"auto-close { }\" checkbox.");

	container = vbox;
	WIDGET_CONF_BOOL(delete_pairing_brace, "Delete pairing character while backspacing first",
		"Check if you want to delete pairing bracket by pressing BackSpace.");
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
	#undef WIDGET_CONF_BOOL

	ac_make_indent_for_cbracket_cb(NULL, dialog);
	ac_cbracket_cb(NULL, dialog);
	ac_enclose_selections_cb(NULL, dialog);
	ac_parenthesis_cb(NULL, dialog);
	g_signal_connect(dialog, "response", G_CALLBACK(configure_response_cb), NULL);
	gtk_widget_show_all(vbox);
	return vbox;
}

/* Called by Geany before unloading the plugin. */
void
plugin_cleanup(void)
{
	g_free(ac_info->config_file);
	g_free(ac_info);
}
