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

#ifndef __GPROJECT_PROJECT_H__
#define __GPROJECT_PROJECT_H__


typedef struct
{
	TMSourceFile *tag;
} TagObject;


typedef struct
{
	gchar **source_patterns;
	gchar **header_patterns;
	gchar **ignored_dirs_patterns;
	gboolean generate_tags;

	GHashTable *file_tag_table;
} GPrj;

extern GPrj *g_prj;

void gprj_project_open(GKeyFile * key_file);

gint gprj_project_add_properties_tab(GtkWidget *notebook);

void gprj_project_close(void);

void gprj_project_save(GKeyFile * key_file);
void gprj_project_read_properties_tab(void);
void gprj_project_rescan(void);


void gprj_project_add_file_tag(gchar *filename);
void gprj_project_remove_file_tag(gchar *filename);

gboolean gprj_project_is_in_project(const gchar * filename);


#endif
