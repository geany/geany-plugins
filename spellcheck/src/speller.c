/*
 *      speller.c - this file is part of Spellcheck, a Geany plugin
 *
 *      Copyright 2008-2009 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2008-2009 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 *
 * $Id$
 */


#include "geanyplugin.h"

#include "scintilla/SciLexer.h"

#include <string.h>
#include <ctype.h>
#include <enchant.h>

#include "speller.h"
#include "scplugin.h"



static EnchantBroker *sc_speller_broker = NULL;
static EnchantDict *sc_speller_dict = NULL;



static void dict_describe(const gchar* const lang, const gchar* const name,
						  const gchar* const desc, const gchar* const file, void *target)
{
	gchar **result = (gchar**) target;
	*result = g_strdup_printf("\"%s\" (%s)", lang, name);
}


static gint sc_speller_check_word(GeanyDocument *doc, gint line_number, const gchar *word,
						   gint start_pos, gint end_pos)
{
	gsize n_suggs = 0;

	g_return_val_if_fail(sc_speller_dict != NULL, 0);
	g_return_val_if_fail(doc != NULL, 0);
	g_return_val_if_fail(word != NULL, 0);
	g_return_val_if_fail(start_pos >= 0 && end_pos >= 0, 0);

	if (! NZV(word))
		return 0;

	/* ignore numbers or words starting with digits */
	if (isdigit(*word))
		return 0;

	/* ignore non-text */
	if (! sc_speller_is_text(doc, start_pos))
		return 0;

	/* early out if the word is spelled correctly */
	if (enchant_dict_check(sc_speller_dict, word, -1) == 0)
		return 0;

	editor_indicator_set_on_range(doc->editor, GEANY_INDICATOR_ERROR, start_pos, end_pos);

	if (sc_info->use_msgwin && line_number != -1)
	{
		gsize j;
		gchar **suggs;
		GString *str;

		str = g_string_sized_new(256);
		suggs = enchant_dict_suggest(sc_speller_dict, word, -1, &n_suggs);
		if (suggs != NULL)
		{
			g_string_append_printf(str, "line %d: %s | ",  line_number + 1, word);

			g_string_append(str, _("Try: "));

			/* Now find the misspellings in the line, limit suggestions to a maximum of 15 (for now) */
			for (j = 0; j < MIN(n_suggs, 15); j++)
			{
				g_string_append(str, suggs[j]);
				g_string_append_c(str, ' ');
			}

			msgwin_msg_add(COLOR_RED, line_number + 1, doc, "%s", str->str);

			if (suggs != NULL && n_suggs > 0)
				enchant_dict_free_string_list(sc_speller_dict, suggs);
		}
		g_string_free(str, TRUE);
	}

	return n_suggs;
}


gint sc_speller_process_line(GeanyDocument *doc, gint line_number, const gchar *line)
{
	gint pos_start, pos_end;
	gint wstart, wend;
	GString *str;
	gint suggestions_found = 0;
	gchar c;

	g_return_val_if_fail(sc_speller_dict != NULL, 0);
	g_return_val_if_fail(doc != NULL, 0);
	g_return_val_if_fail(line != NULL, 0);

	str = g_string_sized_new(256);

	pos_start = sci_get_position_from_line(doc->editor->sci, line_number);
	pos_end = sci_get_position_from_line(doc->editor->sci, line_number + 1);

	while (pos_start < pos_end)
	{
		wstart = scintilla_send_message(doc->editor->sci, SCI_WORDSTARTPOSITION, pos_start, TRUE);
		wend = scintilla_send_message(doc->editor->sci, SCI_WORDENDPOSITION, wstart, FALSE);
		if (wstart == wend)
			break;
		c = sci_get_char_at(doc->editor->sci, wstart);
		/* hopefully it's enough to check for these both */
		if (ispunct(c) || isspace(c))
		{
			pos_start++;
			continue;
		}

		/* ensure the string has enough allocated memory */
		if (str->len < (guint)(wend - wstart))
			g_string_set_size(str, wend - wstart);

		sci_get_text_range(doc->editor->sci, wstart, wend, str->str);

		suggestions_found += sc_speller_check_word(doc, line_number, str->str, wstart, wend);

		pos_start = wend + 1;
	}

	g_string_free(str, TRUE);
	return suggestions_found;
}


