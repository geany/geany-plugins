/*
 *  geanyprj - Alternative project support for geany light IDE.
 *
 *  Copyright 2007 Frank Lanitz <frank(at)frank(dot)uvena(dot)de>
 *  Copyright 2007 Enrico Tröger <enrico.troeger@uvena.de>
 *  Copyright 2007 Nick Treleaven <nick.treleaven@btinternet.com>
 *  Copyright 2007,2008 Yura Siamashka <yurand2@gmail.com>
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

#include <string.h>
#include <sys/time.h>

#include "geanyprj.h"

const gchar *project_type_string[NEW_PROJECT_TYPE_SIZE] = {
	[NEW_PROJECT_TYPE_ALL]    = "All",
	[NEW_PROJECT_TYPE_CPP]    = "C/C++",
	[NEW_PROJECT_TYPE_C]      = "C",
	[NEW_PROJECT_TYPE_PYTHON] = "Python",
	[NEW_PROJECT_TYPE_NONE]   = "None"
};

static gboolean project_filter_c_cpp(const gchar *file)
{
	if (filetypes_detect_from_file(file)->id == GEANY_FILETYPES_C ||
	    filetypes_detect_from_file(file)->id == GEANY_FILETYPES_CPP)
		return TRUE;
	return FALSE;
}


static gboolean project_filter_c(const gchar *file)
{
	if (filetypes_detect_from_file(file)->id == GEANY_FILETYPES_C)
		return TRUE;
	return FALSE;
}


static gboolean project_filter_python(const gchar *file)
{
	if (filetypes_detect_from_file(file)->id == GEANY_FILETYPES_PYTHON)
		return TRUE;
	return FALSE;
}


static gboolean project_filter_all(const gchar *file)
{
	if (filetypes_detect_from_file(file)->id != GEANY_FILETYPES_NONE)
		return TRUE;
	return FALSE;
}


static gboolean project_filter_none(G_GNUC_UNUSED const gchar *file)
{
	return FALSE;
}


gboolean (*project_type_filter[NEW_PROJECT_TYPE_SIZE]) (const gchar *) = {
	[NEW_PROJECT_TYPE_ALL]    = project_filter_all,
	[NEW_PROJECT_TYPE_CPP]    = project_filter_c_cpp,
	[NEW_PROJECT_TYPE_C]      = project_filter_c,
	[NEW_PROJECT_TYPE_PYTHON] = project_filter_python,
	[NEW_PROJECT_TYPE_NONE]   = project_filter_none
};


static void free_tag_object(gpointer obj)
{
	if (obj != NULL)
	{
		tm_workspace_remove_source_file((TMSourceFile *) obj);
		tm_source_file_free((TMSourceFile *) obj);
	}
}


struct GeanyPrj *geany_project_new(void)
{
	struct GeanyPrj *ret;

	ret = (struct GeanyPrj *) g_new0(struct GeanyPrj, 1);
	ret->tags = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, free_tag_object);

	return ret;
}


struct GeanyPrj *geany_project_load(const gchar *path)
{
	struct GeanyPrj *ret;
	TMSourceFile *tm_obj = NULL;
	GKeyFile *config;
	gint i = 0;
	gchar *file;
	gchar *filename, *locale_filename;
	gchar *key;
	gchar *tmp;

	debug("%s path=%s\n", __FUNCTION__, path);

	if (!path)
		return NULL;

	config = g_key_file_new();
	if (!g_key_file_load_from_file(config, path, G_KEY_FILE_NONE, NULL))
	{
		g_key_file_free(config);
		return NULL;
	}


	ret = geany_project_new();
	geany_project_set_path(ret, path);

	tmp = utils_get_setting_string(config, "project", "name", GEANY_STRING_UNTITLED);
	geany_project_set_name(ret, tmp);
	g_free(tmp);

	tmp = utils_get_setting_string(config, "project", "description", "");
	geany_project_set_description(ret, tmp);
	g_free(tmp);

	tmp = utils_get_setting_string(config, "project", "base_path", "");
	geany_project_set_base_path(ret, tmp);
	g_free(tmp);

	tmp = utils_get_setting_string(config, "project", "run_cmd", "");
	geany_project_set_run_cmd(ret, tmp);
	g_free(tmp);

	geany_project_set_type_string(ret,
				      utils_get_setting_string(config, "project", "type",
							       project_type_string[0]));
	geany_project_set_regenerate(ret,
				     g_key_file_get_boolean(config, "project", "regenerate", NULL));

	if (ret->regenerate)
	{
		geany_project_regenerate_file_list(ret);
	}
	else
	{
		GPtrArray *to_add = g_ptr_array_new();

		/* Create tag files */
		key = g_strdup_printf("file%d", i);
		while ((file = g_key_file_get_string(config, "files", key, NULL)))
		{
			filename = get_full_path(path, file);

			locale_filename = utils_get_locale_from_utf8(filename);
			tm_obj = tm_source_file_new(locale_filename,
						    filetypes_detect_from_file(filename)->name);
			g_free(locale_filename);
			if (tm_obj)
			{
				g_hash_table_insert(ret->tags, filename, tm_obj);
				g_ptr_array_add(to_add, tm_obj);
			}
			else
				g_free(filename);
			i++;
			g_free(key);
			g_free(file);
			key = g_strdup_printf("file%d", i);
		}
		tm_workspace_add_source_files(to_add);
		g_ptr_array_free(to_add, TRUE);
		g_free(key);
	}
	g_key_file_free(config);
	return ret;
}


