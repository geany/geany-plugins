/*
 * Copyright 2017 LarsGit223
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

#ifndef __UTILS_H__
#define __UTILS_H__

#include <gtk/gtk.h>

gchar *get_relative_path(const gchar *utf8_parent, const gchar *utf8_descendant);
gboolean patterns_match(GSList *patterns, const gchar *str);
GSList *get_precompiled_patterns(gchar **patterns);
gchar *get_combined_path(const gchar *base, const gchar *relative);
gchar *get_any_relative_path (const gchar *base, const gchar *target);
void open_all_files_in_list(GPtrArray *list);
void close_all_files_in_list(GPtrArray *list);
gboolean is_git_repository(gchar *path);

#endif
