/*
 *      latexkeybindings.c
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

#include "latexkeybindings.h"

void glatex_kblabel_insert(G_GNUC_UNUSED guint key_id)
{
	g_return_if_fail(document_get_current != NULL);
	glatex_insert_label_activated(NULL, NULL);
}


void glatex_kbref_insert(G_GNUC_UNUSED guint key_id)
{
	g_return_if_fail(document_get_current != NULL);	
	glatex_insert_ref_activated(NULL, NULL);
}


void glatex_kbref_insert_environment(G_GNUC_UNUSED guint key_id)
{
	g_return_if_fail(document_get_current != NULL);
	glatex_insert_environment_dialog(NULL, NULL);
}


void glatex_kbwizard(G_GNUC_UNUSED guint key_id)
{
	glatex_wizard_activated(NULL, NULL);
}


void glatex_kb_insert_newline(G_GNUC_UNUSED guint key_id)
{
	g_return_if_fail(document_get_current != NULL);
	glatex_insert_string("\\\\\n", TRUE);
}


void glatex_kb_insert_newitem(G_GNUC_UNUSED guint key_id)
{
	g_return_if_fail(document_get_current != NULL);
	glatex_insert_string("\\item ", TRUE);
}


void glatex_kb_replace_special_chars(G_GNUC_UNUSED guint key_id)
{
	g_return_if_fail(document_get_current != NULL);
	glatex_replace_special_character();
}


void glatex_kb_format_bold(G_GNUC_UNUSED guint key_id)
{
	g_return_if_fail(document_get_current != NULL);
	glatex_insert_latex_format(NULL, GINT_TO_POINTER(LATEX_BOLD));
}


void glatex_kb_format_italic(G_GNUC_UNUSED guint key_id)
{
	g_return_if_fail(document_get_current != NULL);
	glatex_insert_latex_format(NULL, GINT_TO_POINTER(LATEX_ITALIC));
}


void glatex_kb_format_typewriter(G_GNUC_UNUSED guint key_id)
{
	g_return_if_fail(document_get_current != NULL);
	glatex_insert_latex_format(NULL, GINT_TO_POINTER(LATEX_TYPEWRITER));
}


void glatex_kb_format_centering(G_GNUC_UNUSED guint key_id)
{
	g_return_if_fail(document_get_current != NULL);
	glatex_insert_latex_format(NULL, GINT_TO_POINTER(LATEX_CENTER));
}


void glatex_kb_format_left(G_GNUC_UNUSED guint key_id)
{
	g_return_if_fail(document_get_current != NULL);
	glatex_insert_latex_format(NULL, GINT_TO_POINTER(LATEX_LEFT));
}


void glatex_kb_format_right(G_GNUC_UNUSED guint key_id)
{
	g_return_if_fail(document_get_current != NULL);
	glatex_insert_latex_format(NULL, GINT_TO_POINTER(LATEX_RIGHT));
}


void glatex_kb_insert_description_list(G_GNUC_UNUSED guint key_id)
{
	g_return_if_fail(document_get_current != NULL);
	glatex_insert_list_environment(GLATEX_LIST_DESCRIPTION);
}


void glatex_kb_insert_itemize_list(G_GNUC_UNUSED guint key_id)
{
	g_return_if_fail(document_get_current != NULL);
	glatex_insert_list_environment(GLATEX_LIST_ITEMIZE);
}


void glatex_kb_insert_enumerate_list(G_GNUC_UNUSED guint key_id)
{
	g_return_if_fail(document_get_current != NULL);
	glatex_insert_list_environment(GLATEX_LIST_ENUMERATE);
}


void glatex_kb_structure_lvlup(G_GNUC_UNUSED guint key_id)
{
	g_return_if_fail(document_get_current != NULL);
	glatex_structure_lvlup();
}


void glatex_kb_structure_lvldown(G_GNUC_UNUSED guint key_id)
{
	g_return_if_fail(document_get_current != NULL);
	glatex_structure_lvldown();
}


void glatex_kb_usepackage_dialog(G_GNUC_UNUSED guint key_id)
{
	g_return_if_fail(document_get_current != NULL);
	glatex_insert_usepackage_dialog(NULL, NULL);
}

void glatex_kb_insert_command_dialog(G_GNUC_UNUSED guint key_id)
{
	g_return_if_fail(document_get_current != NULL);
	glatex_insert_command_activated(NULL, NULL);
}

void glatex_kb_insert_bibtex_cite(G_GNUC_UNUSED guint key_id)
{
	g_return_if_fail(document_get_current != NULL);
	on_insert_bibtex_dialog_activate(NULL, NULL);
}
