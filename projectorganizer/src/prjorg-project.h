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

#include <gtk/gtk.h>

#define PRJORG_PATTERNS_SOURCE "*.c *.C *.cpp *.cxx *.c++ *.cc *.m"
#define PRJORG_PATTERNS_HEADER "*.h *.H *.hpp *.hxx *.h++ *.hh"
#define PRJORG_PATTERNS_IGNORED_DIRS ".* CVS"
#define PRJORG_PATTERNS_IGNORED_FILE "*.o *.obj *.a *.lib *.so *.dll *.lo *.la *.class *.jar *.pyc *.mo *.gmo"

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
	gboolean show_empty_dirs;
	PrjOrgTagPrefs generate_tag_prefs;

	GSList *roots;  /* list of PrjOrgRoot; the project root is always the first followed by external dirs roots */
} PrjOrg;

extern PrjOrg *prj_org;

void prjorg_project_open(GKeyFile * key_file);
gchar **prjorg_project_load_expanded_paths(GKeyFile * key_file);

GtkWidget *prjorg_project_add_properties_tab(GtkWidget *notebook);

void prjorg_project_close(void);

void prjorg_project_save(GKeyFile * key_file);
void prjorg_project_read_properties_tab(void);
void prjorg_project_rescan(void);

void prjorg_project_add_external_dir(const gchar *utf8_dirname);
void prjorg_project_remove_external_dir(const gchar *utf8_dirname);

void prjorg_project_add_single_tm_file(gchar *utf8_filename);
void prjorg_project_remove_single_tm_file(gchar *utf8_filename);

gboolean prjorg_project_is_in_project(const gchar *utf8_filename);


/* set open command based on OS */
#if defined(_WIN32) || defined(G_OS_WIN32)
#define PRJORG_COMMAND_OPEN "start"
#define PRJORG_COMMAND_TERMINAL "PowerShell"
#define PRJORG_COMMAND_TERMINAL_ALT ""
#elif defined(__APPLE__)
#define PRJORG_COMMAND_OPEN "open"
#define PRJORG_COMMAND_TERMINAL "open -b com.apple.terminal"
#define PRJORG_COMMAND_TERMINAL_ALT ""
#else
#define PRJORG_COMMAND_OPEN "xdg-open"
#define PRJORG_COMMAND_TERMINAL "xterm"
#define PRJORG_COMMAND_TERMINAL_ALT "/usr/bin/x-terminal-emulator"
#endif

/* In the code we create a list of all files but we want to keep empty directories
 * in the list for which we create a fake file name with the PROJORG_DIR_ENTRY
 * value. */
#define PROJORG_DIR_ENTRY "..."

#endif