static void remove_all_tags(struct GeanyPrj *prj)
{
	GPtrArray *to_remove, *keys;
	GHashTableIter iter;
	gpointer key, value;
	
	/* instead of relatively slow removal of source files by one from the tag manager, 
	 * use the bulk operations - this requires that the normal destroy function
	 * of GHashTable is skipped - steal the keys and values and handle them manually */
	to_remove = g_ptr_array_new_with_free_func((GFreeFunc)tm_source_file_free);
	keys = g_ptr_array_new_with_free_func(g_free);
	g_hash_table_iter_init(&iter, prj->tags);
	while (g_hash_table_iter_next(&iter, &key, &value))
	{
		g_ptr_array_add(to_remove, value);
		g_ptr_array_add(keys, key);
	}
	tm_workspace_remove_source_files(to_remove);
	g_hash_table_steal_all(prj->tags);
	g_ptr_array_free(to_remove, TRUE);
	g_ptr_array_free(keys, TRUE);
}


void geany_project_regenerate_file_list(struct GeanyPrj *prj)
{
	GSList *lst;

	debug("%s path=%s\n", __FUNCTION__, prj->base_path);
	remove_all_tags(prj);

	lst = get_file_list(prj->base_path, NULL, project_type_filter[prj->type], NULL);
	geany_project_set_tags_from_list(prj, lst);

	g_slist_foreach(lst, (GFunc) g_free, NULL);
	g_slist_free(lst);
}


void geany_project_set_path(struct GeanyPrj *prj, const gchar *path)
{
	gchar *norm_path = normpath(path);
	if (prj->path)
	{
		if (strcmp(prj->path, norm_path) == 0)
		{
			g_free(norm_path);
			return;
		}
	}
	prj->path = norm_path;
}


void geany_project_set_name(struct GeanyPrj *prj, const gchar *name)
{
	if (prj->name)
		g_free(prj->name);
	prj->name = g_strdup(name);
}


void geany_project_set_type_int(struct GeanyPrj *prj, gint val)
{
	prj->type = val;
}


void geany_project_set_type_string(struct GeanyPrj *prj, const gchar *val)
{
	guint i;

	for (i = 0; i < sizeof(project_type_string) / sizeof(project_type_string[0]); i++)
	{
		if (strcmp(val, project_type_string[i]) == 0)
		{
			geany_project_set_type_int(prj, i);
			return;
		}
	}
}


void geany_project_set_regenerate(struct GeanyPrj *prj, gboolean val)
{
	prj->regenerate = val;
}


void geany_project_set_description(struct GeanyPrj *prj, const gchar *description)
{
	if (prj->description)
		g_free(prj->description);
	prj->description = g_strdup(description);
}


void geany_project_set_base_path(struct GeanyPrj *prj, const gchar *base_path)
{
	if (prj->base_path)
		g_free(prj->base_path);

	if (g_path_is_absolute(base_path))
	{
		prj->base_path = g_strdup(base_path);
	}
	else
	{
		prj->base_path = get_full_path(prj->path, base_path);
	}
}


void geany_project_set_run_cmd(struct GeanyPrj *prj, const gchar *run_cmd)
{
	if (prj->run_cmd)
		g_free(prj->run_cmd);
	prj->run_cmd = g_strdup(run_cmd);
}