void sc_speller_check_document(GeanyDocument *doc)
{
	gchar *line;
	gint i;
	gint first_line, last_line;
	gchar *dict_string = NULL;
	gint suggestions_found = 0;

	g_return_if_fail(sc_speller_dict != NULL);
	g_return_if_fail(doc != NULL);

	ui_progress_bar_start(_("Checking"));

	enchant_dict_describe(sc_speller_dict, dict_describe, &dict_string);

	if (sci_has_selection(doc->editor->sci))
	{
		first_line = sci_get_line_from_position(
			doc->editor->sci, sci_get_selection_start(doc->editor->sci));
		last_line = sci_get_line_from_position(
			doc->editor->sci, sci_get_selection_end(doc->editor->sci));

		if (sc_info->use_msgwin)
			msgwin_msg_add(COLOR_BLUE, -1, NULL,
				_("Checking file \"%s\" (lines %d to %d using %s):"),
				DOC_FILENAME(doc), first_line + 1, last_line + 1, dict_string);
		g_message("Checking file \"%s\" (lines %d to %d using %s):",
			DOC_FILENAME(doc), first_line + 1, last_line + 1, dict_string);
	}
	else
	{
		first_line = 0;
		last_line = sci_get_line_count(doc->editor->sci);
		if (sc_info->use_msgwin)
			msgwin_msg_add(COLOR_BLUE, -1, NULL, _("Checking file \"%s\" (using %s):"),
				DOC_FILENAME(doc), dict_string);
		g_message("Checking file \"%s\" (using %s):", DOC_FILENAME(doc), dict_string);
	}
	g_free(dict_string);

	for (i = first_line; i < last_line; i++)
	{
		line = sci_get_line(doc->editor->sci, i);

		suggestions_found += sc_speller_process_line(doc, i, line);

		/* process other GTK events to keep the GUI being responsive */
		while (g_main_context_iteration(NULL, FALSE));

		g_free(line);
	}
	if (suggestions_found == 0 && sc_info->use_msgwin)
		msgwin_msg_add(COLOR_BLUE, -1, NULL, _("The checked text is spelled correctly."));

	ui_progress_bar_stop();
}


static void broker_init_failed(void)
{
	const gchar *err = enchant_broker_get_error(sc_speller_broker);
	dialogs_show_msgbox(GTK_MESSAGE_ERROR,
		_("The Enchant library couldn't be initialized (%s)."),
		(err != NULL) ? err : _("unknown error (maybe the chosen language is not available)"));
}


static void dict_compare(gpointer data, gpointer user_data)
{
	gboolean *supported = user_data;

	if (utils_str_equal(sc_info->default_language, data))
		*supported = TRUE;
}


static gboolean check_default_lang(void)
{
	gboolean supported = FALSE;

	g_ptr_array_foreach(sc_info->dicts, dict_compare, &supported);

	return supported;
}


void sc_speller_reinit_enchant_dict(void)
{
	gchar *lang = sc_info->default_language;

	/* Release a previous dict object */
	if (sc_speller_dict != NULL)
		enchant_broker_free_dict(sc_speller_broker, sc_speller_dict);

	/* Check if the stored default dictionary is (still) avaiable, fall back to the first
	 * one in the list if not */
	if (! check_default_lang())
	{
		if (sc_info->dicts->len > 0)
		{
			lang = g_ptr_array_index(sc_info->dicts, 0);
			g_warning("Stored language ('%s') could not be loaded. Falling back to '%s'",
				sc_info->default_language, lang);
		}
		else
			g_warning("Stored language ('%s') could not be loaded.", sc_info->default_language);
	}

	/* Request new dict object */
	sc_speller_dict = enchant_broker_request_dict(sc_speller_broker, lang);
	if (sc_speller_dict == NULL)
	{
		broker_init_failed();
		gtk_widget_set_sensitive(sc_info->menu_item, FALSE);
	}
	else
	{
		gtk_widget_set_sensitive(sc_info->menu_item, TRUE);
	}
}


gchar *sc_speller_get_default_lang(void)
{
	const gchar *lang = g_getenv("LANG");
	gchar *result = NULL;

	if (NZV(lang))
	{
		if (*lang == 'C' || *lang == 'c')
			lang = "en";
		else
		{	/* if we have something like de_DE.UTF-8, strip everything from the period to the end */
			gchar *period = strchr(lang, '.');
			if (period != NULL)
				result = g_strndup(lang, g_utf8_pointer_to_offset(lang, period));
		}
	}
	else
		lang = "en";

	return (result != NULL) ? result : g_strdup(lang);
}


