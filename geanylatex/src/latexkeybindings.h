/*
 *      latexkeybindings.h
 *
 *      Copyright 2009-2010 Frank Lanitz <frank(at)frank(dot)uvena(dot)de>
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
 */

#ifndef LATEXKEYBINDINGS_H
#define LATEXKEYBINDINGS_H

#include "geanylatex.h"

/* Enum for available keybindings */
enum
{
	KB_LATEX_WIZARD,
	KB_LATEX_INSERT_LABEL,
	KB_LATEX_INSERT_REF,
	KB_LATEX_INSERT_NEWLINE,
	KB_LATEX_TOGGLE_ACTIVE,
	KB_LATEX_ENVIRONMENT_INSERT,
	KB_LATEX_INSERT_NEWITEM,
	KB_LATEX_REPLACE_SPECIAL_CHARS,
	KB_LATEX_FORMAT_BOLD,
	KB_LATEX_FORMAT_ITALIC,
	KB_LATEX_FORMAT_TYPEWRITER,
	KB_LATEX_FORMAT_CENTER,
	KB_LATEX_FORMAT_LEFT,
	KB_LATEX_FORMAT_RIGHT,
	KB_LATEX_ENVIRONMENT_INSERT_DESCRIPTION,
	KB_LATEX_ENVIRONMENT_INSERT_ITEMIZE,
	KB_LATEX_ENVIRONMENT_INSERT_ENUMERATE,
	KB_LATEX_STRUCTURE_LVLUP,
	KB_LATEX_STRUCTURE_LVLDOWN,
	KB_LATEX_USEPACKAGE_DIALOG,
	KB_LATEX_INSERT_COMMAND,
	COUNT_KB
};


void glatex_kblabel_insert(G_GNUC_UNUSED guint key_id);
void glatex_kbref_insert(G_GNUC_UNUSED guint key_id);
void glatex_kbref_insert_environment(G_GNUC_UNUSED guint key_id);
void glatex_kbwizard(G_GNUC_UNUSED guint key_id);
void glatex_kb_insert_newline(G_GNUC_UNUSED guint key_id);
void glatex_kb_insert_newitem(G_GNUC_UNUSED guint key_id);
void glatex_kb_replace_special_chars(G_GNUC_UNUSED guint key_id);
void glatex_kb_format_bold(G_GNUC_UNUSED guint key_id);
void glatex_kb_format_italic(G_GNUC_UNUSED guint key_id);
void glatex_kb_format_typewriter(G_GNUC_UNUSED guint key_id);
void glatex_kb_format_centering(G_GNUC_UNUSED guint key_id);
void glatex_kb_format_left(G_GNUC_UNUSED guint key_id);
void glatex_kb_format_right(G_GNUC_UNUSED guint key_id);
void glatex_kb_insert_description_list(G_GNUC_UNUSED guint key_id);
void glatex_kb_insert_itemize_list(G_GNUC_UNUSED guint key_id);
void glatex_kb_insert_enumerate_list(G_GNUC_UNUSED guint key_id);
void glatex_kb_structure_lvlup(G_GNUC_UNUSED guint key_id);
void glatex_kb_structure_lvldown(G_GNUC_UNUSED guint key_id);
void glatex_kb_usepackage_dialog(G_GNUC_UNUSED guint key_id);
void glatex_kb_insert_command_dialog(G_GNUC_UNUSED guint key_id);

#endif
