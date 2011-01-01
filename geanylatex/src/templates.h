 /*
 *      templates.h
 *
 *      Copyright 2009-2011 Frank Lanitz <frank(at)frank(dot)uvena(dot)de>
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

#ifndef GLATEXTEMPLATES_H
#define GLATEXTEMPLATES_H

#include "geanylatex.h"

/* Add an enum entry for every new build in templates which are not
 * the TEMPLATE_LATEX_BEAMER and TEMPLATE_LATEX_LETTER.
 * Anyway, you sould prefer to add new templates not by patching this
 * code but by providing a template as described in documentation.
 * However, whenever adding a new template here, please ensure
 * LATEX_WIZARD_TEMPLATE_END is the last entry as it will trigger
 * the custom templates.*/
enum {
	LATEX_WIZARD_TEMPLATE_DEFAULT = 0,
	LATEX_WIZARD_TEMPLATE_END
};


#define TEMPLATE_LATEX "\
\\documentclass[{CLASSOPTION}]{{DOCUMENTCLASS}}\n\
{GEOMETRY}\
{ENCODING}\
{TITLE}\
{AUTHOR}\
{DATE}\
\\begin{document}\n\
\n\
\\end{document}\n"

#define TEMPLATE_LATEX_LETTER "\
\\documentclass[{CLASSOPTION}]{{DOCUMENTCLASS}}\n\
{ENCODING}\
\\address{}\n\
{DATE}\
{TITLE}\
{AUTHOR}\
\\begin{document}\n\
\\begin{letter}{}\n\
\\opening{{OPENING}}\n\n\
\\closing{{CLOSING}}\n\
\\end{letter}\n\
\\end{document}\n"

#define TEMPLATE_LATEX_BEAMER "\
\\documentclass[]{{DOCUMENTCLASS}}\n\
\\usetheme{default}\n\
{ENCODING}\
{TITLE}\
{AUTHOR}\
{DATE}\
\\begin{document}\n\
\\frame{\\titlepage}\n\
\\begin{frame}\n\
\\end{frame}\n\
\\end{document}\n"

GString *glatex_get_template_from_file(gchar *filepath);
GPtrArray *glatex_init_custom_templates(void);
GList *glatex_get_template_list_from_config_dir(void);
void glatex_free_template_entry(TemplateEntry *template, gpointer *data);
void glatex_add_templates_to_combobox(GPtrArray *templates, GtkWidget *combobox);

#endif