static void add_dict_array(const gchar* const lang_tag, const gchar* const provider_name,
						   const gchar* const provider_desc, const gchar* const provider_file,
						   gpointer user_data)
{
	guint i;
	gchar *result = g_strdup(lang_tag);

	/* sometimes dictionaries are named lang-LOCALE instead of lang_LOCALE, so replace the
	 * hyphen by a dash, enchant seems to not care about it. */
	for (i = 0; i < strlen(result); i++)
	{
		if (result[i] == '-')
			result[i] = '_';
	}

	/* find duplicates and skip them */
	for (i = 0; i < sc_info->dicts->len; i++)
	{
		if (utils_str_equal(g_ptr_array_index(sc_info->dicts, i), result))
			return;
	}

	g_ptr_array_add(sc_info->dicts, result);
}


static gint sort_dicts(gconstpointer a, gconstpointer b)
{	/* casting mania ;-) */
	return strcmp((gchar*)((GPtrArray*)a)->pdata, (gchar*)((GPtrArray*)b)->pdata);
}


static void create_dicts_array(void)
{
	sc_info->dicts = g_ptr_array_new();

	enchant_broker_list_dicts(sc_speller_broker, add_dict_array, sc_info->dicts);

	g_ptr_array_sort(sc_info->dicts, sort_dicts);
}


void sc_speller_dict_free_string_list(gchar **tmp_suggs)
{
	g_return_if_fail(sc_speller_dict != NULL);

	enchant_dict_free_string_list(sc_speller_dict, tmp_suggs);
}


void sc_speller_add_word(const gchar *word)
{
	g_return_if_fail(sc_speller_dict != NULL);
	g_return_if_fail(word != NULL);

	enchant_dict_add_to_pwl(sc_speller_dict, word, -1);
}

gboolean sc_speller_dict_check(const gchar *word)
{
	g_return_val_if_fail(sc_speller_dict != NULL, FALSE);
	g_return_val_if_fail(word != NULL, FALSE);

	return enchant_dict_check(sc_speller_dict, word, -1);
}


gchar **sc_speller_dict_suggest(const gchar *word, gsize *n_suggs)
{
	g_return_val_if_fail(sc_speller_dict != NULL, NULL);
	g_return_val_if_fail(word != NULL, NULL);

	return enchant_dict_suggest(sc_speller_dict, word, -1, n_suggs);
}


void sc_speller_add_word_to_session(const gchar *word)
{
	g_return_if_fail(sc_speller_dict != NULL);
	g_return_if_fail(word != NULL);

	enchant_dict_add_to_session(sc_speller_dict, word, -1);
}


void sc_speller_store_replacement(const gchar *old_word, const gchar *new_word)
{
	g_return_if_fail(sc_speller_dict != NULL);
	g_return_if_fail(old_word != NULL);
	g_return_if_fail(new_word != NULL);

	enchant_dict_store_replacement(sc_speller_dict, old_word, -1, new_word, -1);
}


void sc_speller_init(void)
{
	sc_speller_broker = enchant_broker_init();

	create_dicts_array();

	sc_speller_reinit_enchant_dict();
}


void sc_speller_free(void)
{
	if (sc_speller_dict != NULL)
		enchant_broker_free_dict(sc_speller_broker, sc_speller_dict);
	enchant_broker_free(sc_speller_broker);
}


