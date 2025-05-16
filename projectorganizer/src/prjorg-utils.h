/*
 * Copyright 2010 Jiri Techet <techet@gmail.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __PRJORG_UTILS_H__
#define __PRJORG_UTILS_H__

#include <gtk/gtk.h>
#include <geanyplugin.h>

#if ! GLIB_CHECK_VERSION(2, 70, 0)
# define g_pattern_spec_match_string g_pattern_match_string
#endif

gchar *get_relative_path(const gchar *utf8_parent, const gchar *utf8_descendant);

gboolean patterns_match(GSList *patterns, const gchar *str);
GSList *get_precompiled_patterns(gchar **patterns);

void open_file(gchar *utf8_name);
void close_file(gchar *utf8_name);

gboolean create_file(char *utf8_name);
gboolean create_dir(char *utf8_name);
gboolean remove_file_or_dir(char *utf8_name);
gboolean rename_file_or_dir(gchar *utf8_oldname, gchar *utf8_newname);

gchar *get_selection(void);
gchar *get_project_base_path(void);

GtkWidget *menu_item_new(const gchar *icon_name, const gchar *label);

gchar *try_find_header_source(gchar *utf8_file_name, gboolean is_header, GSList *file_list, GSList *header_patterns, GSList *source_patterns);
gchar *find_header_source(GeanyDocument *doc);
void set_header_filetype(GeanyDocument * doc);

gchar *get_open_cmd(gboolean perform_substitution, const gchar *dirname);
gchar *get_terminal_cmd(gboolean perform_substitution, const gchar *dirname);
void set_commands(const gchar *open_cmd, const gchar *term_cmd);

#endif
