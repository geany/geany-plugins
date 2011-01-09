/*
 *      speller.h - this file is part of Spellcheck, a Geany plugin
 *
 *      Copyright 2008-2011 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2008-2010 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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


#ifndef SC_SPELLER_H
#define SC_SPELLER_H 1


gint sc_speller_process_line(GeanyDocument *doc, gint line_number, const gchar *line);

void sc_speller_check_document(GeanyDocument *doc);

void sc_speller_reinit_enchant_dict(void);

gchar *sc_speller_get_default_lang(void);

void sc_speller_dict_free_string_list(gchar **tmp_suggs);

void sc_speller_add_word(const gchar *word);

gboolean sc_speller_dict_check(const gchar *word);

gchar **sc_speller_dict_suggest(const gchar *word, gsize *n_suggs);

gboolean sc_speller_is_text(GeanyDocument *doc, gint pos);

void sc_speller_add_word_to_session(const gchar *word);

void sc_speller_store_replacement(const gchar *old_word, const gchar *new_word);

void sc_speller_init(void);

void sc_speller_free(void);

#endif
