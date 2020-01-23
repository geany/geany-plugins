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

#ifndef GP_UTILS_FILELIST_H
#define GP_UTILS_FILELIST_H

#include <glib.h>

G_BEGIN_DECLS

typedef enum
{
	FILELIST_FLAG_ADD_DIRS = 1,
}FILELIST_FLAG;

GSList *filelist_get_precompiled_patterns(gchar **patterns);
gboolean filelist_patterns_match(GSList *patterns, const gchar *str);
GSList *gp_filelist_scan_directory(guint *files, guint *folders, const gchar *searchdir, gchar **file_patterns,
		gchar **ignored_dirs_patterns, gchar **ignored_file_patterns);
GSList *gp_filelist_scan_directory_full(guint *files, guint *folders, const gchar *searchdir, gchar **file_patterns,
		gchar **ignored_dirs_patterns, gchar **ignored_file_patterns, guint flags);
gboolean gp_filelist_filepath_matches_patterns(const gchar *filepath, gchar **file_patterns,
		gchar **ignored_dirs_patterns, gchar **ignored_file_patterns);
GSList *gp_filelist_scan_directory_callback(const gchar *searchdir,
	void (*callback)(const gchar *path, gboolean *add, gboolean *enter, void *userdata),
	void *userdata);

G_END_DECLS

#endif /* GP_UTILS_FILELIST_H */
