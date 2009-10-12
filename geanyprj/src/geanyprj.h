/*
 *  geanyprj - Alternative project support for geany light IDE.
 *
 *  Copyright 2008 Yura Siamashka <yurand2@gmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GEANYPRJ_H__
#define __GEANYPRJ_H__

#include "geanyplugin.h"

#ifdef __GNUC__
#  ifdef DEBUG
#    define debug(format, ...) printf((format), __VA_ARGS__)
#  else
#    define debug(...)
#  endif
#else
#  ifdef DEBUG
#    define debug printf
#  else
#    define debug
#  endif
#endif

#define MAX_NAME_LEN 50
#define PROJECT_TYPE 1

enum
{
	NEW_PROJECT_TYPE_ALL,
	NEW_PROJECT_TYPE_CPP,
	NEW_PROJECT_TYPE_C,
	NEW_PROJECT_TYPE_PYTHON,
	NEW_PROJECT_TYPE_NONE,
	NEW_PROJECT_TYPE_SIZE
};

struct GeanyPrj
{
	gchar *path;		///< path to disk file

	gchar *name;
	gchar *description;
	gchar *base_path;
	gchar *run_cmd;

	gboolean regenerate;
	gint type;

	GHashTable *tags;	///< project tags
};

extern GeanyFunctions *geany_functions;

extern const gchar *project_type_string[NEW_PROJECT_TYPE_SIZE];
extern void *project_type_filter[NEW_PROJECT_TYPE_SIZE];

// project.c
struct GeanyPrj *geany_project_new();
struct GeanyPrj *geany_project_load(const gchar * path);
void geany_project_free(struct GeanyPrj *prj);

void geany_project_regenerate_file_list(struct GeanyPrj *prj);

gboolean geany_project_add_file(struct GeanyPrj *prj, const gchar * path);
gboolean geany_project_remove_file(struct GeanyPrj *prj, const gchar * path);
void geany_project_save(struct GeanyPrj *prj);

void geany_project_set_path(struct GeanyPrj *prj, const gchar * path);
void geany_project_set_name(struct GeanyPrj *prj, const gchar * name);
void geany_project_set_type_int(struct GeanyPrj *prj, gint val);
void geany_project_set_type_string(struct GeanyPrj *prj, const gchar * val);
void geany_project_set_regenerate(struct GeanyPrj *prj, gboolean val);

void geany_project_set_description(struct GeanyPrj *prj, const gchar * description);
void geany_project_set_base_path(struct GeanyPrj *prj, const gchar * base_path);
void geany_project_set_run_cmd(struct GeanyPrj *prj, const gchar * run_cmd);

void geany_project_set_tags_from_list(struct GeanyPrj *prj, GSList * files);


// sidebar.c
void create_sidebar();
void destroy_sidebar();

void sidebar_refresh();


// xproject.c
void xproject_init();
void xproject_open(const gchar * path);
gboolean xproject_add_file(const gchar * path);
gboolean xproject_remove_file(const gchar * path);
void xproject_update_tag(const gchar * filename);
void xproject_cleanup();
void xproject_close(gboolean cache);

// menu.h
void tools_menu_init();
void tools_menu_uninit();

void on_new_project(GtkMenuItem * menuitem, gpointer user_data);
void on_preferences(GtkMenuItem * menuitem, gpointer user_data);
void on_delete_project(GtkMenuItem * menuitem, gpointer user_data);
void on_add_file(GtkMenuItem * menuitem, gpointer user_data);
void on_find_in_project(GtkMenuItem * menuitem, gpointer user_data);



// utils.c
gchar *find_file_path(const gchar * dir, const gchar * filename);
gchar *normpath(const gchar * filename);
gchar *get_full_path(const gchar * location, const gchar * path);
gchar *get_relative_path(const gchar * location, const gchar * path);

gint config_length(GKeyFile * config, const gchar * section, const gchar * name);
void save_config(GKeyFile * config, const gchar * path);
GSList *get_file_list(const gchar * path, guint * length, gboolean(*func) (const gchar *),
		      GError ** error);


extern struct GeanyPrj *g_current_project;

#endif
