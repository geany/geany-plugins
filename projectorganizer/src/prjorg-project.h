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

#ifndef __PRJORG_PROJECT_H__
#define __PRJORG_PROJECT_H__

typedef struct
{
	gchar *base_dir;
	GHashTable *file_table; /* contains all file names within base_dir, maps file_name->TMSourceFile */
} PrjOrgRoot;

typedef enum
{
	PrjOrgTagAuto,
	PrjOrgTagYes,
	PrjOrgTagNo,
} PrjOrgTagPrefs;

typedef struct
{
	gchar **source_patterns;
	gchar **header_patterns;
	gchar **ignored_dirs_patterns;
	gchar **ignored_file_patterns;
	PrjOrgTagPrefs generate_tag_prefs;
	
	GSList *roots;  /* list of PrjOrgRoot; the project root is always the first followed by external dirs roots */
} PrjOrg;

extern PrjOrg *prj_org;

void prjorg_project_open(GKeyFile * key_file);

gint prjorg_project_add_properties_tab(GtkWidget *notebook);

void prjorg_project_close(void);

void prjorg_project_save(GKeyFile * key_file);
void prjorg_project_read_properties_tab(void);
void prjorg_project_rescan(void);

void prjorg_project_add_external_dir(const gchar *utf8_dirname);
void prjorg_project_remove_external_dir(const gchar *utf8_dirname);

void prjorg_project_add_single_tm_file(gchar *utf8_filename);
void prjorg_project_remove_single_tm_file(gchar *utf8_filename);

gboolean prjorg_project_is_in_project(const gchar *utf8_filename);

#endif
