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

gchar *get_relative_path(const gchar *utf8_parent, const gchar *utf8_descendant);

gboolean patterns_match(GSList *patterns, const gchar *str);
GSList *get_precompiled_patterns(gchar **patterns);

void open_file(gchar *utf8_name);
gchar *get_selection(void);
gchar *get_project_base_path(void);

#endif
