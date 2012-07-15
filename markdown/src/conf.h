/*
 * conf.h - Part of the Geany Markdown plugin
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

#ifndef MARKDOWN_CONF_H
#define MARKDOWN_CONF_H 1

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
  MARKDOWN_VIEW_POS_SIDEBAR=0,
  MARKDOWN_VIEW_POS_MSGWIN=1
} MarkdownViewPosition;

typedef struct MarkdownConf MarkdownConf;

MarkdownConf *markdown_conf_new(const gchar *conf_filename);
void markdown_conf_free(MarkdownConf *conf);
void markdown_conf_save(MarkdownConf *conf);

gchar *markdown_conf_get_template_name(MarkdownConf *conf);
void markdown_conf_set_template_name(MarkdownConf *conf, const gchar *tmpl_name);

gboolean markdown_conf_get_readme_shown(MarkdownConf *conf);
void markdown_conf_set_readme_shown(MarkdownConf *conf, gboolean shown);

MarkdownViewPosition markdown_conf_get_view_position(MarkdownConf *conf);
void markdown_conf_set_view_position(MarkdownConf *conf, MarkdownViewPosition pos);


G_END_DECLS

#endif /* MARKDOWN_CONF_H */