/* list in utf8 */
void geany_project_set_tags_from_list(struct GeanyPrj *prj, GSList *files)
{
	GSList *tmp;
	gchar *locale_filename;
	TMSourceFile *tm_obj = NULL;
	GPtrArray *to_add = g_ptr_array_new();

	prj->tags = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, free_tag_object);

	for (tmp = files; tmp != NULL; tmp = g_slist_next(tmp))
	{
		locale_filename = utils_get_locale_from_utf8(tmp->data);
		tm_obj = tm_source_file_new(locale_filename,
					    filetypes_detect_from_file(tmp->data)->name);
		g_free(locale_filename);
		if (tm_obj)
		{
			g_hash_table_insert(prj->tags, g_strdup(tmp->data), tm_obj);
			g_ptr_array_add(to_add, tm_obj);
		}
	}
	tm_workspace_add_source_files(to_add);
	g_ptr_array_free(to_add, TRUE);
}


void geany_project_free(struct GeanyPrj *prj)
{
	debug("%s prj=%p\n", __FUNCTION__, prj);
	g_return_if_fail(prj);

	if (prj->path)
		g_free(prj->path);
	if (prj->name)
		g_free(prj->name);
	if (prj->description)
		g_free(prj->description);
	if (prj->base_path)
		g_free(prj->base_path);
	if (prj->run_cmd)
		g_free(prj->run_cmd);
	if (prj->tags)
	{
		remove_all_tags(prj);
		g_hash_table_destroy(prj->tags);
	}

	g_free(prj);
}


gboolean geany_project_add_file(struct GeanyPrj *prj, const gchar *path)
{
	gchar *filename;
	TMSourceFile *tm_obj = NULL;

	GKeyFile *config;

	config = g_key_file_new();
	if (!g_key_file_load_from_file(config, prj->path, G_KEY_FILE_NONE, NULL))
	{
		g_key_file_free(config);
		return FALSE;
	}

	if (g_hash_table_lookup(prj->tags, path))
	{
		g_key_file_free(config);
		return TRUE;
	}
	g_key_file_free(config);

	filename = utils_get_locale_from_utf8(path);
	tm_obj = tm_source_file_new(filename, filetypes_detect_from_file(path)->name);
	g_free(filename);
	if (tm_obj)
	{
		g_hash_table_insert(prj->tags, g_strdup(path), tm_obj);
		tm_workspace_add_source_file(tm_obj);
	}
	geany_project_save(prj);
	return TRUE;
}


struct CFGData
{
	struct GeanyPrj *prj;
	GKeyFile *config;
	gint i;
};


static void geany_project_save_files(gpointer key, G_GNUC_UNUSED gpointer value, gpointer user_data)
{
	gchar *fkey;
	gchar *filename;
	struct CFGData *data = (struct CFGData *) user_data;

	filename = get_relative_path(data->prj->path, (const gchar *) key);
	if (filename)
	{
		fkey = g_strdup_printf("file%d", data->i);
		g_key_file_set_string(data->config, "files", fkey, filename);
		data->i++;
		g_free(fkey);
		g_free(filename);
	}
}


gboolean geany_project_remove_file(struct GeanyPrj *prj, const gchar *path)
{
	if (!g_hash_table_remove(prj->tags, path))
	{
		return FALSE;
	}

	geany_project_save(prj);
	return TRUE;
}


void geany_project_save(struct GeanyPrj *prj)
{
	GKeyFile *config;
	struct CFGData data;
	gchar *base_path;

	base_path = get_relative_path(prj->path, prj->base_path);

	config = g_key_file_new();
	g_key_file_load_from_file(config, prj->path, G_KEY_FILE_NONE, NULL);

	g_key_file_set_string(config, "project", "name", prj->name);
	g_key_file_set_string(config, "project", "description", prj->description);
	g_key_file_set_string(config, "project", "base_path", base_path);
	g_key_file_set_string(config, "project", "run_cmd", prj->run_cmd);
	g_key_file_set_boolean(config, "project", "regenerate", prj->regenerate);
	g_key_file_set_string(config, "project", "type", project_type_string[prj->type]);

	data.prj = prj;
	data.config = config;
	data.i = 0;

	g_key_file_remove_group(config, "files", NULL);
	if (!prj->regenerate)
	{
		g_hash_table_foreach(prj->tags, geany_project_save_files, &data);
	}
	save_config(config, prj->path);
	g_free(base_path);
}
