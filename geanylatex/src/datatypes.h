/*
 *      datatypes.h
 *
 *      Copyright 2008-2010 Frank Lanitz <frank(at)frank(dot)uvena(dot)de>
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

#ifndef DATATYPES_H
#define DATATYPES_H

typedef struct
{
	gint cat;
	const gchar *label;
	const gchar *latex;
} SubMenuTemplate;


typedef struct
{
	gint cat;
	gchar *label;
	gboolean sorted;
} CategoryName;


typedef struct
{
	gchar *filepath;
	gchar *label;
	GString *template;
} TemplateEntry;


typedef struct
{
	GtkWidget *documentclass_combobox;
	GtkWidget *encoding_combobox;
	GtkWidget *fontsize_combobox;
	GtkWidget *checkbox_KOMA;
	GtkWidget *author_textbox;
	GtkWidget *date_textbox;
	GtkWidget *title_textbox;
	GtkWidget *papersize_combobox;
	GtkWidget *checkbox_draft;
	GtkWidget *template_combobox;
	GtkWidget *orientation_combobox;
	GPtrArray *template_list;
	gboolean draft_active;
} LaTeXWizard;

typedef struct
{
	gchar *latex;
	gchar *label;
} BibTeXType;

typedef struct
{
	gchar *label_name;
	gint page;
	gchar *chapter;
} LaTeXLabel;

#endif