gboolean sc_speller_is_text(GeanyDocument *doc, gint pos)
{
	gint lexer, style;

	g_return_val_if_fail(doc != NULL, FALSE);
	g_return_val_if_fail(pos >= 0, FALSE);

	style = sci_get_style_at(doc->editor->sci, pos);
	/* early out for the default style */
	if (style == STYLE_DEFAULT)
		return TRUE;

	lexer = scintilla_send_message(doc->editor->sci, SCI_GETLEXER, 0, 0);
	switch (lexer)
	{
		case SCLEX_ADA:
		{
			switch (style)
			{
				case SCE_ADA_DEFAULT:
				case SCE_ADA_COMMENTLINE:
				case SCE_ADA_STRING:
				case SCE_ADA_STRINGEOL:
				case SCE_ADA_CHARACTER:
				case SCE_ADA_CHARACTEREOL:
					return TRUE;
				default:
					return FALSE;
			}
			break;
		}
		case SCLEX_ASM:
		{
			switch (style)
			{
				case SCE_ASM_DEFAULT:
				case SCE_ASM_COMMENT:
				case SCE_ASM_COMMENTBLOCK:
				case SCE_ASM_STRING:
				case SCE_ASM_STRINGEOL:
				case SCE_ASM_CHARACTER:
					return TRUE;
				default:
					return FALSE;
			}
			break;
		}
		case SCLEX_BASH:
		{
			switch (style)
			{
				case SCE_SH_DEFAULT:
				case SCE_SH_COMMENTLINE:
				case SCE_SH_STRING:
				case SCE_SH_CHARACTER:
					return TRUE;
				default:
					return FALSE;
			}
			break;
		}
		case SCLEX_CAML:
		{
			switch (style)
			{
				case SCE_CAML_DEFAULT:
				case SCE_CAML_COMMENT:
				case SCE_CAML_COMMENT1:
				case SCE_CAML_COMMENT2:
				case SCE_CAML_COMMENT3:
				case SCE_CAML_STRING:
				case SCE_CAML_CHAR:
					return TRUE;
				default:
					return FALSE;
			}
			break;
		}
#ifdef SCE_PAS_DEFAULT
		case SCLEX_PASCAL:
		{
			switch (style)
			{
				case SCE_PAS_DEFAULT:
				case SCE_PAS_COMMENT:
				case SCE_PAS_COMMENT2:
				case SCE_PAS_COMMENTLINE:
				case SCE_PAS_STRING:
				case SCE_PAS_CHARACTER:
					return TRUE;
				default:
					return FALSE;
			}
			break;
		}

#endif
		case SCLEX_CPP:
#ifndef SCE_PAS_DEFAULT
		case SCLEX_PASCAL:
#endif
		{
			switch (style)
			{
				case SCE_C_DEFAULT:
				case SCE_C_COMMENT:
				case SCE_C_COMMENTLINE:
				case SCE_C_COMMENTDOC:
				case SCE_C_STRING:
				case SCE_C_CHARACTER:
				case SCE_C_STRINGEOL:
				case SCE_C_COMMENTLINEDOC:
					return TRUE;
				default:
					return FALSE;
			}
			break;
		}
		case SCLEX_CMAKE:
		{
			switch (style)
			{
				case SCE_CMAKE_DEFAULT:
				case SCE_CMAKE_COMMENT:
				case SCE_CMAKE_STRINGDQ:
				case SCE_CMAKE_STRINGLQ:
				case SCE_CMAKE_STRINGRQ:
				case SCE_CMAKE_STRINGVAR:
					return TRUE;
				default:
					return FALSE;
			}
			break;
		}
		case SCLEX_CSS:
		{
			switch (style)
			{
				case SCE_CSS_DEFAULT:
				case SCE_CSS_COMMENT:
					return TRUE;
				default:
					return FALSE;
			}
			break;
		}
		case SCLEX_D:
		{
			switch (style)
			{
				case SCE_D_DEFAULT:
				case SCE_D_COMMENT:
				case SCE_D_COMMENTLINE:
				case SCE_D_COMMENTDOC:
				case SCE_D_COMMENTNESTED:
				case SCE_D_STRING:
				case SCE_D_STRINGEOL:
				case SCE_D_CHARACTER:
				case SCE_D_COMMENTLINEDOC:
					return TRUE;
				default:
					return FALSE;
			}
			break;
		}
		case SCLEX_DIFF:
		{
			switch (style)
			{
				case SCE_DIFF_DEFAULT:
				case SCE_DIFF_COMMENT:
				case SCE_DIFF_HEADER:
					return TRUE;
				default:
					return FALSE;
			}
			break;
		}
		case SCLEX_FORTRAN:
		case SCLEX_F77:
		{
			switch (style)
			{
				case SCE_F_DEFAULT:
				case SCE_F_COMMENT:
				case SCE_F_STRING1:
				case SCE_F_STRING2:
				case SCE_F_STRINGEOL:
					return TRUE;
				default:
					return FALSE;
			}
			break;
		}
		case SCLEX_FREEBASIC:
		{
			switch (style)
			{
				case SCE_B_DEFAULT:
				case SCE_B_COMMENT:
				case SCE_B_STRING:
				case SCE_B_STRINGEOL:
				case SCE_B_CONSTANT:
					return TRUE;
				default:
					return FALSE;
			}
			break;
		}
		case SCLEX_HASKELL:
		{
			switch (style)
			{
				case SCE_HA_DEFAULT:
				case SCE_HA_COMMENTLINE:
				case SCE_HA_COMMENTBLOCK:
				case SCE_HA_COMMENTBLOCK2:
				case SCE_HA_COMMENTBLOCK3:
				case SCE_HA_STRING:
				case SCE_HA_CHARACTER:
				case SCE_HA_DATA:
					return TRUE;
				default:
					return FALSE;
			}
			break;
		}
		case SCLEX_HTML:
		case SCLEX_XML:
		{
			switch (style)
			{
				case SCE_H_DEFAULT:
				case SCE_H_TAGUNKNOWN:
				case SCE_H_ATTRIBUTEUNKNOWN:
				case SCE_H_DOUBLESTRING:
				case SCE_H_SINGLESTRING:
				case SCE_H_COMMENT:
				case SCE_H_CDATA:
				case SCE_H_VALUE: /* really? */
				case SCE_H_SGML_DEFAULT:
				case SCE_H_SGML_COMMENT:
				case SCE_H_SGML_DOUBLESTRING:
				case SCE_H_SGML_SIMPLESTRING:
				case SCE_H_SGML_1ST_PARAM_COMMENT:
				case SCE_HJ_DEFAULT:
				case SCE_HJ_COMMENT:
				case SCE_HJ_COMMENTLINE:
				case SCE_HJ_COMMENTDOC:
				case SCE_HJ_DOUBLESTRING:
				case SCE_HJ_SINGLESTRING:
				case SCE_HJ_STRINGEOL:
				case SCE_HB_DEFAULT:
				case SCE_HB_COMMENTLINE:
				case SCE_HB_STRING:
				case SCE_HB_STRINGEOL:
				case SCE_HBA_DEFAULT:
				case SCE_HBA_COMMENTLINE:
				case SCE_HBA_STRING:
				case SCE_HBA_STRINGEOL:
				case SCE_HJA_DEFAULT:
				case SCE_HJA_COMMENT:
				case SCE_HJA_COMMENTLINE:
				case SCE_HJA_COMMENTDOC:
				case SCE_HJA_DOUBLESTRING:
				case SCE_HJA_SINGLESTRING:
				case SCE_HJA_STRINGEOL:
				case SCE_HP_DEFAULT:
				case SCE_HP_COMMENTLINE:
				case SCE_HP_STRING:
				case SCE_HP_CHARACTER:
				case SCE_HP_TRIPLE:
				case SCE_HP_TRIPLEDOUBLE:
				case SCE_HPA_DEFAULT:
				case SCE_HPA_COMMENTLINE:
				case SCE_HPA_STRING:
				case SCE_HPA_CHARACTER:
				case SCE_HPA_TRIPLE:
				case SCE_HPA_TRIPLEDOUBLE:
				case SCE_HPHP_DEFAULT:
				case SCE_HPHP_SIMPLESTRING:
				case SCE_HPHP_HSTRING:
				case SCE_HPHP_COMMENT:
				case SCE_HPHP_COMMENTLINE:
					return TRUE;
				default:
					return FALSE;
			}
			break;
		}
		case SCLEX_LATEX:
		{
			switch (style)
			{
				case SCE_L_DEFAULT:
				case SCE_L_COMMENT:
					return TRUE;
				default:
					return FALSE;
			}
			break;
		}
		case SCLEX_LUA:
		{
			switch (style)
			{
				case SCE_LUA_DEFAULT:
				case SCE_LUA_COMMENT:
				case SCE_LUA_COMMENTLINE:
				case SCE_LUA_COMMENTDOC:
				case SCE_LUA_STRING:
				case SCE_LUA_CHARACTER:
				case SCE_LUA_LITERALSTRING:
				case SCE_LUA_STRINGEOL:
					return TRUE;
				default:
					return FALSE;
			}
			break;
		}
		case SCLEX_MAKEFILE:
		{
			switch (style)
			{
				case SCE_MAKE_DEFAULT:
				case SCE_MAKE_COMMENT:
					return TRUE;
				default:
					return FALSE;
			}
			break;
		}
		case SCLEX_MATLAB:
		{
			switch (style)
			{
				case SCE_MATLAB_DEFAULT:
				case SCE_MATLAB_COMMENT:
				case SCE_MATLAB_STRING:
				case SCE_MATLAB_DOUBLEQUOTESTRING:
					return TRUE;
				default:
					return FALSE;
			}
			break;
		}
		case SCLEX_NSIS:
		{
			switch (style)
			{
				case SCE_NSIS_DEFAULT:
				case SCE_NSIS_COMMENT:
				case SCE_NSIS_STRINGDQ:
				case SCE_NSIS_STRINGLQ:
				case SCE_NSIS_STRINGRQ:
				case SCE_NSIS_STRINGVAR:
				case SCE_NSIS_COMMENTBOX:
					return TRUE;
				default:
					return FALSE;
			}
			break;
		}
		case SCLEX_PERL:
		{
			switch (style)
			{
				case SCE_PL_DEFAULT:
				case SCE_PL_COMMENTLINE:
				case SCE_PL_STRING:
				case SCE_PL_CHARACTER:
				case SCE_PL_POD:
				case SCE_PL_POD_VERB:
				case SCE_PL_LONGQUOTE:
				/* do we want SCE_PL_STRING_* too? */
					return TRUE;
				default:
					return FALSE;
			}
			break;
		}
		case SCLEX_PO:
		{
			switch (style)
			{
				case SCE_PO_DEFAULT:
				case SCE_PO_COMMENT:
				case SCE_PO_MSGID_TEXT:
				case SCE_PO_MSGSTR_TEXT:
				case SCE_PO_MSGCTXT_TEXT:
					return TRUE;
				default:
					return FALSE;
			}
			break;
		}
		case SCLEX_PROPERTIES:
		{
			switch (style)
			{
				case SCE_PROPS_DEFAULT:
				case SCE_PROPS_COMMENT:
					return TRUE;
				default:
					return FALSE;
			}
			break;
		}
		case SCLEX_PYTHON:
		{
			switch (style)
			{
				case SCE_P_DEFAULT:
				case SCE_P_COMMENTLINE:
				case SCE_P_STRING:
				case SCE_P_CHARACTER:
				case SCE_P_TRIPLE:
				case SCE_P_TRIPLEDOUBLE:
				case SCE_P_COMMENTBLOCK:
				case SCE_P_STRINGEOL:
					return TRUE;
				default:
					return FALSE;
			}
			break;
		}
		case SCLEX_R:
		{
			switch (style)
			{
				case SCE_R_DEFAULT:
				case SCE_R_COMMENT:
				case SCE_R_STRING:
				case SCE_R_STRING2:
					return TRUE;
				default:
					return FALSE;
			}
			break;
		}
		case SCLEX_RUBY:
		{
			switch (style)
			{
				case SCE_RB_DEFAULT:
				case SCE_RB_COMMENTLINE:
				case SCE_RB_STRING:
				case SCE_RB_CHARACTER:
				case SCE_RB_POD:
					return TRUE;
				default:
					return FALSE;
			}
			break;
		}
		case SCLEX_SQL:
		{
			switch (style)
			{
				case SCE_SQL_DEFAULT:
				case SCE_SQL_COMMENT:
				case SCE_SQL_COMMENTLINE:
				case SCE_SQL_COMMENTDOC:
				case SCE_SQL_STRING:
				case SCE_SQL_CHARACTER:
				case SCE_SQL_SQLPLUS_COMMENT:
					return TRUE;
				default:
					return FALSE;
			}
			break;
		}
		case SCLEX_TCL:
		{
			switch (style)
			{
				case SCE_TCL_DEFAULT:
				case SCE_TCL_COMMENT:
				case SCE_TCL_COMMENTLINE:
				case SCE_TCL_IN_QUOTE:
					return TRUE;
				default:
					return FALSE;
			}
			break;
		}
		case SCLEX_VHDL:
		{
			switch (style)
			{
				case SCE_VHDL_DEFAULT:
				case SCE_VHDL_COMMENT:
				case SCE_VHDL_COMMENTLINEBANG:
				case SCE_VHDL_STRING:
				case SCE_VHDL_STRINGEOL:
					return TRUE;
				default:
					return FALSE;
			}
			break;
		}
		case SCLEX_YAML:
		{
			switch (style)
			{
				case SCE_YAML_DEFAULT:
				case SCE_YAML_COMMENT:
				case SCE_YAML_TEXT:
					return TRUE;
				default:
					return FALSE;
			}
			break;
		}
	}
	/* if the current lexer was not handled, let's say the passed position contains
	 * valid text to not ignore more than we want */
	return TRUE;
}


