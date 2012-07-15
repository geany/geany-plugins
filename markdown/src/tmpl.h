/*
 * tmpl.h - Part of the Geany Markdown plugin
 *
 * Copyright 2012 Matthew Brush <mbrush@codebrainz.ca>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#ifndef MARKDOWN_TEMPLATE_H
#define MARKDOWN_TEMPLATE_H 1

G_BEGIN_DECLS

#include <glib.h>

#define MARKDOWN_TEMPLATE_STRING "<!-- @markdown_document@ -->"

typedef struct MarkdownTemplate MarkdownTemplate;

MarkdownTemplate *markdown_template_new(const gchar *tmpl_name,
  const gchar *tmpl_file);

void markdown_template_free(MarkdownTemplate *tmpl);

const gchar *markdown_template_get_name(MarkdownTemplate *tmpl);
const gchar *markdown_template_get_filename(MarkdownTemplate *tmpl);
const gchar *markdown_template_get_base_dir(MarkdownTemplate *tmpl);
const gchar *markdown_template_get_text(MarkdownTemplate *tmpl, gsize *len);

void markdown_template_set_name(MarkdownTemplate *tmpl, const gchar *name);
void markdown_template_set_filename(MarkdownTemplate *tmpl, const gchar *filename);

gchar *markdown_template_replace(MarkdownTemplate *tmpl, const gchar *replacement,
  gsize *replaced_len);

G_END_DECLS

#endif /* MARKDOWN_TEMPLATE_H */
